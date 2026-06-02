#include "commands.h"
#include "fs/vfs/internal.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CP_BUFFER_SIZE 4096U

void cli_command_touch(cli_main_state_t* state,
                       const int argc,
                       char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: touch <path>\n");
    return;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_path,
    sizeof(absolute_path));

  const fs_status_t ensure_status =
    cli_utils_ensure_parent_directories(
      state->fs_state.fs_context.vfs_context,
      absolute_path);

  if (ensure_status != FS_STATUS_OK) {
    fprintf(stderr,
            "touch: cannot create parent directories for "
            "'%s' (status: %d)\n",
            absolute_path,
            ensure_status);
    return;
  }

  int32_t file_descriptor = -1;
  fs_status_t status =
    fs_vfs_open(state->fs_state.fs_context.vfs_context,
                absolute_path,
                FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                &file_descriptor);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "touch: cannot create '%s' (status: %d)\n",
            absolute_path,
            status);
    return;
  }

  (void)fs_vfs_close(state->fs_state.fs_context.vfs_context,
                     file_descriptor);
  printf("File created: %s\n", absolute_path);
}

void cli_command_rm(cli_main_state_t* state,
                    const int argc,
                    char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: rm <path>\n");
    return;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_path,
    sizeof(absolute_path));

  const fs_status_t status = fs_vfs_remove(
    state->fs_state.fs_context.vfs_context, absolute_path);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "rm: cannot remove '%s' (status: %d)\n",
            absolute_path,
            status);
    return;
  }

  printf("File removed: %s\n", absolute_path);
}

void cli_command_cat(cli_main_state_t* state,
                     const int argc,
                     char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: cat <path>\n");
    return;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_path,
    sizeof(absolute_path));

  int32_t file_descriptor = -1;
  fs_status_t status =
    fs_vfs_open(state->fs_state.fs_context.vfs_context,
                absolute_path,
                FS_OPEN_READ_ONLY,
                &file_descriptor);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "cat: cannot open '%s' (status: %d)\n",
            absolute_path,
            status);
    return;
  }

  uint8_t buffer[FS_CLUSTER_SIZE];
  size_t bytes_read = 0;

  while (true) {
    status =
      fs_vfs_read(state->fs_state.fs_context.vfs_context,
                  file_descriptor,
                  buffer,
                  sizeof(buffer),
                  &bytes_read);

    if (status != FS_STATUS_OK || bytes_read == 0) {
      break;
    }

    (void)fwrite(buffer, 1, bytes_read, stdout);
  }

  (void)fs_vfs_close(state->fs_state.fs_context.vfs_context,
                     file_descriptor);
  printf("\n");
}

void cli_command_info(cli_main_state_t* state,
                      const int argc,
                      char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: info <path>\n");
    return;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_path,
    sizeof(absolute_path));

  fs_inode_t inode_info = {0};
  const fs_status_t status =
    fs_vfs_get_info(state->fs_state.fs_context.vfs_context,
                    absolute_path,
                    &inode_info);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "info: cannot get info for '%s' (status: %d)\n",
            absolute_path,
            status);
    return;
  }

  const char* type_name = "Unknown";
  if (inode_info.type == FS_TYPE_REGULAR)
    type_name = "Regular File";
  else if (inode_info.type == FS_TYPE_DIRECTORY)
    type_name = "Directory";
  else if (inode_info.type == FS_TYPE_SYMLINK)
    type_name = "Symlink";

  printf("Info for %s:\n", absolute_path);
  printf("  Type:         %s\n", type_name);
  printf("  Inode ID:     %u\n", inode_info.id);
  printf("  Size:         %u bytes\n", inode_info.size);
  printf("  Clusters:     %u\n", inode_info.cluster_count);
  printf("  Link Count:   %u\n", inode_info.link_count);
  printf("  Used:         %s\n",
         inode_info.is_used ? "Yes" : "No");
}

