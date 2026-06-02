#include "commands.h"
#include "utils.h"
#include <stdio.h>

void cli_command_ln(cli_main_state_t* state,
                    const int argc,
                    char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: ln <target> <link_name>\n");
    fprintf(stderr,
            "  Creates a symbolic link named <link_name> "
            "pointing to <target>.\n");
    fprintf(stderr,
            "  Example: ln /documents/report.txt "
            "/home/shortcut_to_report\n");
    return;
  }

  if (state->current_level != CLI_LEVEL_FS) {
    fprintf(stderr, "ln: no file system is mounted\n");
    return;
  }

  const char* target_path = argv[1];
  char absolute_link_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[2],
    absolute_link_path,
    sizeof(absolute_link_path));

  const fs_status_t status = fs_vfs_create_symlink(
    state->fs_state.fs_context.vfs_context,
    target_path,
    absolute_link_path);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "ln: cannot create symlink '%s' -> '%s' "
            "(status: %d)\n",
            absolute_link_path,
            target_path,
            status);
    return;
  }

  printf("Symlink created: '%s' -> '%s'\n",
         absolute_link_path,
         target_path);
}