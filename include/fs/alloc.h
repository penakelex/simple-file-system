#pragma once

#include "fs/bitmap.h"
#include "fs/disk.h"
#include "fs/index.h"
#include <stdint.h>

typedef struct fs_alloc_context fs_alloc_context_t;

[[nodiscard]] fs_status_t fs_alloc_create_context(
  fs_alloc_context_t** output_alloc_context,
  fs_disk_t* disk_context,
  fs_bitmap_t* bitmap_context,
  fs_index_t* index_context);

[[nodiscard]] fs_status_t
fs_alloc_destroy_context(fs_alloc_context_t* alloc_context);

[[nodiscard]] fs_status_t fs_alloc_resolve_cluster(
  fs_alloc_context_t* alloc_context,
  fs_inode_t* inode_context,
  const uint32_t logical_cluster_index,
  const bool allocate_on_missing,
  uint32_t* output_physical_cluster_index);

[[nodiscard]] fs_status_t
fs_alloc_read_data(fs_alloc_context_t* alloc_context,
                   const fs_inode_t* inode_context,
                   const size_t byte_offset,
                   void* destination_buffer,
                   const size_t bytes_to_read,
                   size_t* bytes_actually_read);

[[nodiscard]] fs_status_t
fs_alloc_write_data(fs_alloc_context_t* alloc_context,
                    fs_inode_t* inode_context,
                    const size_t byte_offset,
                    const void* source_buffer,
                    const size_t bytes_to_write,
                    size_t* bytes_actually_written);

[[nodiscard]] fs_status_t
fs_alloc_truncate_file(fs_alloc_context_t* alloc_context,
                       fs_inode_t* inode_context);