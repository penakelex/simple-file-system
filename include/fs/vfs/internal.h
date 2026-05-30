#pragma once

#include "fs/vfs/vfs.h"
#include <stdint.h>

#define FS_CACHE_CLUSTER_COUNT 4U

typedef struct fs_file_descriptor {
  bool is_open;
  uint32_t inode_id;
  fs_file_type_t type;
  uint32_t current_position;
  uint32_t cached_file_size;
  bool allow_read;
  bool allow_write;

  uint32_t cached_logical_indices[FS_CACHE_CLUSTER_COUNT];
  uint8_t cached_buffers[FS_CACHE_CLUSTER_COUNT]
                        [FS_CLUSTER_SIZE];
  bool cache_slot_dirty[FS_CACHE_CLUSTER_COUNT];
  uint32_t cache_access_timestamp[FS_CACHE_CLUSTER_COUNT];
  uint32_t cache_timestamp_counter;
  size_t cache_valid_count;
} fs_file_descriptor_t;

struct fs_vfs_context {
  fs_alloc_context_t* alloc_context;
  fs_dir_context_t* dir_context;
  fs_index_t* index_context;
  fs_file_descriptor_t open_files[FS_MAX_OPEN_FILES];
  uint32_t root_inode_id;
};

[[nodiscard]] fs_status_t
vfs_resolve_path(fs_vfs_context_t* vfs_context,
                 const char* file_path,
                 uint32_t* output_inode_id);
[[nodiscard]] fs_status_t vfs_resolve_parent_and_name(
  fs_vfs_context_t* vfs_context,
  const char* file_path,
  uint32_t* output_parent_inode_id,
  char* output_file_name);
[[nodiscard]] fs_status_t
vfs_cache_flush_all(fs_vfs_context_t* vfs_context,
                    fs_file_descriptor_t* file_descriptor);
[[nodiscard]] fs_status_t
vfs_cache_flush_slot(fs_vfs_context_t* vfs_context,
                     fs_file_descriptor_t* file_descriptor,
                     const size_t cache_index);
[[nodiscard]] fs_status_t vfs_cache_load_cluster(
  fs_vfs_context_t* vfs_context,
  fs_file_descriptor_t* file_descriptor,
  uint32_t target_logical_cluster_index,
  int32_t* output_cache_index);