#include "commands.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

void cli_command_create(cli_main_state_t* state,
                        const int argc,
                        char** argv) {
  (void)state;

  if (argc < 3) {
    fprintf(stderr, "Usage: create <path> <clusters>\n");
    return;
  }

  const char* disk_path = argv[1];
  const uint32_t total_clusters =
    (uint32_t)strtoul(argv[2], nullptr, 10);

  if (total_clusters == 0U) {
    fprintf(stderr, "create: clusters must be > 0\n");
    return;
  }

  const fs_status_t status =
    cli_create_disk(disk_path, total_clusters);

  if (status != FS_STATUS_OK) {
    fprintf(
      stderr, "create: failed (status: %d)\n", status);
    return;
  }

  const uint64_t size_bytes =
    (uint64_t)total_clusters * FS_CLUSTER_SIZE;

  printf("Created and formatted disk image: %s (%u "
         "clusters, %.2f MB)\n",
         disk_path,
         total_clusters,
         (double)size_bytes / (1024.0 * 1024.0));
}

void cli_command_format(cli_main_state_t* state,
                        const int argc,
                        char** argv) {
  (void)state;

  if (argc < 3) {
    fprintf(stderr, "Usage: format <path> <clusters>\n");
    return;
  }

  const char* disk_path = argv[1];
  const uint32_t total_clusters =
    (uint32_t)strtoul(argv[2], nullptr, 10);

  if (total_clusters == 0U) {
    fprintf(stderr, "format: clusters must be > 0\n");
    return;
  }

  printf("Formatting disk '%s' with %u clusters...\n",
         disk_path,
         total_clusters);
  const fs_status_t status =
    cli_format_disk(disk_path, total_clusters);

  if (status != FS_STATUS_OK) {
    fprintf(
      stderr, "format: failed (status: %d)\n", status);
    return;
  }

  printf("Disk formatted successfully.\n");
}

void cli_command_open(cli_main_state_t* state,
                      const int argc,
                      char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: open <path>\n");
    return;
  }

  if (state->current_level != CLI_LEVEL_MANAGER) {
    fprintf(stderr,
            "open: already inside a file system (use "
            "'close' first)\n");
    return;
  }

  const char* disk_path = argv[1];

  printf("Mounting disk '%s'...\n", disk_path);
  const fs_status_t mount_status =
    cli_mount_disk(disk_path, &state->fs_state.fs_context);

  if (mount_status != FS_STATUS_OK) {
    fprintf(stderr,
            "open: mount failed (status: %d)\n",
            mount_status);
    return;
  }

  cli_utils_extract_disk_name_from_path(
    disk_path,
    state->fs_state.disk_name,
    sizeof(state->fs_state.disk_name));
  state->fs_state.current_fs_path[0] = '/';
  state->fs_state.current_fs_path[1] = '\0';
  state->fs_state.is_mounted = true;
  state->current_level = CLI_LEVEL_FS;
  printf("Disk mounted successfully.\n");
}

void cli_command_delete(cli_main_state_t* state,
                        const int argc,
                        char** argv) {
  (void)state;

  if (argc < 2) {
    fprintf(stderr, "Usage: delete <path>\n");
    return;
  }

  const char* disk_path = argv[1];
  const fs_status_t status = cli_delete_disk(disk_path);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "delete: failed to remove '%s' (status: %d)\n",
            disk_path,
            status);
    return;
  }

  printf("Deleted: %s\n", disk_path);
}