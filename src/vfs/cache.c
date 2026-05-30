#include "fs/vfs/internal.h"
#include <string.h>

[[nodiscard]] static int32_t vfs_cache_find_slot(
  const fs_file_descriptor_t* file_descriptor,
  uint32_t target_logical_cluster_index) {
  for (size_t cache_index = 0;
       cache_index < file_descriptor->cache_valid_count;
       ++cache_index) {
    if (file_descriptor->cached_logical_indices[cache_index]
        == target_logical_cluster_index) {
      return (int32_t)cache_index;
    }
  }

  return -1;
}

[[nodiscard]] static int32_t
vfs_cache_find_least_recently_used_slot(
  const fs_file_descriptor_t* file_descriptor) {
  int32_t least_recently_used_index = -1;
  uint32_t oldest_timestamp = UINT32_MAX;

  for (size_t cache_index = 0;
       cache_index < file_descriptor->cache_valid_count;
       ++cache_index) {
    if (file_descriptor->cache_access_timestamp[cache_index]
        < oldest_timestamp) {
      oldest_timestamp =
        file_descriptor
          ->cache_access_timestamp[cache_index];
      least_recently_used_index = (int32_t)cache_index;
    }
  }

  return least_recently_used_index;
}

[[nodiscard]] fs_status_t
vfs_cache_flush_slot(fs_vfs_context_t* vfs_context,
                     fs_file_descriptor_t* file_descriptor,
                     size_t cache_index) {
  if (vfs_context == nullptr
      || file_descriptor == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (cache_index >= file_descriptor->cache_valid_count
      || !file_descriptor->cache_slot_dirty[cache_index]) {
    return FS_STATUS_OK;
  }

  fs_inode_t inode_context = {0};
  fs_status_t status =
    fs_index_read_inode(vfs_context->index_context,
                        file_descriptor->inode_id,
                        &inode_context);

  if (status != FS_STATUS_OK) {
    return status;
  }

  const size_t byte_offset =
    (size_t)
      file_descriptor->cached_logical_indices[cache_index]
    * FS_CLUSTER_SIZE;

  status = fs_alloc_write_data(
    vfs_context->alloc_context,
    &inode_context,
    byte_offset,
    file_descriptor->cached_buffers[cache_index],
    FS_CLUSTER_SIZE,
    &(size_t){0});

  if (status != FS_STATUS_OK) {
    return status;
  }

  file_descriptor->cache_slot_dirty[cache_index] = false;
  return fs_index_write_inode(vfs_context->index_context,
                              &inode_context);
}

[[nodiscard]] fs_status_t
vfs_cache_flush_all(fs_vfs_context_t* vfs_context,
                    fs_file_descriptor_t* file_descriptor) {
  if (vfs_context == nullptr
      || file_descriptor == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  for (size_t cache_index = 0;
       cache_index < file_descriptor->cache_valid_count;
       ++cache_index) {
    fs_status_t status = vfs_cache_flush_slot(
      vfs_context, file_descriptor, cache_index);
    if (status != FS_STATUS_OK) {
      return status;
    }
  }

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t vfs_cache_load_cluster(
  fs_vfs_context_t* vfs_context,
  fs_file_descriptor_t* file_descriptor,
  const uint32_t target_logical_cluster_index,
  int32_t* output_cache_index) {
  if (vfs_context == nullptr || file_descriptor == nullptr
      || output_cache_index == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const int32_t existing_slot_index = vfs_cache_find_slot(
    file_descriptor, target_logical_cluster_index);

  if (existing_slot_index != -1) {
    file_descriptor->cache_timestamp_counter++;
    file_descriptor
      ->cache_access_timestamp[existing_slot_index] =
      file_descriptor->cache_timestamp_counter;
    *output_cache_index = existing_slot_index;
    return FS_STATUS_OK;
  }

  int32_t target_slot_index = 0;

  if (file_descriptor->cache_valid_count
      < FS_CACHE_CLUSTER_COUNT) {
    target_slot_index =
      (int32_t)file_descriptor->cache_valid_count;
    file_descriptor->cache_valid_count++;
  } else {
    target_slot_index =
      vfs_cache_find_least_recently_used_slot(
        file_descriptor);

    if (target_slot_index == -1) {
      return FS_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    fs_status_t flush_status = vfs_cache_flush_slot(
      vfs_context, file_descriptor, target_slot_index);

    if (flush_status != FS_STATUS_OK) {
      return flush_status;
    }
  }

  fs_inode_t inode_context = {0};
  fs_status_t status =
    fs_index_read_inode(vfs_context->index_context,
                        file_descriptor->inode_id,
                        &inode_context);

  if (status != FS_STATUS_OK) {
    return status;
  }

  const size_t byte_offset =
    (size_t)target_logical_cluster_index * FS_CLUSTER_SIZE;

  size_t bytes_read = 0;
  status = fs_alloc_read_data(
    vfs_context->alloc_context,
    &inode_context,
    byte_offset,
    file_descriptor->cached_buffers[target_slot_index],
    FS_CLUSTER_SIZE,
    &bytes_read);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (bytes_read < FS_CLUSTER_SIZE) {
    memset(
      file_descriptor->cached_buffers[target_slot_index]
        + bytes_read,
      0,
      FS_CLUSTER_SIZE - bytes_read);
  }

  file_descriptor
    ->cached_logical_indices[target_slot_index] =
    target_logical_cluster_index;
  file_descriptor->cache_slot_dirty[target_slot_index] =
    false;
  file_descriptor->cache_timestamp_counter++;
  file_descriptor
    ->cache_access_timestamp[target_slot_index] =
    file_descriptor->cache_timestamp_counter;

  *output_cache_index = target_slot_index;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t vfs_flush_and_invalidate_cache(
  fs_vfs_context_t* vfs_context,
  fs_file_descriptor_t* file_descriptor) {
  const fs_status_t status =
    vfs_cache_flush_all(vfs_context, file_descriptor);

  if (status != FS_STATUS_OK) {
    return status;
  }

  file_descriptor->cache_valid_count = 0;
  return FS_STATUS_OK;
}