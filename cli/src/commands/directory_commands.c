#include "commands.h"
#include "core/terminal.h"
#include "fs/vfs/internal.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

typedef struct remove_stats {
  uint32_t files_removed;
  uint32_t directories_removed;
} remove_stats_t;

static fs_status_t recursive_remove_directory(
  cli_context_t* fs_context,
  const uint32_t directory_inode_id,
  remove_stats_t* stats) {
  fs_dentry_t** entry_array = nullptr;
  uint32_t entry_count = 0;

  fs_inode_t directory_inode = {0};
  fs_status_t status =
    fs_index_read_inode(fs_context->index_context,
                        directory_inode_id,
                        &directory_inode);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (directory_inode.size > 0) {
    uint8_t* directory_buffer =
      calloc(1, directory_inode.size);
    if (directory_buffer == nullptr) {
      return FS_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    size_t bytes_read = 0;
    status = fs_alloc_read_data(fs_context->alloc_context,
                                &directory_inode,
                                0,
                                directory_buffer,
                                directory_inode.size,
                                &bytes_read);

    if (status != FS_STATUS_OK) {
      free(directory_buffer);
      return status;
    }

    size_t current_offset = 0;
    while (current_offset < directory_inode.size) {
      if (current_offset + sizeof(fs_dentry_t)
          > directory_inode.size) {
        break;
      }

      const fs_dentry_t* current_dentry =
        (const fs_dentry_t*)(directory_buffer
                             + current_offset);

      if (current_dentry->record_length == 0) {
        break;
      }

      current_offset += current_dentry->record_length;
      entry_count++;
    }

    if (entry_count > 0) {
      entry_array =
        calloc(entry_count, sizeof(fs_dentry_t*));

      if (entry_array == nullptr) {
        free(directory_buffer);
        return FS_STATUS_ERROR_MEMORY_ALLOCATION;
      }

      current_offset = 0;

      for (uint32_t i = 0; i < entry_count; ++i) {
        const fs_dentry_t* current_dentry =
          (const fs_dentry_t*)(directory_buffer
                               + current_offset);
        entry_array[i] = fs_dentry_create(
          current_dentry->inode_id, current_dentry->name);
        current_offset += current_dentry->record_length;
      }
    }

    free(directory_buffer);
  }

  for (uint32_t i = 0; i < entry_count; ++i) {
    if (entry_array[i] == nullptr) {
      continue;
    }

    const char* entry_name = entry_array[i]->name;
    const uint32_t entry_inode_id =
      entry_array[i]->inode_id;

    if (strcmp(entry_name, ".") == 0
        || strcmp(entry_name, "..") == 0) {
      fs_dentry_destroy(entry_array[i]);
      continue;
    }

    fs_inode_t entry_inode = {0};
    status = fs_index_read_inode(fs_context->index_context,
                                 entry_inode_id,
                                 &entry_inode);

    if (status != FS_STATUS_OK) {
      fs_dentry_destroy(entry_array[i]);
      continue;
    }

    if (entry_inode.type == FS_TYPE_DIRECTORY) {
      status = recursive_remove_directory(
        fs_context, entry_inode_id, stats);

      if (status != FS_STATUS_OK) {
        fs_dentry_destroy(entry_array[i]);
        continue;
      }

      stats->directories_removed++;
    } else {
      (void)fs_alloc_truncate_file(
        fs_context->alloc_context, &entry_inode);
      (void)fs_index_free_inode(fs_context->index_context,
                                entry_inode_id);
      stats->files_removed++;
    }

    (void)fs_dir_remove_entry(fs_context->dir_context,
                              directory_inode_id,
                              entry_name);

    fs_dentry_destroy(entry_array[i]);
  }

  if (entry_array != nullptr) {
    free(entry_array);
  }

  (void)fs_alloc_truncate_file(fs_context->alloc_context,
                               &directory_inode);
  (void)fs_index_free_inode(fs_context->index_context,
                            directory_inode_id);

  return FS_STATUS_OK;
}

static void
count_directory_contents(cli_context_t* fs_context,
                         const uint32_t directory_inode_id,
                         uint32_t* file_count,
                         uint32_t* dir_count) {
  fs_inode_t directory_inode = {0};
  fs_status_t status =
    fs_index_read_inode(fs_context->index_context,
                        directory_inode_id,
                        &directory_inode);

  if (status != FS_STATUS_OK || directory_inode.size == 0) {
    return;
  }

  uint8_t* directory_buffer =
    calloc(1, directory_inode.size);

  if (directory_buffer == nullptr) {
    return;
  }

  size_t bytes_read = 0;
  status = fs_alloc_read_data(fs_context->alloc_context,
                              &directory_inode,
                              0,
                              directory_buffer,
                              directory_inode.size,
                              &bytes_read);

  if (status != FS_STATUS_OK) {
    free(directory_buffer);
    return;
  }

  size_t current_offset = 0;
  while (current_offset < directory_inode.size) {
    if (current_offset + sizeof(fs_dentry_t)
        > directory_inode.size) {
      break;
    }

    const fs_dentry_t* current_dentry =
      (const fs_dentry_t*)(directory_buffer
                           + current_offset);

    if (current_dentry->record_length == 0) {
      break;
    }

    const char* entry_name = current_dentry->name;

    if (strcmp(entry_name, ".") != 0
        && strcmp(entry_name, "..") != 0) {
      fs_inode_t entry_inode = {0};
      status =
        fs_index_read_inode(fs_context->index_context,
                            current_dentry->inode_id,
                            &entry_inode);

      if (status == FS_STATUS_OK) {
        if (entry_inode.type == FS_TYPE_DIRECTORY) {
          (*dir_count)++;
          count_directory_contents(fs_context,
                                   current_dentry->inode_id,
                                   file_count,
                                   dir_count);
        } else {
          (*file_count)++;
        }
      }
    }

    current_offset += current_dentry->record_length;
  }

  free(directory_buffer);
}

static bool cli_prompt_confirmation(const char* message) {
  cli_terminal_disable_raw_mode();

  printf("%s yes/no [default: no] ", message);
  fflush(stdout);

  char response[32] = {0};
  if (fgets(response, sizeof(response), stdin) == nullptr) {
    cli_terminal_enable_raw_mode();
    return false;
  }

  cli_terminal_enable_raw_mode();

  const size_t length = strlen(response);
  if (length > 0 && response[length - 1] == '\n') {
    response[length - 1] = '\0';
  }

  return (strcmp(response, "yes") == 0
          || strcmp(response, "y") == 0
          || strcmp(response, "YES") == 0
          || strcmp(response, "Y") == 0);
}

void cli_command_mkdir(cli_main_state_t* state,
                       const int argc,
                       char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: mkdir <path>\n");
    return;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_path,
    sizeof(absolute_path));

  char normalized_path[FS_MAX_PATH_LENGTH];
  cli_utils_normalize_fs_path(absolute_path,
                              normalized_path,
                              sizeof(normalized_path));

  char current_path[FS_MAX_PATH_LENGTH] = {0};
  const char* cursor = normalized_path;

  if (*cursor == '/') {
    current_path[0] = '/';
    current_path[1] = '\0';
    cursor++;
  }

  while (*cursor != '\0') {
    const char* component_start = cursor;

    while (*cursor != '\0' && *cursor != '/') {
      cursor++;
    }

    const size_t component_length =
      (size_t)(cursor - component_start);

    if (component_length == 0U) {
      if (*cursor == '/') {
        cursor++;
      }
      continue;
    }

    char component[FS_MAX_FILENAME_LENGTH + 1U];
    memcpy(component, component_start, component_length);
    component[component_length] = '\0';

    if (strcmp(current_path, "/") == 0) {
      snprintf(current_path,
               sizeof(current_path),
               "/%s",
               component);
    } else {
      const size_t current_length = strlen(current_path);
      snprintf(current_path + current_length,
               sizeof(current_path) - current_length,
               "/%s",
               component);
    }

    uint32_t existing_inode_id = 0U;
    const fs_status_t resolve_status = vfs_resolve_path(
      state->fs_state.fs_context.vfs_context,
      current_path,
      &existing_inode_id);

    if (resolve_status == FS_STATUS_OK) {
      fs_inode_t inode_info = {0};
      const fs_status_t info_status = fs_index_read_inode(
        state->fs_state.fs_context.index_context,
        existing_inode_id,
        &inode_info);

      if (info_status != FS_STATUS_OK
          || inode_info.type != FS_TYPE_DIRECTORY) {
        fprintf(
          stderr,
          "mkdir: '%s' exists but is not a directory\n",
          current_path);
        return;
      }
    } else {
      const fs_status_t create_status =
        fs_vfs_create_directory(
          state->fs_state.fs_context.vfs_context,
          current_path);

      if (create_status != FS_STATUS_OK) {
        fprintf(stderr,
                "mkdir: cannot create '%s' (status: %d)\n",
                current_path,
                create_status);
        return;
      }
    }

    if (*cursor == '/') {
      cursor++;
    }
  }

  printf("Directory created: %s\n", normalized_path);
}

