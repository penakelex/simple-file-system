#pragma once

#include "fs/disk.h"
#include <stdint.h>

typedef struct fs_format_context fs_format_context_t;

[[nodiscard]] fs_status_t fs_format_create_context(
  fs_format_context_t** output_format_context);

[[nodiscard]] fs_status_t fs_format_destroy_context(
  fs_format_context_t* format_context);

[[nodiscard]] fs_status_t
fs_format_initialize_disk(fs_disk_t* disk_context,
                          uint32_t total_cluster_count);

[[nodiscard]] fs_status_t
fs_format_mount_disk(fs_disk_t* disk_context,
                     fs_format_context_t* format_context);

bool fs_format_is_mounted(
  const fs_format_context_t* format_context);