typedef struct copy_statistics {
  uint32_t files_copied;
  uint32_t directories_copied;
  uint32_t symlinks_copied;
  uint64_t bytes_copied;
} copy_statistics_t;

typedef struct copy_callback_context {
  fs_vfs_context_t* vfs_context;
  const char* source_directory_path;
  const char* destination_directory_path;
  copy_statistics_t* statistics;
  fs_status_t error_status;
  bool has_error;
} copy_callback_context_t;

static void
extract_path_basename(const char* path,
                      char* output_buffer,
                      const size_t buffer_size) {
  if (path == nullptr || output_buffer == nullptr
      || buffer_size == 0U) {
    return;
  }

  const char* last_slash = strrchr(path, '/');
  const char* name_start =
    (last_slash != nullptr) ? (last_slash + 1) : path;
  strncpy(output_buffer, name_start, buffer_size - 1U);
  output_buffer[buffer_size - 1] = '\0';
}

static fs_status_t
copy_single_file(fs_vfs_context_t* vfs_context,
                 const char* source_path,
                 const char* destination_path,
                 copy_statistics_t* statistics) {
  const fs_status_t ensure_status =
    cli_utils_ensure_parent_directories(vfs_context,
                                        destination_path);

  if (ensure_status != FS_STATUS_OK) {
    return ensure_status;
  }

  int32_t source_descriptor = -1;
  fs_status_t status = fs_vfs_open(vfs_context,
                                   source_path,
                                   FS_OPEN_READ_ONLY,
                                   &source_descriptor);

  if (status != FS_STATUS_OK) {
    return status;
  }

  int32_t destination_descriptor = -1;
  status = fs_vfs_open(
    vfs_context,
    destination_path,
    FS_OPEN_CREATE | FS_OPEN_WRITE_ONLY | FS_OPEN_TRUNCATE,
    &destination_descriptor);

  if (status != FS_STATUS_OK) {
    (void)fs_vfs_close(vfs_context, source_descriptor);
    return status;
  }

  uint8_t transfer_buffer[CP_BUFFER_SIZE];
  size_t bytes_read = 0;
  uint64_t total_transferred = 0U;
  bool copy_failed = false;

  while (true) {
    status = fs_vfs_read(vfs_context,
                         source_descriptor,
                         transfer_buffer,
                         sizeof(transfer_buffer),
                         &bytes_read);
    if (status != FS_STATUS_OK) {
      copy_failed = true;
      break;
    }

    if (bytes_read == 0U) {
      break;
    }

    size_t bytes_written = 0;
    status = fs_vfs_write(vfs_context,
                          destination_descriptor,
                          transfer_buffer,
                          bytes_read,
                          &bytes_written);

    if (status != FS_STATUS_OK
        || bytes_written != bytes_read) {
      copy_failed = true;
      break;
    }

    total_transferred += bytes_written;
  }

  (void)fs_vfs_close(vfs_context, source_descriptor);
  (void)fs_vfs_close(vfs_context, destination_descriptor);

  if (copy_failed) {
    return status;
  }

  if (statistics != nullptr) {
    statistics->files_copied++;
    statistics->bytes_copied += total_transferred;
  }

  return FS_STATUS_OK;
}

static fs_status_t
copy_symlink(fs_vfs_context_t* vfs_context,
             const uint32_t source_inode_id,
             const char* destination_path,
             copy_statistics_t* statistics) {
  fs_inode_t symlink_inode = {0};
  fs_status_t status =
    fs_index_read_inode(vfs_context->index_context,
                        source_inode_id,
                        &symlink_inode);

  if (status != FS_STATUS_OK) {
    return status;
  }

  char target_buffer[FS_MAX_PATH_LENGTH] = {0};
  size_t bytes_read = 0U;
  status = fs_alloc_read_data(vfs_context->alloc_context,
                              &symlink_inode,
                              0U,
                              target_buffer,
                              sizeof(target_buffer) - 1U,
                              &bytes_read);

  if (status != FS_STATUS_OK) {
    return status;
  }

  target_buffer[bytes_read] = '\0';

  const fs_status_t ensure_status =
    cli_utils_ensure_parent_directories(vfs_context,
                                        destination_path);

  if (ensure_status != FS_STATUS_OK) {
    return ensure_status;
  }

  status = fs_vfs_create_symlink(
    vfs_context, target_buffer, destination_path);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (statistics != nullptr) {
    statistics->symlinks_copied++;
  }

  return FS_STATUS_OK;
}

