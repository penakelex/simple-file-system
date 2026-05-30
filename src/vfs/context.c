#include "fs/vfs/internal.h"
#include <stdio.h>
#include <stdlib.h>

[[nodiscard]] static int32_t vfs_find_free_descriptor(
  const fs_vfs_context_t* vfs_context) {
  for (int32_t descriptor_index = 0;
       descriptor_index < (int32_t)FS_MAX_OPEN_FILES;
       ++descriptor_index) {
    if (!vfs_context->open_files[descriptor_index]
           .is_open) {
      return descriptor_index;
    }
  }

  return -1;
}

[[nodiscard]] fs_status_t
fs_vfs_create_context(fs_vfs_context_t** output_vfs_context,
                      fs_alloc_context_t* alloc_context,
                      fs_dir_context_t* dir_context,
                      fs_index_t* index_context,
                      uint32_t root_inode_id) {
  if (output_vfs_context == nullptr
      || alloc_context == nullptr || dir_context == nullptr
      || index_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_vfs_context_t* vfs_context =
    calloc(1, sizeof(fs_vfs_context_t));

  if (vfs_context == nullptr) {
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  vfs_context->alloc_context = alloc_context;
  vfs_context->dir_context = dir_context;
  vfs_context->index_context = index_context;
  vfs_context->root_inode_id = root_inode_id;

  for (uint32_t descriptor_index = 0;
       descriptor_index < FS_MAX_OPEN_FILES;
       ++descriptor_index) {
    vfs_context->open_files[descriptor_index].is_open =
      false;
  }

  *output_vfs_context = vfs_context;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_vfs_destroy_context(fs_vfs_context_t* vfs_context) {
  if (vfs_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  for (int32_t descriptor_index = 0;
       descriptor_index < (int32_t)FS_MAX_OPEN_FILES;
       ++descriptor_index) {
    if (vfs_context->open_files[descriptor_index].is_open) {
      (void)fs_vfs_close(vfs_context, descriptor_index);
    }
  }

  free(vfs_context);
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_vfs_open(fs_vfs_context_t* vfs_context,
            const char* file_path,
            uint32_t open_flags,
            int32_t* output_file_descriptor) {
  if (vfs_context == nullptr || file_path == nullptr
      || output_file_descriptor == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const bool is_read_mode =
    (open_flags & FS_OPEN_READ_ONLY) != 0;
  const bool is_write_mode =
    (open_flags & FS_OPEN_WRITE_ONLY) != 0;
  const bool is_append_mode =
    (open_flags & FS_OPEN_APPEND) != 0;
  const bool is_create_mode =
    (open_flags & FS_OPEN_CREATE) != 0;
  const bool is_truncate_mode =
    (open_flags & FS_OPEN_TRUNCATE) != 0;

  if (!is_read_mode && !is_write_mode) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  uint32_t target_inode_id = 0;
  fs_status_t status = vfs_resolve_path(
    vfs_context, file_path, &target_inode_id);

  if (status != FS_STATUS_OK) {
    if (!is_create_mode) {
      return status;
    }

    char file_name_buffer[FS_MAX_FILENAME_LENGTH + 1] = {0};
    uint32_t parent_inode_id = 0;
    status = vfs_resolve_parent_and_name(vfs_context,
                                         file_path,
                                         &parent_inode_id,
                                         file_name_buffer);
    if (status != FS_STATUS_OK) {
      return status;
    }

    status =
      fs_index_allocate_inode(vfs_context->index_context,
                              FS_TYPE_REGULAR,
                              &target_inode_id);
    if (status != FS_STATUS_OK) {
      return status;
    }

    status = fs_dir_insert_entry(vfs_context->dir_context,
                                 parent_inode_id,
                                 file_name_buffer,
                                 target_inode_id);
    if (status != FS_STATUS_OK) {
      (void)fs_index_free_inode(vfs_context->index_context,
                                target_inode_id);
      return status;
    }
  }

  fs_inode_t inode_context = {0};
  status = fs_index_read_inode(vfs_context->index_context,
                               target_inode_id,
                               &inode_context);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (inode_context.type != FS_TYPE_REGULAR) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (is_truncate_mode && is_write_mode) {
    status = fs_alloc_truncate_file(
      vfs_context->alloc_context, &inode_context);
    if (status != FS_STATUS_OK) {
      return status;
    }
  }

  const int32_t free_descriptor_index =
    vfs_find_free_descriptor(vfs_context);

  if (free_descriptor_index == -1) {
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  fs_file_descriptor_t* file_descriptor =
    &vfs_context->open_files[free_descriptor_index];

  file_descriptor->is_open = true;
  file_descriptor->inode_id = target_inode_id;
  file_descriptor->type = inode_context.type;
  file_descriptor->cached_file_size = inode_context.size;
  file_descriptor->allow_read = is_read_mode;
  file_descriptor->allow_write = is_write_mode;
  file_descriptor->cache_valid_count = 0;
  file_descriptor->cache_timestamp_counter = 0;

  if (is_append_mode) {
    file_descriptor->current_position = inode_context.size;
  } else {
    file_descriptor->current_position = 0;
  }

  *output_file_descriptor = free_descriptor_index;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_vfs_close(fs_vfs_context_t* vfs_context,
             int32_t file_descriptor) {
  if (vfs_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (file_descriptor < 0
      || file_descriptor >= (int32_t)FS_MAX_OPEN_FILES) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_file_descriptor_t* file_descriptor_context =
    &vfs_context->open_files[file_descriptor];

  if (!file_descriptor_context->is_open) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_status_t status = vfs_cache_flush_all(
    vfs_context, file_descriptor_context);

  if (status != FS_STATUS_OK) {
    return status;
  }

  file_descriptor_context->is_open = false;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_vfs_seek(fs_vfs_context_t* vfs_context,
            int32_t file_descriptor,
            int64_t offset,
            int origin) {
  if (vfs_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (file_descriptor < 0
      || file_descriptor >= (int32_t)FS_MAX_OPEN_FILES) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_file_descriptor_t* file_descriptor_context =
    &vfs_context->open_files[file_descriptor];

  if (!file_descriptor_context->is_open) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  int64_t new_position = 0;

  switch (origin) {
  case SEEK_SET:
    new_position = offset;
    break;
  case SEEK_CUR:
    new_position =
      (int64_t)file_descriptor_context->current_position
      + offset;
    break;
  case SEEK_END:
    new_position =
      (int64_t)file_descriptor_context->cached_file_size
      + offset;
    break;
  default:
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (new_position < 0) {
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  const uint32_t previous_cluster_index =
    file_descriptor_context->current_position
    / FS_CLUSTER_SIZE;
  const uint32_t target_cluster_index =
    (uint32_t)new_position / FS_CLUSTER_SIZE;

  if (previous_cluster_index != target_cluster_index) {
    fs_status_t status = vfs_cache_flush_all(
      vfs_context, file_descriptor_context);
    if (status != FS_STATUS_OK) {
      return status;
    }
    file_descriptor_context->cache_valid_count = 0;
  }

  file_descriptor_context->current_position =
    (uint32_t)new_position;
  return FS_STATUS_OK;
}