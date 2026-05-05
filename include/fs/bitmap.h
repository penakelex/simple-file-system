#pragma once

#include "fs/disk.h"
#include <stdint.h>

typedef struct fs_bitmap fs_bitmap_t;

[[nodiscard]] fs_status_t
fs_bitmap_create(fs_bitmap_t** output_bitmap_context,
                 const uint32_t total_cluster_count);

[[nodiscard]] fs_status_t
fs_bitmap_destroy(fs_bitmap_t* bitmap_context);

[[nodiscard]] fs_status_t fs_bitmap_find_free_cluster(
  const fs_bitmap_t* bitmap_context,
  uint32_t* output_free_cluster_index);

[[nodiscard]] fs_status_t fs_bitmap_mark_cluster_used(
  fs_bitmap_t* bitmap_context,
  const uint32_t target_cluster_index);

[[nodiscard]] fs_status_t fs_bitmap_mark_cluster_free(
  fs_bitmap_t* bitmap_context,
  const uint32_t target_cluster_index);

[[nodiscard]] fs_status_t fs_bitmap_serialize_to_disk(
  const fs_bitmap_t* bitmap_context,
  fs_disk_t* disk_context);

[[nodiscard]] fs_status_t
fs_bitmap_deserialize_from_disk(fs_bitmap_t* bitmap_context,
                                fs_disk_t* disk_context);

uint32_t fs_bitmap_get_total_cluster_count(
  const fs_bitmap_t* bitmap_context);

size_t fs_bitmap_get_byte_length(
  const fs_bitmap_t* bitmap_context);