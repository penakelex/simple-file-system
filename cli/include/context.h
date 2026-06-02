#pragma once

#include "fs/alloc/alloc.h"
#include "fs/logical/dir.h"
#include "fs/metadata/index.h"
#include "fs/space/bitmap.h"
#include "fs/storage/disk.h"
#include "fs/vfs/vfs.h"
#include <stdint.h>

typedef struct cli_context {
  fs_disk_t* disk_context;
  fs_bitmap_t* bitmap_context;
  fs_index_t* index_context;
  fs_alloc_context_t* alloc_context;
  fs_dir_context_t* dir_context;
  fs_vfs_context_t* vfs_context;
  uint32_t root_inode_id;
} cli_context_t;

[[nodiscard]] fs_status_t
cli_mount_disk(const char* disk_path,
               cli_context_t* context);
[[nodiscard]] fs_status_t
cli_unmount_disk(cli_context_t* context);
[[nodiscard]] fs_status_t
cli_format_disk(const char* disk_path,
                const uint32_t total_cluster_count);
[[nodiscard]] fs_status_t
cli_create_disk(const char* disk_path,
                const uint32_t total_cluster_count);
[[nodiscard]] fs_status_t
cli_delete_disk(const char* disk_path);