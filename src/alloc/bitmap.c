#include "fs/space/bitmap.h"
#include "fs/config.h"
#include <stdlib.h>
#include <string.h>

struct fs_bitmap {
  uint8_t* bitmap_buffer;
  uint32_t total_cluster_count;
  size_t bitmap_byte_length;
};

[[nodiscard]] fs_status_t
fs_bitmap_create(fs_bitmap_t** output_bitmap_context,
                 uint32_t total_cluster_count) {
  if (output_bitmap_context == nullptr
      || total_cluster_count == 0) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_bitmap_t* bitmap_context =
    calloc(1, sizeof(fs_bitmap_t));

  if (bitmap_context == nullptr) {
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  const size_t required_byte_length =
    (total_cluster_count + 7) / 8;

  bitmap_context->bitmap_buffer =
    calloc(1, required_byte_length);

  if (bitmap_context->bitmap_buffer == nullptr) {
    free(bitmap_context);
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  bitmap_context->total_cluster_count = total_cluster_count;
  bitmap_context->bitmap_byte_length = required_byte_length;
  *output_bitmap_context = bitmap_context;

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_bitmap_destroy(fs_bitmap_t* bitmap_context) {
  if (bitmap_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (bitmap_context->bitmap_buffer != nullptr) {
    free(bitmap_context->bitmap_buffer);
  }

  free(bitmap_context);
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t fs_bitmap_find_free_cluster(
  const fs_bitmap_t* bitmap_context,
  uint32_t* output_free_cluster_index) {
  if (bitmap_context == nullptr
      || output_free_cluster_index == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  for (size_t byte_index = 0;
       byte_index < bitmap_context->bitmap_byte_length;
       ++byte_index) {
    const uint8_t current_byte =
      bitmap_context->bitmap_buffer[byte_index];

    if (current_byte != 0xFF) {
      for (uint32_t bit_offset = 0; bit_offset < 8;
           ++bit_offset) {
        const uint32_t candidate_cluster_index =
          (uint32_t)(byte_index * 8) + bit_offset;

        if (candidate_cluster_index
            >= bitmap_context->total_cluster_count) {
          return FS_STATUS_ERROR_OUT_OF_BOUNDS;
        }

        if (!((current_byte >> bit_offset) & 1U)) {
          *output_free_cluster_index =
            candidate_cluster_index;
          return FS_STATUS_OK;
        }
      }
    }
  }

  return FS_STATUS_ERROR_OUT_OF_BOUNDS;
}

[[nodiscard]] fs_status_t
fs_bitmap_mark_cluster_used(fs_bitmap_t* bitmap_context,
                            uint32_t target_cluster_index) {
  if (bitmap_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (target_cluster_index
      >= bitmap_context->total_cluster_count) {
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  const size_t byte_index = target_cluster_index / 8;
  const uint32_t bit_offset = target_cluster_index % 8;
  bitmap_context->bitmap_buffer[byte_index] |=
    (uint8_t)(1U << bit_offset);

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_bitmap_mark_cluster_free(fs_bitmap_t* bitmap_context,
                            uint32_t target_cluster_index) {
  if (bitmap_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (target_cluster_index
      >= bitmap_context->total_cluster_count) {
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  const size_t byte_index = target_cluster_index / 8;
  const uint32_t bit_offset = target_cluster_index % 8;
  bitmap_context->bitmap_buffer[byte_index] &=
    (uint8_t)~(1U << bit_offset);

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t fs_bitmap_serialize_to_disk(
  const fs_bitmap_t* bitmap_context,
  fs_disk_t* disk_context) {
  if (bitmap_context == nullptr
      || disk_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  size_t current_offset = 0;

  while (current_offset
         < bitmap_context->bitmap_byte_length) {
    const size_t bytes_remaining =
      bitmap_context->bitmap_byte_length - current_offset;
    const size_t chunk_size =
      (bytes_remaining > FS_CLUSTER_SIZE) ? FS_CLUSTER_SIZE
                                          : bytes_remaining;

    uint8_t write_buffer[FS_CLUSTER_SIZE] = {0};
    memcpy(write_buffer,
           bitmap_context->bitmap_buffer + current_offset,
           chunk_size);

    const uint32_t target_cluster_index =
      (uint32_t)(current_offset / FS_CLUSTER_SIZE);
    const fs_status_t write_status = fs_disk_write_cluster(
      disk_context, target_cluster_index, write_buffer);

    if (write_status != FS_STATUS_OK) {
      return write_status;
    }

    current_offset += chunk_size;
  }

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_bitmap_deserialize_from_disk(fs_bitmap_t* bitmap_context,
                                fs_disk_t* disk_context) {
  if (bitmap_context == nullptr
      || disk_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  size_t current_offset = 0;

  while (current_offset
         < bitmap_context->bitmap_byte_length) {
    const size_t bytes_remaining =
      bitmap_context->bitmap_byte_length - current_offset;
    const size_t chunk_size =
      bytes_remaining > FS_CLUSTER_SIZE ? FS_CLUSTER_SIZE
                                        : bytes_remaining;

    uint8_t read_buffer[FS_CLUSTER_SIZE] = {0};
    const uint32_t source_cluster_index =
      (uint32_t)(current_offset / FS_CLUSTER_SIZE);

    const fs_status_t read_status = fs_disk_read_cluster(
      disk_context, source_cluster_index, read_buffer);
    if (read_status != FS_STATUS_OK) {
      return read_status;
    }

    memcpy(bitmap_context->bitmap_buffer + current_offset,
           read_buffer,
           chunk_size);
    current_offset += chunk_size;
  }

  return FS_STATUS_OK;
}

uint32_t fs_bitmap_get_total_cluster_count(
  const fs_bitmap_t* bitmap_context) {
  return bitmap_context != nullptr
           ? bitmap_context->total_cluster_count
           : 0;
}

size_t fs_bitmap_get_byte_length(
  const fs_bitmap_t* bitmap_context) {
  return bitmap_context != nullptr
           ? bitmap_context->bitmap_byte_length
           : 0;
}