#include "commands.h"
#include "fs/space/bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct defrag_context {
  cli_context_t* fs_context;
  uint32_t files_processed;
  uint32_t clusters_moved;
  uint32_t total_clusters_used;
  uint32_t total_clusters_free;
  uint32_t highest_used_cluster;
} defrag_context_t;

static fs_status_t defrag_rebuild_inode_pointers(
  fs_disk_t* disk_context,
  fs_bitmap_t* bitmap_context,
  fs_inode_t* inode,
  const uint32_t* new_data_clusters,
  const uint32_t cluster_count) {
  memset(inode->direct_clusters,
         0,
         sizeof(inode->direct_clusters));
  inode->single_indirect_cluster = 0;
  inode->double_indirect_cluster = 0;
  inode->triple_indirect_cluster = 0;

  const uint32_t pointers_per_cluster =
    FS_CLUSTER_SIZE / sizeof(uint32_t);
  uint32_t current_index = 0;

  const uint32_t direct_count =
    (cluster_count < FS_DIRECT_POINTERS)
      ? cluster_count
      : FS_DIRECT_POINTERS;

  for (uint32_t i = 0; i < direct_count; ++i) {
    inode->direct_clusters[i] = new_data_clusters[i];
  }

  current_index = direct_count;
  uint32_t remaining = cluster_count - direct_count;

  if (remaining > 0) {
    uint32_t single_indirect = 0;
    fs_status_t status = fs_bitmap_find_free_cluster(
      bitmap_context, &single_indirect);

    if (status != FS_STATUS_OK) {
      return status;
    }
    status = fs_bitmap_mark_cluster_used(bitmap_context,
                                         single_indirect);

    if (status != FS_STATUS_OK) {
      return status;
    }

    uint32_t indirect_buffer[FS_CLUSTER_SIZE
                             / sizeof(uint32_t)] = {0};
    const uint32_t count =
      (remaining < pointers_per_cluster)
        ? remaining
        : pointers_per_cluster;

    for (uint32_t i = 0; i < count; ++i) {
      indirect_buffer[i] =
        new_data_clusters[current_index + i];
    }

    status = fs_disk_write_cluster(
      disk_context, single_indirect, indirect_buffer);

    if (status != FS_STATUS_OK) {
      return status;
    }

    inode->single_indirect_cluster = single_indirect;
    current_index += count;
    remaining -= count;
  }

  if (remaining > 0) {
    uint32_t double_indirect = 0;
    fs_status_t status = fs_bitmap_find_free_cluster(
      bitmap_context, &double_indirect);

    if (status != FS_STATUS_OK) {
      return status;
    }
    status = fs_bitmap_mark_cluster_used(bitmap_context,
                                         double_indirect);

    if (status != FS_STATUS_OK) {
      return status;
    }

    uint32_t double_buffer[FS_CLUSTER_SIZE
                           / sizeof(uint32_t)] = {0};
    uint32_t double_index = 0;

    while (remaining > 0
           && double_index < pointers_per_cluster) {
      uint32_t single_indirect = 0;
      status = fs_bitmap_find_free_cluster(
        bitmap_context, &single_indirect);

      if (status != FS_STATUS_OK) {
        return status;
      }
      status = fs_bitmap_mark_cluster_used(bitmap_context,
                                           single_indirect);

      if (status != FS_STATUS_OK) {
        return status;
      }

      uint32_t single_buffer[FS_CLUSTER_SIZE
                             / sizeof(uint32_t)] = {0};
      const uint32_t count =
        (remaining < pointers_per_cluster)
          ? remaining
          : pointers_per_cluster;

      for (uint32_t i = 0; i < count; ++i) {
        single_buffer[i] =
          new_data_clusters[current_index + i];
      }

      status = fs_disk_write_cluster(
        disk_context, single_indirect, single_buffer);

      if (status != FS_STATUS_OK) {
        return status;
      }

      double_buffer[double_index] = single_indirect;
      current_index += count;
      remaining -= count;
      double_index++;
    }

    status = fs_disk_write_cluster(
      disk_context, double_indirect, double_buffer);

    if (status != FS_STATUS_OK) {
      return status;
    }

    inode->double_indirect_cluster = double_indirect;
  }

  if (remaining > 0) {
    uint32_t triple_indirect = 0;
    fs_status_t status = fs_bitmap_find_free_cluster(
      bitmap_context, &triple_indirect);

    if (status != FS_STATUS_OK) {
      return status;
    }

    status = fs_bitmap_mark_cluster_used(bitmap_context,
                                         triple_indirect);

    if (status != FS_STATUS_OK) {
      return status;
    }

    uint32_t triple_buffer[FS_CLUSTER_SIZE
                           / sizeof(uint32_t)] = {0};
    uint32_t triple_index = 0;

    while (remaining > 0
           && triple_index < pointers_per_cluster) {
      uint32_t double_indirect = 0;
      status = fs_bitmap_find_free_cluster(
        bitmap_context, &double_indirect);

      if (status != FS_STATUS_OK) {
        return status;
      }
      status = fs_bitmap_mark_cluster_used(bitmap_context,
                                           double_indirect);

      if (status != FS_STATUS_OK) {
        return status;
      }

      uint32_t double_buf[FS_CLUSTER_SIZE
                          / sizeof(uint32_t)] = {0};
      uint32_t double_index = 0;

      while (remaining > 0
             && double_index < pointers_per_cluster) {
        uint32_t single_indirect = 0;
        status = fs_bitmap_find_free_cluster(
          bitmap_context, &single_indirect);

        if (status != FS_STATUS_OK) {
          return status;
        }
        status = fs_bitmap_mark_cluster_used(
          bitmap_context, single_indirect);

        if (status != FS_STATUS_OK) {
          return status;
        }

        uint32_t single_buf[FS_CLUSTER_SIZE
                            / sizeof(uint32_t)] = {0};
        const uint32_t count =
          (remaining < pointers_per_cluster)
            ? remaining
            : pointers_per_cluster;

        for (uint32_t i = 0; i < count; ++i) {
          single_buf[i] =
            new_data_clusters[current_index + i];
        }

        status = fs_disk_write_cluster(
          disk_context, single_indirect, single_buf);

        if (status != FS_STATUS_OK) {
          return status;
        }

        double_buf[double_index] = single_indirect;
        current_index += count;
        remaining -= count;
        double_index++;
      }

      status = fs_disk_write_cluster(
        disk_context, double_indirect, double_buf);

      if (status != FS_STATUS_OK) {
        return status;
      }

      triple_buffer[triple_index] = double_indirect;
      triple_index++;
    }

    status = fs_disk_write_cluster(
      disk_context, triple_indirect, triple_buffer);

    if (status != FS_STATUS_OK) {
      return status;
    }

    inode->triple_indirect_cluster = triple_indirect;
  }

  inode->cluster_count = cluster_count;
  return FS_STATUS_OK;
}