static fs_status_t
recursive_copy_entry(fs_vfs_context_t* vfs_context,
                     const char* source_path,
                     const char* destination_path,
                     copy_statistics_t* statistics);

static void copy_directory_entry_callback(
  const char* entry_name,
  const uint32_t entry_inode_id,
  const fs_file_type_t entry_type,
  void* user_data) {
  copy_callback_context_t* callback_context =
    (copy_callback_context_t*)user_data;

  if (callback_context->has_error) {
    return;
  }

  if (strcmp(entry_name, ".") == 0
      || strcmp(entry_name, "..") == 0) {
    return;
  }

  char source_child_path[FS_MAX_PATH_LENGTH];
  char destination_child_path[FS_MAX_PATH_LENGTH];

  if (strcmp(callback_context->source_directory_path, "/")
      == 0) {
    snprintf(source_child_path,
             sizeof(source_child_path),
             "/%s",
             entry_name);
  } else {
    snprintf(source_child_path,
             sizeof(source_child_path),
             "%s/%s",
             callback_context->source_directory_path,
             entry_name);
  }

  if (strcmp(callback_context->destination_directory_path,
             "/")
      == 0) {
    snprintf(destination_child_path,
             sizeof(destination_child_path),
             "/%s",
             entry_name);
  } else {
    snprintf(destination_child_path,
             sizeof(destination_child_path),
             "%s/%s",
             callback_context->destination_directory_path,
             entry_name);
  }

  fs_status_t child_status = FS_STATUS_OK;

  if (entry_type == FS_TYPE_SYMLINK) {
    child_status =
      copy_symlink(callback_context->vfs_context,
                   entry_inode_id,
                   destination_child_path,
                   callback_context->statistics);
  } else {
    child_status =
      recursive_copy_entry(callback_context->vfs_context,
                           source_child_path,
                           destination_child_path,
                           callback_context->statistics);
  }

  if (child_status != FS_STATUS_OK) {
    callback_context->error_status = child_status;
    callback_context->has_error = true;
  }
}

static fs_status_t
copy_directory_recursive(fs_vfs_context_t* vfs_context,
                         const char* source_path,
                         const char* destination_path,
                         copy_statistics_t* statistics) {
  uint32_t existing_destination_inode = 0U;
  const fs_status_t resolve_status =
    vfs_resolve_path(vfs_context,
                     destination_path,
                     &existing_destination_inode);

  if (resolve_status != FS_STATUS_OK) {
    const fs_status_t ensure_status =
      cli_utils_ensure_parent_directories(vfs_context,
                                          destination_path);

    if (ensure_status != FS_STATUS_OK) {
      return ensure_status;
    }

    const fs_status_t create_status =
      fs_vfs_create_directory(vfs_context,
                              destination_path);

    if (create_status != FS_STATUS_OK) {
      return create_status;
    }
  } else {
    fs_inode_t existing_inode = {0};
    const fs_status_t info_status =
      fs_index_read_inode(vfs_context->index_context,
                          existing_destination_inode,
                          &existing_inode);

    if (info_status != FS_STATUS_OK) {
      return info_status;
    }

    if (existing_inode.type != FS_TYPE_DIRECTORY) {
      return FS_STATUS_ERROR_INVALID_ARGUMENT;
    }
  }

  if (statistics != nullptr) {
    statistics->directories_copied++;
  }

  uint32_t source_inode_id = 0U;
  fs_status_t status = vfs_resolve_path(
    vfs_context, source_path, &source_inode_id);

  if (status != FS_STATUS_OK) {
    return status;
  }

  copy_callback_context_t callback_context = {0};
  callback_context.vfs_context = vfs_context;
  callback_context.source_directory_path = source_path;
  callback_context.destination_directory_path =
    destination_path;
  callback_context.statistics = statistics;
  callback_context.has_error = false;
  callback_context.error_status = FS_STATUS_OK;

  status =
    fs_dir_list_entries(vfs_context->dir_context,
                        source_inode_id,
                        copy_directory_entry_callback,
                        &callback_context);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (callback_context.has_error) {
    return callback_context.error_status;
  }

  return FS_STATUS_OK;
}

