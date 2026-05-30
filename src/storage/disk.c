#include "fs/storage/disk.h"
#include "fs/config.h"
#include "fs/platform_io.h"
#include <stdio.h>
#include <stdlib.h>

struct fs_disk {
  FILE* file_stream;
  uint32_t total_cluster_count;
  bool is_read_only;
};

[[nodiscard]] fs_status_t
fs_disk_create_or_open(fs_disk_t** output_disk_context,
                       const char* storage_file_path,
                       const uint32_t total_cluster_count,
                       const bool is_read_only) {
  if (output_disk_context == nullptr
      || storage_file_path == nullptr
      || total_cluster_count == 0) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_disk_t* disk_context = calloc(1, sizeof(fs_disk_t));

  if (disk_context == nullptr) {
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  disk_context->file_stream =
    fopen(storage_file_path, is_read_only ? "rb" : "r+b");

  if (disk_context->file_stream == nullptr) {
    if (!is_read_only) {
      disk_context->file_stream =
        fopen(storage_file_path, "w+b");

      if (disk_context->file_stream == nullptr) {
        free(disk_context);
        return FS_STATUS_ERROR_FILE_ACCESS;
      }

      const int64_t total_disk_size =
        (int64_t)total_cluster_count * FS_CLUSTER_SIZE;

      if (total_disk_size > 0) {
        if (fs_file_seek(disk_context->file_stream,
                         total_disk_size - 1,
                         SEEK_SET)
            != 0) {
          fclose(disk_context->file_stream);
          free(disk_context);
          return FS_STATUS_ERROR_FILE_ACCESS;
        }

        if (fwrite("\0", 1, 1, disk_context->file_stream)
            != 1) {
          fclose(disk_context->file_stream);
          free(disk_context);
          return FS_STATUS_ERROR_FILE_ACCESS;
        }

        if (fflush(disk_context->file_stream) != 0) {
          fclose(disk_context->file_stream);
          free(disk_context);
          return FS_STATUS_ERROR_FILE_ACCESS;
        }
      }
    } else {
      free(disk_context);
      return FS_STATUS_ERROR_FILE_ACCESS;
    }
  }

  disk_context->total_cluster_count = total_cluster_count;
  disk_context->is_read_only = is_read_only;
  *output_disk_context = disk_context;

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_disk_close(fs_disk_t* disk_context) {
  if (disk_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (disk_context->file_stream != nullptr) {
    fclose(disk_context->file_stream);
  }

  free(disk_context);
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_disk_read_cluster(const fs_disk_t* disk_context,
                     const uint32_t cluster_index,
                     void* destination_buffer) {
  if (disk_context == nullptr
      || destination_buffer == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (cluster_index >= disk_context->total_cluster_count) {
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  const int64_t byte_offset =
    (int64_t)cluster_index * FS_CLUSTER_SIZE;

  if (fs_file_seek(
        disk_context->file_stream, byte_offset, SEEK_SET)
      != 0) {
    return FS_STATUS_ERROR_FILE_ACCESS;
  }

  const size_t bytes_read =
    fread(destination_buffer,
          1,
          FS_CLUSTER_SIZE,
          disk_context->file_stream);

  if (bytes_read != FS_CLUSTER_SIZE) {
    return FS_STATUS_ERROR_FILE_ACCESS;
  }

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_disk_write_cluster(fs_disk_t* disk_context,
                      const uint32_t cluster_index,
                      const void* source_buffer) {
  if (disk_context == nullptr || source_buffer == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (disk_context->is_read_only) {
    return FS_STATUS_ERROR_READ_ONLY;
  }

  if (cluster_index >= disk_context->total_cluster_count) {
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  const int64_t byte_offset =
    (int64_t)cluster_index * FS_CLUSTER_SIZE;

  if (fs_file_seek(
        disk_context->file_stream, byte_offset, SEEK_SET)
      != 0) {
    return FS_STATUS_ERROR_FILE_ACCESS;
  }

  const size_t bytes_written =
    fwrite(source_buffer,
           1,
           FS_CLUSTER_SIZE,
           disk_context->file_stream);

  if (bytes_written != FS_CLUSTER_SIZE) {
    return FS_STATUS_ERROR_FILE_ACCESS;
  }

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_disk_flush(const fs_disk_t* disk_context) {
  if (disk_context == nullptr
      || disk_context->file_stream == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (fs_file_sync(disk_context->file_stream) != 0) {
    return FS_STATUS_ERROR_FILE_ACCESS;
  }

  return FS_STATUS_OK;
}

uint32_t fs_disk_get_total_cluster_count(
  const fs_disk_t* disk_context) {
  return disk_context != nullptr
           ? disk_context->total_cluster_count
           : 0;
}

uint32_t fs_disk_get_cluster_size() {
  return FS_CLUSTER_SIZE;
}

bool fs_disk_is_read_only(const fs_disk_t* disk_context) {
  return disk_context != nullptr
         && disk_context->is_read_only;
}