static void
defrag_process_inode(defrag_context_t* defrag_context,
                     const uint32_t inode_id) {
  fs_inode_t inode = {0};
  fs_status_t status = fs_index_read_inode(
    defrag_context->fs_context->index_context,
    inode_id,
    &inode);

  if (status != FS_STATUS_OK || !inode.is_used
      || inode.type != FS_TYPE_REGULAR || inode.size == 0) {
    return;
  }

  const uint32_t cluster_count = inode.cluster_count;

  if (cluster_count <= 1) {
    defrag_context->files_processed++;
    defrag_context->total_clusters_used += cluster_count;

    if (cluster_count == 1) {
      uint32_t physical = 0;
      (void)fs_alloc_resolve_cluster(
        defrag_context->fs_context->alloc_context,
        &inode,
        0,
        false,
        &physical);

      if (physical > defrag_context->highest_used_cluster) {
        defrag_context->highest_used_cluster = physical;
      }
    }

    return;
  }

  uint32_t* old_clusters =
    calloc(cluster_count, sizeof(uint32_t));

  if (old_clusters == nullptr) {
    return;
  }

  for (uint32_t i = 0; i < cluster_count; ++i) {
    uint32_t physical = 0;
    status = fs_alloc_resolve_cluster(
      defrag_context->fs_context->alloc_context,
      &inode,
      i,
      false,
      &physical);

    if (status != FS_STATUS_OK) {
      free(old_clusters);
      return;
    }

    old_clusters[i] = physical;
  }

  bool is_contiguous = true;

  for (uint32_t i = 1; i < cluster_count; ++i) {
    if (old_clusters[i] != old_clusters[i - 1] + 1) {
      is_contiguous = false;
      break;
    }
  }

  if (is_contiguous) {
    defrag_context->files_processed++;
    defrag_context->total_clusters_used += cluster_count;
    const uint32_t last_cluster =
      old_clusters[cluster_count - 1];

    if (last_cluster
        > defrag_context->highest_used_cluster) {
      defrag_context->highest_used_cluster = last_cluster;
    }

    free(old_clusters);
    return;
  }

  uint32_t new_start = 0;
  status = fs_bitmap_find_contiguous_free(
    defrag_context->fs_context->bitmap_context,
    cluster_count,
    &new_start);

  if (status != FS_STATUS_OK) {
    free(old_clusters);
    return;
  }

  for (uint32_t i = 0; i < cluster_count; ++i) {
    (void)fs_bitmap_mark_cluster_used(
      defrag_context->fs_context->bitmap_context,
      new_start + i);
  }

  uint8_t cluster_buffer[FS_CLUSTER_SIZE] = {0};
  bool copy_failed = false;

  for (uint32_t i = 0; i < cluster_count; ++i) {
    status = fs_disk_read_cluster(
      defrag_context->fs_context->disk_context,
      old_clusters[i],
      cluster_buffer);

    if (status != FS_STATUS_OK) {
      copy_failed = true;
      break;
    }

    status = fs_disk_write_cluster(
      defrag_context->fs_context->disk_context,
      new_start + i,
      cluster_buffer);

    if (status != FS_STATUS_OK) {
      copy_failed = true;
      break;
    }
  }

  if (copy_failed) {
    for (uint32_t i = 0; i < cluster_count; ++i) {
      (void)fs_bitmap_mark_cluster_free(
        defrag_context->fs_context->bitmap_context,
        new_start + i);
    }

    free(old_clusters);
    return;
  }

  uint32_t* new_clusters =
    calloc(cluster_count, sizeof(uint32_t));

  if (new_clusters == nullptr) {
    for (uint32_t i = 0; i < cluster_count; ++i) {
      (void)fs_bitmap_mark_cluster_free(
        defrag_context->fs_context->bitmap_context,
        new_start + i);
    }

    free(old_clusters);
    return;
  }

  for (uint32_t i = 0; i < cluster_count; ++i) {
    new_clusters[i] = new_start + i;
  }

  fs_inode_t new_inode = inode;
  status = defrag_rebuild_inode_pointers(
    defrag_context->fs_context->disk_context,
    defrag_context->fs_context->bitmap_context,
    &new_inode,
    new_clusters,
    cluster_count);

  free(new_clusters);

  if (status != FS_STATUS_OK) {
    for (uint32_t i = 0; i < cluster_count; ++i) {
      (void)fs_bitmap_mark_cluster_free(
        defrag_context->fs_context->bitmap_context,
        new_start + i);
    }

    free(old_clusters);
    return;
  }

  const uint32_t original_size = inode.size;

  (void)fs_alloc_truncate_file(
    defrag_context->fs_context->alloc_context, &inode);

  memcpy(inode.direct_clusters,
         new_inode.direct_clusters,
         sizeof(inode.direct_clusters));
  inode.single_indirect_cluster =
    new_inode.single_indirect_cluster;
  inode.double_indirect_cluster =
    new_inode.double_indirect_cluster;
  inode.triple_indirect_cluster =
    new_inode.triple_indirect_cluster;
  inode.size = original_size;
  inode.cluster_count = cluster_count;

  (void)fs_index_write_inode(
    defrag_context->fs_context->index_context, &inode);

  defrag_context->files_processed++;
  defrag_context->clusters_moved += cluster_count;
  defrag_context->total_clusters_used += cluster_count;
  const uint32_t last_new_cluster =
    new_start + cluster_count - 1;

  if (last_new_cluster
      > defrag_context->highest_used_cluster) {
    defrag_context->highest_used_cluster = last_new_cluster;
  }

  free(old_clusters);
}