static fs_status_t
recursive_copy_entry(fs_vfs_context_t* vfs_context,
                     const char* source_path,
                     const char* destination_path,
                     copy_statistics_t* statistics) {
  fs_inode_t source_inode_info = {0};
  const fs_status_t info_status = fs_vfs_get_info_no_follow(
    vfs_context, source_path, &source_inode_info);
  if (info_status != FS_STATUS_OK) {
    return info_status;
  }

  if (source_inode_info.type == FS_TYPE_SYMLINK) {
    return copy_symlink(vfs_context,
                        source_inode_info.id,
                        destination_path,
                        statistics);
  }

  if (source_inode_info.type == FS_TYPE_REGULAR) {
    return copy_single_file(vfs_context,
                            source_path,
                            destination_path,
                            statistics);
  }

  if (source_inode_info.type == FS_TYPE_DIRECTORY) {
    return copy_directory_recursive(vfs_context,
                                    source_path,
                                    destination_path,
                                    statistics);
  }

  return FS_STATUS_ERROR_INVALID_ARGUMENT;
}

void cli_command_cp(cli_main_state_t* state,
                    const int argc,
                    char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: cp <source> <destination>\n");
    fprintf(stderr,
            "  Copies files or directories recursively.\n");
    fprintf(stderr,
            "  If destination is an existing directory,\n");
    fprintf(stderr, "  source is copied inside it.\n");
    return;
  }

  char absolute_source[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_source,
    sizeof(absolute_source));

  char absolute_destination[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[2],
    absolute_destination,
    sizeof(absolute_destination));

  fs_inode_t source_inode_info = {0};
  fs_status_t status =
    fs_vfs_get_info(state->fs_state.fs_context.vfs_context,
                    absolute_source,
                    &source_inode_info);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "cp: cannot access '%s' (status: %d)\n",
            absolute_source,
            status);
    return;
  }

  char final_destination[FS_MAX_PATH_LENGTH];
  strncpy(final_destination,
          absolute_destination,
          sizeof(final_destination) - 1U);
  final_destination[sizeof(final_destination) - 1] = '\0';

  fs_inode_t destination_inode_info = {0};
  const fs_status_t destination_status =
    fs_vfs_get_info(state->fs_state.fs_context.vfs_context,
                    absolute_destination,
                    &destination_inode_info);

  if (destination_status == FS_STATUS_OK
      && destination_inode_info.type == FS_TYPE_DIRECTORY) {
    char source_basename[FS_MAX_FILENAME_LENGTH + 1U];
    extract_path_basename(absolute_source,
                          source_basename,
                          sizeof(source_basename));

    if (strcmp(absolute_destination, "/") == 0) {
      snprintf(final_destination,
               sizeof(final_destination),
               "/%s",
               source_basename);
    } else {
      snprintf(final_destination,
               sizeof(final_destination),
               "%s/%s",
               absolute_destination,
               source_basename);
    }
  }

  copy_statistics_t statistics = {0};
  status = recursive_copy_entry(
    state->fs_state.fs_context.vfs_context,
    absolute_source,
    final_destination,
    &statistics);

  if (status != FS_STATUS_OK) {
    fprintf(
      stderr, "cp: copy failed (status: %d)\n", status);
    return;
  }

  printf("Copied: '%s' -> '%s'\n",
         absolute_source,
         final_destination);
  printf("  Files:       %u\n", statistics.files_copied);
  printf("  Directories: %u\n",
         statistics.directories_copied);
  printf("  Symlinks:    %u\n", statistics.symlinks_copied);
  printf("  Bytes:       %llu\n",
         (unsigned long long)statistics.bytes_copied);
}

