#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum [[nodiscard]] {
  FS_STATUS_OK = 0,
  FS_STATUS_ERROR_FILE_ACCESS,
  FS_STATUS_ERROR_INVALID_ARGUMENT,
  FS_STATUS_ERROR_OUT_OF_BOUNDS,
  FS_STATUS_ERROR_MEMORY_ALLOCATION,
  FS_STATUS_ERROR_READ_ONLY
} fs_status_t;

typedef struct fs_disk fs_disk_t;

[[nodiscard]] fs_status_t
fs_disk_create_or_open(fs_disk_t** output_disk_context,
                       const char* storage_file_path,
                       const uint32_t total_cluster_count,
                       const bool is_read_only);
[[nodiscard]] fs_status_t
fs_disk_close(fs_disk_t* disk_context);
[[nodiscard]] fs_status_t
fs_disk_read_cluster(const fs_disk_t* disk_context,
                     const uint32_t cluster_index,
                     void* destination_buffer);
[[nodiscard]] fs_status_t
fs_disk_write_cluster(fs_disk_t* disk_context,
                      const uint32_t cluster_index,
                      const void* source_buffer);
[[nodiscard]] fs_status_t
fs_disk_flush(const fs_disk_t* disk_context);

uint32_t fs_disk_get_total_cluster_count(
  const fs_disk_t* disk_context);
uint32_t fs_disk_get_cluster_size();
bool fs_disk_is_read_only(const fs_disk_t* disk_context);