static void
defrag_directory_callback(const char* entry_name,
                          const uint32_t entry_inode_id,
                          const fs_file_type_t entry_type,
                          void* user_data) {
  defrag_context_t* defrag_context =
    (defrag_context_t*)user_data;

  if (strcmp(entry_name, ".") == 0
      || strcmp(entry_name, "..") == 0) {
    return;
  }

  if (entry_type == FS_TYPE_REGULAR) {
    defrag_process_inode(defrag_context, entry_inode_id);
  } else if (entry_type == FS_TYPE_DIRECTORY) {
    (void)fs_dir_list_entries(
      defrag_context->fs_context->dir_context,
      entry_inode_id,
      defrag_directory_callback,
      user_data);
  }
}

void cli_command_defrag(cli_main_state_t* state,
                        const int argc,
                        char** argv) {
  (void)argc;
  (void)argv;
  printf("Starting disk defragmentation...\n");

  defrag_context_t defrag_context = {0};
  defrag_context.fs_context = &state->fs_state.fs_context;
  defrag_context.highest_used_cluster = 0;

  (void)fs_dir_list_entries(
    state->fs_state.fs_context.dir_context,
    state->fs_state.fs_context.root_inode_id,
    defrag_directory_callback,
    &defrag_context);

  (void)fs_bitmap_serialize_to_disk(
    state->fs_state.fs_context.bitmap_context,
    state->fs_state.fs_context.disk_context);
  (void)fs_index_flush(
    state->fs_state.fs_context.index_context);
  (void)fs_disk_flush(
    state->fs_state.fs_context.disk_context);

  const uint32_t total_clusters =
    fs_bitmap_get_total_cluster_count(
      state->fs_state.fs_context.bitmap_context);
  const uint32_t used_clusters =
    fs_bitmap_count_used_clusters(
      state->fs_state.fs_context.bitmap_context);
  const uint32_t free_clusters =
    total_clusters - used_clusters;

  const uint64_t current_size_bytes =
    (uint64_t)total_clusters * FS_CLUSTER_SIZE;
  const uint64_t used_bytes =
    (uint64_t)used_clusters * FS_CLUSTER_SIZE;
  const uint64_t free_bytes =
    (uint64_t)free_clusters * FS_CLUSTER_SIZE;
  const double usage_percent =
    (total_clusters > 0U)
      ? ((double)used_clusters / (double)total_clusters
         * 100.0)
      : 0.0;

  printf("Defragmentation complete:\n");
  printf("  Files processed:       %u\n",
         defrag_context.files_processed);
  printf("  Clusters moved:        %u\n",
         defrag_context.clusters_moved);
  printf("  Total clusters:        %u\n", total_clusters);
  printf("  Used clusters:         %u (%.1f%%)\n",
         used_clusters,
         usage_percent);
  printf("  Free clusters:         %u\n", free_clusters);
  printf("  Highest used cluster:  %u\n",
         defrag_context.highest_used_cluster);
  printf("  Disk size:             %.2f MB\n",
         (double)current_size_bytes / (1024.0 * 1024.0));
  printf("  Used space:            %.2f KB\n",
         (double)used_bytes / 1024.0);
  printf("  Free space:            %.2f MB\n",
         (double)free_bytes / (1024.0 * 1024.0));
}

void cli_command_bitmap_dump(cli_main_state_t* state,
                             const int argc,
                             char** argv) {
  (void)argc;
  (void)argv;
  if (state->current_level != CLI_LEVEL_FS) {
    fprintf(stderr,
            "bitmap_dump: no file system is mounted\n");
    return;
  }
  const fs_bitmap_t* bitmap =
    state->fs_state.fs_context.bitmap_context;
  const uint32_t total =
    fs_bitmap_get_total_cluster_count(bitmap);
  printf("Cluster map (64 clusters per row):\n");
  printf("Legend: [.] = free, [#] = used\n\n");
  for (uint32_t i = 0; i < total; ++i) {
    bool is_free = fs_bitmap_is_cluster_free(bitmap, i);
    printf("%c", is_free ? '.' : '#');
    if ((i + 1) % 64 == 0) {
      printf("  | row %u\n", (i / 64) + 1);
    }
  }
  if (total % 64 != 0) {
    printf("\n");
  }
  printf("\nTotal: %u | Used: %u | Free: %u\n",
         total,
         fs_bitmap_count_used_clusters(bitmap),
         total - fs_bitmap_count_used_clusters(bitmap));
}