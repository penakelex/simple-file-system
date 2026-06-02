#include "commands.h"
#include "fs/vfs/internal.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

typedef struct find_context {
  cli_context_t* fs_context;
  const char* search_pattern;
  char current_path[FS_MAX_PATH_LENGTH];
  uint32_t match_count;
} find_context_t;

static void find_callback(const char* entry_name,
                          const uint32_t entry_inode_id,
                          const fs_file_type_t entry_type,
                          void* user_data) {
  find_context_t* find_context = (find_context_t*)user_data;

  if (strcmp(entry_name, ".") == 0
      || strcmp(entry_name, "..") == 0) {
    return;
  }

  if (strstr(entry_name, find_context->search_pattern)
      != nullptr) {
    char full_path[FS_MAX_PATH_LENGTH];

    if (strcmp(find_context->current_path, "/") == 0) {
      snprintf(
        full_path, sizeof(full_path), "/%s", entry_name);
    } else {
      snprintf(full_path,
               sizeof(full_path),
               "%s/%s",
               find_context->current_path,
               entry_name);
    }

    const char* type_indicator = "???";

    if (entry_type == FS_TYPE_REGULAR) {
      type_indicator = "FILE";
    } else if (entry_type == FS_TYPE_DIRECTORY) {
      type_indicator = "DIR ";
    } else if (entry_type == FS_TYPE_SYMLINK) {
      type_indicator = "LINK";
    }

    printf("  [%s] %s\n", type_indicator, full_path);
    find_context->match_count++;
  }

  if (entry_type == FS_TYPE_DIRECTORY) {
    char child_path[FS_MAX_PATH_LENGTH];

    if (strcmp(find_context->current_path, "/") == 0) {
      snprintf(
        child_path, sizeof(child_path), "/%s", entry_name);
    } else {
      snprintf(child_path,
               sizeof(child_path),
               "%s/%s",
               find_context->current_path,
               entry_name);
    }

    find_context_t child_context = {0};
    child_context.fs_context = find_context->fs_context;
    child_context.search_pattern =
      find_context->search_pattern;
    strncpy(child_context.current_path,
            child_path,
            sizeof(child_context.current_path) - 1U);
    child_context
      .current_path[sizeof(child_context.current_path)
                    - 1U] = '\0';
    child_context.match_count = 0U;

    (void)fs_dir_list_entries(
      find_context->fs_context->dir_context,
      entry_inode_id,
      find_callback,
      &child_context);
    find_context->match_count += child_context.match_count;
  }
}

void cli_command_find(cli_main_state_t* state,
                      const int argc,
                      char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: find <pattern> [start_path]\n");
    fprintf(stderr,
            "  Searches for files/directories containing "
            "<pattern> in their name.\n");
    return;
  }

  if (state->current_level != CLI_LEVEL_FS) {
    fprintf(stderr, "find: no file system is mounted\n");
    return;
  }

  const char* search_pattern = argv[1];
  char start_path[FS_MAX_PATH_LENGTH];

  if (argc >= 3) {
    cli_utils_build_absolute_path(
      state->fs_state.current_fs_path,
      argv[2],
      start_path,
      sizeof(start_path));
  } else {
    strncpy(start_path,
            state->fs_state.current_fs_path,
            sizeof(start_path) - 1U);
    start_path[sizeof(start_path) - 1U] = '\0';
  }

  uint32_t start_inode_id = 0U;
  const fs_status_t status =
    vfs_resolve_path(state->fs_state.fs_context.vfs_context,
                     start_path,
                     &start_inode_id);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "find: cannot access '%s' (status: %d)\n",
            start_path,
            status);
    return;
  }

  fs_inode_t start_inode = {0};
  const fs_status_t info_status = fs_index_read_inode(
    state->fs_state.fs_context.index_context,
    start_inode_id,
    &start_inode);

  if (info_status != FS_STATUS_OK
      || start_inode.type != FS_TYPE_DIRECTORY) {
    fprintf(stderr,
            "find: '%s' is not a directory\n",
            start_path);
    return;
  }

  find_context_t find_context = {0};
  find_context.fs_context = &state->fs_state.fs_context;
  find_context.search_pattern = search_pattern;
  strncpy(find_context.current_path,
          start_path,
          sizeof(find_context.current_path) - 1U);
  find_context
    .current_path[sizeof(find_context.current_path) - 1U] =
    '\0';
  find_context.match_count = 0U;

  printf("Searching for '%s' in %s:\n",
         search_pattern,
         start_path);
  (void)fs_dir_list_entries(
    state->fs_state.fs_context.dir_context,
    start_inode_id,
    find_callback,
    &find_context);

  if (find_context.match_count == 0U) {
    printf("  (no matches found)\n");
  } else {
    printf("Found %u match(es).\n",
           find_context.match_count);
  }
}