void cli_command_mv(cli_main_state_t* state,
                    const int argc,
                    char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: mv <source> <destination>\n");
    fprintf(stderr,
            "  Renames or moves files and directories.\n");
    fprintf(stderr,
            "  If destination is an existing directory,\n");
    fprintf(stderr, "  source is moved inside it.\n");
    return;
  }

  char absolute_source[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_source,
    sizeof(absolute_source));

  char absolute_destination[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[2],
    absolute_destination,
    sizeof(absolute_destination));

  fs_inode_t source_inode_info = {0};
  fs_status_t status =
    fs_vfs_get_info(state->fs_state.fs_context.vfs_context,
                    absolute_source,
                    &source_inode_info);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "mv: cannot access '%s' (status: %d)\n",
            absolute_source,
            status);
    return;
  }

  char final_destination[FS_MAX_PATH_LENGTH];
  strncpy(final_destination,
          absolute_destination,
          sizeof(final_destination) - 1U);
  final_destination[sizeof(final_destination) - 1U] = '\0';

  fs_inode_t destination_inode_info = {0};
  const fs_status_t destination_status =
    fs_vfs_get_info(state->fs_state.fs_context.vfs_context,
                    absolute_destination,
                    &destination_inode_info);

  if (destination_status == FS_STATUS_OK
      && destination_inode_info.type == FS_TYPE_DIRECTORY) {
    char source_basename[FS_MAX_FILENAME_LENGTH + 1U];
    const char* last_slash = strrchr(absolute_source, '/');
    const char* name_start = (last_slash != nullptr)
                               ? (last_slash + 1)
                               : absolute_source;
    strncpy(source_basename,
            name_start,
            sizeof(source_basename) - 1U);
    source_basename[sizeof(source_basename) - 1U] = '\0';

    if (strcmp(absolute_destination, "/") == 0) {
      snprintf(final_destination,
               sizeof(final_destination),
               "/%s",
               source_basename);
    } else {
      snprintf(final_destination,
               sizeof(final_destination),
               "%s/%s",
               absolute_destination,
               source_basename);
    }
  } else {
    const fs_status_t ensure_status =
      cli_utils_ensure_parent_directories(
        state->fs_state.fs_context.vfs_context,
        final_destination);
    if (ensure_status != FS_STATUS_OK) {
      fprintf(stderr,
              "mv: cannot create parent directories for "
              "'%s' (status: %d)\n",
              final_destination,
              ensure_status);
      return;
    }
  }

  if (source_inode_info.type == FS_TYPE_DIRECTORY) {
    const size_t source_length = strlen(absolute_source);

    if (strncmp(
          final_destination, absolute_source, source_length)
          == 0
        && (final_destination[source_length] == '/'
            || final_destination[source_length] == '\0')) {
      fprintf(stderr,
              "mv: cannot move directory '%s' into itself "
              "or its subdirectory\n",
              absolute_source);
      return;
    }
  }

  status =
    fs_vfs_rename(state->fs_state.fs_context.vfs_context,
                  absolute_source,
                  final_destination);
  if (status != FS_STATUS_OK) {
    fprintf(
      stderr, "mv: rename failed (status: %d)\n", status);
    return;
  }

  printf("Renamed: '%s' -> '%s'\n",
         absolute_source,
         final_destination);
}