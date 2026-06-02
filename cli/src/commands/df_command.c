#include "commands.h"
#include <stdio.h>

void cli_command_df(cli_main_state_t* state,
                    const int argc,
                    char** argv) {
  (void)argc;
  (void)argv;

  if (state->current_level != CLI_LEVEL_FS) {
    fprintf(stderr, "df: no file system is mounted\n");
    return;
  }

  const fs_bitmap_t* bitmap_context =
    state->fs_state.fs_context.bitmap_context;
  const fs_index_t* index_context =
    state->fs_state.fs_context.index_context;

  const uint32_t total_clusters =
    fs_bitmap_get_total_cluster_count(bitmap_context);
  const uint32_t used_clusters =
    fs_bitmap_count_used_clusters(bitmap_context);
  const uint32_t free_clusters =
    total_clusters - used_clusters;

  const uint32_t total_inodes = FS_INODE_COUNT;
  const uint32_t used_inodes =
    fs_index_count_used_inodes(index_context);
  const uint32_t free_inodes = total_inodes - used_inodes;

  const uint64_t total_bytes =
    (uint64_t)total_clusters * FS_CLUSTER_SIZE;
  const uint64_t used_bytes =
    (uint64_t)used_clusters * FS_CLUSTER_SIZE;
  const uint64_t free_bytes =
    (uint64_t)free_clusters * FS_CLUSTER_SIZE;

  const double cluster_usage_percent =
    (total_clusters > 0U)
      ? ((double)used_clusters / (double)total_clusters
         * 100.0)
      : 0.0;
  const double inode_usage_percent =
    (total_inodes > 0U)
      ? ((double)used_inodes / (double)total_inodes * 100.0)
      : 0.0;

  printf("File system statistics for '%s':\n",
         state->fs_state.disk_name);
  printf("  Clusters:\n");
  printf("    Total:    %u\n", total_clusters);
  printf("    Used:     %u (%.1f%%)\n",
         used_clusters,
         cluster_usage_percent);
  printf("    Free:     %u\n", free_clusters);
  printf("  Inodes:\n");
  printf("    Total:    %u\n", total_inodes);
  printf("    Used:     %u (%.1f%%)\n",
         used_inodes,
         inode_usage_percent);
  printf("    Free:     %u\n", free_inodes);
  printf("  Space:\n");
  printf("    Cluster size:  %u bytes\n", FS_CLUSTER_SIZE);
  printf("    Total:         %.2f MB\n",
         (double)total_bytes / (1024.0 * 1024.0));
  printf("    Used:          %.2f KB\n",
         (double)used_bytes / 1024.0);
  printf("    Free:          %.2f MB\n",
         (double)free_bytes / (1024.0 * 1024.0));
}