#include "commands.h"
#include "fs/metadata/index.h"
#include "fs/vfs/internal.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

void cli_command_fs_cd(cli_main_state_t* state,
                       const int argc,
                       char** argv) {
  if (argc < 2) {
    state->fs_state.current_fs_path[0] = '/';
    state->fs_state.current_fs_path[1] = '\0';
    return;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_path,
    sizeof(absolute_path));

  uint32_t target_inode_id = 0U;
  fs_status_t status =
    vfs_resolve_path(state->fs_state.fs_context.vfs_context,
                     absolute_path,
                     &target_inode_id);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "cd: cannot access '%s' (status: %d)\n",
            absolute_path,
            status);
    return;
  }

  fs_inode_t inode_info = {0};
  status = fs_index_read_inode(
    state->fs_state.fs_context.index_context,
    target_inode_id,
    &inode_info);

  if (status != FS_STATUS_OK
      || inode_info.type != FS_TYPE_DIRECTORY) {
    fprintf(stderr,
            "cd: '%s' is not a directory\n",
            absolute_path);
    return;
  }

  char normalized_path[FS_MAX_PATH_LENGTH];
  cli_utils_normalize_fs_path(absolute_path,
                              normalized_path,
                              sizeof(normalized_path));

  strncpy(state->fs_state.current_fs_path,
          normalized_path,
          sizeof(state->fs_state.current_fs_path) - 1U);
  state->fs_state
    .current_fs_path[sizeof(state->fs_state.current_fs_path)
                     - 1U] = '\0';
}

void cli_command_pwd(cli_main_state_t* state,
                     const int argc,
                     char** argv) {
  (void)argc;
  (void)argv;
  printf("%s\n", state->fs_state.current_fs_path);
}

static void ls_callback(const char* entry_name,
                        const uint32_t entry_inode_id,
                        const fs_file_type_t entry_type,
                        void* user_data) {
  (void)user_data;

  if (strcmp(entry_name, ".") == 0
      || strcmp(entry_name, "..") == 0) {
    return;
  }

  const char* type_indicator = "???";

  if (entry_type == FS_TYPE_REGULAR) {
    type_indicator = "FILE";
  } else if (entry_type == FS_TYPE_DIRECTORY) {
    type_indicator = "DIR ";
  } else if (entry_type == FS_TYPE_SYMLINK) {
    type_indicator = "LINK";
  }

  printf("  [%s] %-30s (inode: %u)\n",
         type_indicator,
         entry_name,
         entry_inode_id);
}

void cli_command_ls(cli_main_state_t* state,
                    const int argc,
                    char** argv) {
  char absolute_path[FS_MAX_PATH_LENGTH];

  if (argc >= 2) {
    cli_utils_build_absolute_path(
      state->fs_state.current_fs_path,
      argv[1],
      absolute_path,
      sizeof(absolute_path));
  } else {
    strncpy(absolute_path,
            state->fs_state.current_fs_path,
            sizeof(absolute_path) - 1U);
    absolute_path[sizeof(absolute_path) - 1U] = '\0';
  }

  uint32_t target_inode_id = 0;
  fs_status_t status =
    vfs_resolve_path(state->fs_state.fs_context.vfs_context,
                     absolute_path,
                     &target_inode_id);

  if (status != FS_STATUS_OK) {
    fprintf(
      stderr, "ls: cannot access '%s'\n", absolute_path);
    return;
  }

  fs_inode_t inode_info = {0};
  status = fs_index_read_inode(
    state->fs_state.fs_context.index_context,
    target_inode_id,
    &inode_info);

  if (status != FS_STATUS_OK
      || inode_info.type != FS_TYPE_DIRECTORY) {
    fprintf(stderr,
            "ls: '%s' is not a directory\n",
            absolute_path);
    return;
  }

  printf("Contents of %s:\n", absolute_path);
  status = fs_dir_list_entries(
    state->fs_state.fs_context.dir_context,
    target_inode_id,
    ls_callback,
    nullptr);

  if (status != FS_STATUS_OK) {
    fprintf(
      stderr, "ls: error listing (status: %d)\n", status);
  }
}

typedef struct tree_context {
  cli_context_t* fs_context;
  uint32_t depth;
} tree_context_t;

static void tree_callback(const char* entry_name,
                          const uint32_t entry_inode_id,
                          const fs_file_type_t entry_type,
                          void* user_data) {
  if (strcmp(entry_name, ".") == 0
      || strcmp(entry_name, "..") == 0) {
    return;
  }

  tree_context_t* context = (tree_context_t*)user_data;
  for (uint32_t indent_level = 0;
       indent_level < context->depth;
       ++indent_level) {
    printf("  ");
  }

  const char* type_prefix = "[FILE] ";

  if (entry_type == FS_TYPE_DIRECTORY) {
    type_prefix = "[DIR]  ";
  } else if (entry_type == FS_TYPE_SYMLINK) {
    type_prefix = "[LINK] ";
  }

  printf("%s%s\n", type_prefix, entry_name);

  if (entry_type == FS_TYPE_DIRECTORY) {
    tree_context_t child_context = {0};
    child_context.fs_context = context->fs_context;
    child_context.depth = context->depth + 1U;
    (void)fs_dir_list_entries(
      context->fs_context->dir_context,
      entry_inode_id,
      tree_callback,
      &child_context);
  }
}

void cli_command_tree(cli_main_state_t* state,
                      const int argc,
                      char** argv) {
  char absolute_path[FS_MAX_PATH_LENGTH];

  if (argc >= 2) {
    cli_utils_build_absolute_path(
      state->fs_state.current_fs_path,
      argv[1],
      absolute_path,
      sizeof(absolute_path));
  } else {
    strncpy(absolute_path,
            state->fs_state.current_fs_path,
            sizeof(absolute_path) - 1U);
    absolute_path[sizeof(absolute_path) - 1U] = '\0';
  }

  uint32_t target_inode_id = 0;
  const fs_status_t status =
    vfs_resolve_path(state->fs_state.fs_context.vfs_context,
                     absolute_path,
                     &target_inode_id);

  if (status != FS_STATUS_OK) {
    fprintf(
      stderr, "tree: cannot access '%s'\n", absolute_path);
    return;
  }

  printf("Tree of %s:\n", absolute_path);
  tree_context_t context = {0};
  context.fs_context = &state->fs_state.fs_context;
  context.depth = 0;
  (void)fs_dir_list_entries(
    state->fs_state.fs_context.dir_context,
    target_inode_id,
    tree_callback,
    &context);
}