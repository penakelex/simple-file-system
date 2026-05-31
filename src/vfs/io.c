#include "fs/vfs/internal.h"
#include <stdio.h>
#include <string.h>

[[nodiscard]] static fs_status_t
vfs_sync_inode_size(fs_vfs_context_t* vfs_context,
                    uint32_t inode_id,
                    uint32_t new_size) {
  fs_inode_t inode_sync = {0};
  fs_status_t status = fs_index_read_inode(
    vfs_context->index_context, inode_id, &inode_sync);
  if (status != FS_STATUS_OK) {
    return status;
  }

  if (inode_sync.size != new_size) {
    inode_sync.size = new_size;
    status = fs_index_write_inode(
      vfs_context->index_context, &inode_sync);
  }
  return status;
}

static void vfs_prefetch_next_cluster(
  fs_vfs_context_t* vfs_context,
  fs_file_descriptor_t* file_descriptor,
  uint32_t current_logical_cluster_index) {
  if (file_descriptor->cache_valid_count
      >= FS_CACHE_CLUSTER_COUNT) {
    return;
  }

  const uint32_t next_cluster_index =
    current_logical_cluster_index + 1;

  int32_t existing_slot = -1;
  for (size_t cache_index = 0;
       cache_index < file_descriptor->cache_valid_count;
       ++cache_index) {
    if (file_descriptor->cached_logical_indices[cache_index]
        == next_cluster_index) {
      existing_slot = (int32_t)cache_index;
      break;
    }
  }

  if (existing_slot != -1) {
    return;
  }

  int32_t free_slot_index = -1;
  for (size_t cache_index = 0;
       cache_index < FS_CACHE_CLUSTER_COUNT;
       ++cache_index) {
    bool slot_occupied = false;
    for (size_t valid_index = 0;
         valid_index < file_descriptor->cache_valid_count;
         ++valid_index) {
      if (file_descriptor
            ->cached_logical_indices[valid_index]
          == (uint32_t)cache_index) {
        slot_occupied = true;
        break;
      }
    }
    if (!slot_occupied) {
      free_slot_index = (int32_t)cache_index;
      break;
    }
  }

  if (free_slot_index == -1) {
    return;
  }

  fs_inode_t inode_context = {0};
  if (fs_index_read_inode(vfs_context->index_context,
                          file_descriptor->inode_id,
                          &inode_context)
      != FS_STATUS_OK) {
    return;
  }

  const size_t byte_offset =
    (size_t)next_cluster_index * FS_CLUSTER_SIZE;

  size_t bytes_read = 0;
  fs_status_t status = fs_alloc_read_data(
    vfs_context->alloc_context,
    &inode_context,
    byte_offset,
    file_descriptor->cached_buffers[free_slot_index],
    FS_CLUSTER_SIZE,
    &bytes_read);

  if (status != FS_STATUS_OK) {
    return;
  }

  if (bytes_read < FS_CLUSTER_SIZE) {
    memset(file_descriptor->cached_buffers[free_slot_index]
             + bytes_read,
           0,
           FS_CLUSTER_SIZE - bytes_read);
  }

  file_descriptor->cached_logical_indices
    [file_descriptor->cache_valid_count] =
    next_cluster_index;
  file_descriptor
    ->cache_slot_dirty[file_descriptor->cache_valid_count] =
    false;
  file_descriptor->cache_access_timestamp
    [file_descriptor->cache_valid_count] =
    ++file_descriptor->cache_timestamp_counter;
  file_descriptor->cache_valid_count++;
}

