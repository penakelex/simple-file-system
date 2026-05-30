#pragma once

#include "fs/storage/disk.h"
#include "fs/types.h"

[[nodiscard]] fs_status_t
fs_superblock_initialize(fs_superblock_t* output_superblock,
                         uint32_t total_cluster_count);

[[nodiscard]] fs_status_t fs_superblock_validate(
  const fs_superblock_t* superblock_context);

[[nodiscard]] fs_status_t fs_superblock_read_from_disk(
  const fs_disk_t* disk_context,
  fs_superblock_t* output_superblock);

[[nodiscard]] fs_status_t fs_superblock_write_to_disk(
  fs_disk_t* disk_context,
  const fs_superblock_t* superblock_context);