void cli_command_rmdir(cli_main_state_t* state,
                       const int argc,
                       char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: rmdir <path>\n");
    return;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_path,
    sizeof(absolute_path));

  uint32_t target_inode_id = 0;
  fs_status_t status =
    vfs_resolve_path(state->fs_state.fs_context.vfs_context,
                     absolute_path,
                     &target_inode_id);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "rmdir: cannot access '%s' (status: %d)\n",
            absolute_path,
            status);
    return;
  }

  fs_inode_t target_inode = {0};
  status = fs_index_read_inode(
    state->fs_state.fs_context.index_context,
    target_inode_id,
    &target_inode);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "rmdir: cannot read inode (status: %d)\n",
            status);
    return;
  }

  if (target_inode.type != FS_TYPE_DIRECTORY) {
    fprintf(stderr,
            "rmdir: '%s' is not a directory\n",
            absolute_path);
    return;
  }

  if (target_inode_id
      == state->fs_state.fs_context.root_inode_id) {
    fprintf(stderr,
            "rmdir: cannot remove root directory\n");
    return;
  }

  bool is_empty = false;
  status =
    fs_dir_is_empty(state->fs_state.fs_context.dir_context,
                    target_inode_id,
                    &is_empty);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "rmdir: cannot check directory (status: %d)\n",
            status);
    return;
  }

  if (is_empty) {
    char dir_name[FS_MAX_FILENAME_LENGTH + 1] = {0};
    uint32_t parent_inode_id = 0;
    status = vfs_resolve_parent_and_name(
      state->fs_state.fs_context.vfs_context,
      absolute_path,
      &parent_inode_id,
      dir_name);

    if (status != FS_STATUS_OK) {
      fprintf(stderr,
              "rmdir: cannot resolve parent (status: %d)\n",
              status);
      return;
    }

    status = fs_dir_remove_entry(
      state->fs_state.fs_context.dir_context,
      parent_inode_id,
      dir_name);

    if (status != FS_STATUS_OK) {
      fprintf(stderr,
              "rmdir: cannot remove entry (status: %d)\n",
              status);
      return;
    }

    (void)fs_index_free_inode(
      state->fs_state.fs_context.index_context,
      target_inode_id);
    printf("Directory removed: %s\n", absolute_path);
  } else {
    uint32_t file_count = 0;
    uint32_t dir_count = 0;
    count_directory_contents(&state->fs_state.fs_context,
                             target_inode_id,
                             &file_count,
                             &dir_count);

    char prompt_message[256];
    snprintf(prompt_message,
             sizeof(prompt_message),
             "This directory contains %u file(s) and %u "
             "subdirectory(ies). "
             "Are you sure you want to delete it?",
             file_count,
             dir_count);

    if (!cli_prompt_confirmation(prompt_message)) {
      printf("Aborted.\n");
      return;
    }

    remove_stats_t stats = {0};
    status = recursive_remove_directory(
      &state->fs_state.fs_context, target_inode_id, &stats);
    if (status != FS_STATUS_OK) {
      fprintf(
        stderr,
        "rmdir: recursive removal failed (status: %d)\n",
        status);
      return;
    }

    char dir_name[FS_MAX_FILENAME_LENGTH + 1] = {0};
    uint32_t parent_inode_id = 0;
    status = vfs_resolve_parent_and_name(
      state->fs_state.fs_context.vfs_context,
      absolute_path,
      &parent_inode_id,
      dir_name);
    if (status == FS_STATUS_OK) {
      (void)fs_dir_remove_entry(
        state->fs_state.fs_context.dir_context,
        parent_inode_id,
        dir_name);
    }

    printf("Removed: %s (%u files, %u directories)\n",
           absolute_path,
           stats.files_removed,
           stats.directories_removed);
  }
}