[[nodiscard]] fs_status_t
fs_vfs_read(fs_vfs_context_t* vfs_context,
            int32_t file_descriptor,
            void* destination_buffer,
            size_t bytes_to_read,
            size_t* bytes_actually_read) {
  if (vfs_context == nullptr
      || destination_buffer == nullptr
      || bytes_actually_read == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (file_descriptor < 0
      || file_descriptor >= (int32_t)FS_MAX_OPEN_FILES) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_file_descriptor_t* file_descriptor_context =
    &vfs_context->open_files[file_descriptor];

  if (!file_descriptor_context->is_open
      || !file_descriptor_context->allow_read) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const size_t available_bytes =
    file_descriptor_context->cached_file_size
        > file_descriptor_context->current_position
      ? file_descriptor_context->cached_file_size
          - file_descriptor_context->current_position
      : 0;

  const size_t bytes_to_copy =
    bytes_to_read > available_bytes ? available_bytes
                                    : bytes_to_read;

  size_t total_bytes_read = 0;

  while (total_bytes_read < bytes_to_copy) {
    const uint32_t target_cluster_index =
      file_descriptor_context->current_position
      / FS_CLUSTER_SIZE;

    int32_t cache_index = -1;
    fs_status_t status =
      vfs_cache_load_cluster(vfs_context,
                             file_descriptor_context,
                             target_cluster_index,
                             &cache_index);

    if (status != FS_STATUS_OK) {
      break;
    }

    const size_t offset_in_cache =
      file_descriptor_context->current_position
      % FS_CLUSTER_SIZE;
    const size_t bytes_remaining_in_cluster =
      FS_CLUSTER_SIZE - offset_in_cache;
    const size_t bytes_to_copy_from_cache =
      bytes_remaining_in_cluster
          > bytes_to_copy - total_bytes_read
        ? bytes_to_copy - total_bytes_read
        : bytes_remaining_in_cluster;

    memcpy(
      (uint8_t*)destination_buffer + total_bytes_read,
      file_descriptor_context->cached_buffers[cache_index]
        + offset_in_cache,
      bytes_to_copy_from_cache);

    file_descriptor_context->current_position +=
      (uint32_t)bytes_to_copy_from_cache;
    total_bytes_read += bytes_to_copy_from_cache;

    if (bytes_to_copy_from_cache > 0
        && file_descriptor_context->cache_valid_count
             < FS_CACHE_CLUSTER_COUNT) {
      vfs_prefetch_next_cluster(vfs_context,
                                file_descriptor_context,
                                target_cluster_index);
    }
  }

  *bytes_actually_read = total_bytes_read;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_vfs_write(fs_vfs_context_t* vfs_context,
             int32_t file_descriptor,
             const void* source_buffer,
             size_t bytes_to_write,
             size_t* bytes_actually_written) {
  if (vfs_context == nullptr || source_buffer == nullptr
      || bytes_actually_written == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (file_descriptor < 0
      || file_descriptor >= (int32_t)FS_MAX_OPEN_FILES) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_file_descriptor_t* file_descriptor_context =
    &vfs_context->open_files[file_descriptor];

  if (!file_descriptor_context->is_open
      || !file_descriptor_context->allow_write) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  size_t total_bytes_written = 0;

  while (total_bytes_written < bytes_to_write) {
    const uint32_t target_cluster_index =
      file_descriptor_context->current_position
      / FS_CLUSTER_SIZE;

    int32_t cache_index = -1;
    fs_status_t status =
      vfs_cache_load_cluster(vfs_context,
                             file_descriptor_context,
                             target_cluster_index,
                             &cache_index);

    if (status != FS_STATUS_OK) {
      return status;
    }

    const size_t offset_in_cache =
      file_descriptor_context->current_position
      % FS_CLUSTER_SIZE;
    const size_t space_remaining_in_cluster =
      FS_CLUSTER_SIZE - offset_in_cache;
    const size_t bytes_to_copy_to_cache =
      space_remaining_in_cluster
          > bytes_to_write - total_bytes_written
        ? bytes_to_write - total_bytes_written
        : space_remaining_in_cluster;

    memcpy(
      file_descriptor_context->cached_buffers[cache_index]
        + offset_in_cache,
      (const uint8_t*)source_buffer + total_bytes_written,
      bytes_to_copy_to_cache);

    file_descriptor_context->cache_slot_dirty[cache_index] =
      true;
    file_descriptor_context->current_position +=
      (uint32_t)bytes_to_copy_to_cache;
    total_bytes_written += bytes_to_copy_to_cache;

    if (file_descriptor_context->current_position
        > file_descriptor_context->cached_file_size) {
      file_descriptor_context->cached_file_size =
        file_descriptor_context->current_position;

      status = vfs_sync_inode_size(
        vfs_context,
        file_descriptor_context->inode_id,
        file_descriptor_context->cached_file_size);
      if (status != FS_STATUS_OK) {
        return status;
      }
    }

    if (file_descriptor_context->current_position
          % FS_CLUSTER_SIZE
        == 0) {
      status = vfs_cache_flush_slot(
        vfs_context, file_descriptor_context, cache_index);
      if (status != FS_STATUS_OK) {
        return status;
      }
    }
  }

  *bytes_actually_written = total_bytes_written;
  return FS_STATUS_OK;
}