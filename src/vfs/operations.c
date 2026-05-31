#include "fs/vfs/internal.h"
#include <stdio.h>

[[nodiscard]] static bool
vfs_is_inode_open(const fs_vfs_context_t* vfs_context,
                  uint32_t target_inode_id) {
  for (int32_t descriptor_index = 0;
       descriptor_index < (int32_t)FS_MAX_OPEN_FILES;
       ++descriptor_index) {
    if (vfs_context->open_files[descriptor_index].is_open
        && vfs_context->open_files[descriptor_index]
               .inode_id
             == target_inode_id) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] fs_status_t
fs_vfs_get_info(fs_vfs_context_t* vfs_context,
                const char* file_path,
                fs_inode_t* output_inode_info) {
  if (vfs_context == nullptr || file_path == nullptr
      || output_inode_info == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  uint32_t target_inode_id = 0;
  fs_status_t status = vfs_resolve_path(
    vfs_context, file_path, &target_inode_id);

  if (status != FS_STATUS_OK) {
    return status;
  }

  return fs_index_read_inode(vfs_context->index_context,
                             target_inode_id,
                             output_inode_info);
}

[[nodiscard]] fs_status_t
fs_vfs_remove(fs_vfs_context_t* vfs_context,
              const char* file_path) {
  if (vfs_context == nullptr || file_path == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  char file_name_buffer[FS_MAX_FILENAME_LENGTH + 1] = {0};
  uint32_t parent_inode_id = 0;

  fs_status_t status =
    vfs_resolve_parent_and_name(vfs_context,
                                file_path,
                                &parent_inode_id,
                                file_name_buffer);

  if (status != FS_STATUS_OK) {
    return status;
  }

  uint32_t target_inode_id = 0;
  status = fs_dir_find_entry(vfs_context->dir_context,
                             parent_inode_id,
                             file_name_buffer,
                             &target_inode_id);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (vfs_is_inode_open(vfs_context, target_inode_id)) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_inode_t target_inode_context = {0};
  status = fs_index_read_inode(vfs_context->index_context,
                               target_inode_id,
                               &target_inode_context);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (target_inode_context.type == FS_TYPE_DIRECTORY) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  status = fs_dir_remove_entry(vfs_context->dir_context,
                               parent_inode_id,
                               file_name_buffer);

  if (status != FS_STATUS_OK) {
    return status;
  }

  status = fs_alloc_truncate_file(
    vfs_context->alloc_context, &target_inode_context);

  if (status != FS_STATUS_OK) {
    return status;
  }

  return fs_index_free_inode(vfs_context->index_context,
                             target_inode_id);
}

[[nodiscard]] fs_status_t
fs_vfs_rename(fs_vfs_context_t* vfs_context,
              const char* old_path,
              const char* new_path) {
  if (vfs_context == nullptr || old_path == nullptr
      || new_path == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  char old_name_buffer[FS_MAX_FILENAME_LENGTH + 1] = {0};
  uint32_t old_parent_inode_id = 0;
  char new_name_buffer[FS_MAX_FILENAME_LENGTH + 1] = {0};
  uint32_t new_parent_inode_id = 0;

  fs_status_t status =
    vfs_resolve_parent_and_name(vfs_context,
                                old_path,
                                &old_parent_inode_id,
                                old_name_buffer);

  if (status != FS_STATUS_OK) {
    return status;
  }

  status = vfs_resolve_parent_and_name(vfs_context,
                                       new_path,
                                       &new_parent_inode_id,
                                       new_name_buffer);

  if (status != FS_STATUS_OK) {
    return status;
  }

  uint32_t target_inode_id = 0;
  status = fs_dir_find_entry(vfs_context->dir_context,
                             old_parent_inode_id,
                             old_name_buffer,
                             &target_inode_id);

  if (status != FS_STATUS_OK) {
    return status;
  }

  fs_inode_t target_inode_context = {0};
  status = fs_index_read_inode(vfs_context->index_context,
                               target_inode_id,
                               &target_inode_context);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (target_inode_context.type == FS_TYPE_DIRECTORY) {
    status = fs_dir_remove_entry(
      vfs_context->dir_context, target_inode_id, "..");

    if (status != FS_STATUS_OK) {
      return status;
    }

    status = fs_dir_insert_entry(vfs_context->dir_context,
                                 target_inode_id,
                                 "..",
                                 new_parent_inode_id);

    if (status != FS_STATUS_OK) {
      return status;
    }
  }

  if (target_inode_context.type == FS_TYPE_SYMLINK) {
    fs_inode_t old_parent_inode = {0};
    status = fs_index_read_inode(vfs_context->index_context,
                                 old_parent_inode_id,
                                 &old_parent_inode);

    if (status != FS_STATUS_OK) {
      return status;
    }

    fs_inode_t new_parent_inode = {0};
    status = fs_index_read_inode(vfs_context->index_context,
                                 new_parent_inode_id,
                                 &new_parent_inode);
    if (status != FS_STATUS_OK) {
      return status;
    }

    if (old_parent_inode_id != new_parent_inode_id) {
      char old_parent_path[FS_MAX_PATH_LENGTH] = {0};
      char new_parent_path[FS_MAX_PATH_LENGTH] = {0};

      size_t bytes_read = 0;
      status =
        fs_alloc_read_data(vfs_context->alloc_context,
                           &old_parent_inode,
                           0,
                           old_parent_path,
                           sizeof(old_parent_path) - 1,
                           &bytes_read);

      if (status != FS_STATUS_OK) {
        return status;
      }

      old_parent_path[bytes_read] = '\0';

      bytes_read = 0;
      status =
        fs_alloc_read_data(vfs_context->alloc_context,
                           &new_parent_inode,
                           0,
                           new_parent_path,
                           sizeof(new_parent_path) - 1,
                           &bytes_read);

      if (status != FS_STATUS_OK) {
        return status;
      }

      new_parent_path[bytes_read] = '\0';

      if (strcmp(old_parent_path, new_parent_path) != 0) {
        char symlink_target[FS_MAX_PATH_LENGTH] = {0};
        bytes_read = 0;
        status =
          fs_alloc_read_data(vfs_context->alloc_context,
                             &target_inode_context,
                             0,
                             symlink_target,
                             sizeof(symlink_target) - 1,
                             &bytes_read);

        if (status != FS_STATUS_OK) {
          return status;
        }

        symlink_target[bytes_read] = '\0';

        if (symlink_target[0] != '/') {
          char updated_target[FS_MAX_PATH_LENGTH * 2] = {0};
          snprintf(updated_target,
                   sizeof(updated_target),
                   "../%s",
                   symlink_target);

          status = fs_alloc_truncate_file(
            vfs_context->alloc_context,
            &target_inode_context);

          if (status != FS_STATUS_OK) {
            return status;
          }

          size_t bytes_written = 0;
          status =
            fs_alloc_write_data(vfs_context->alloc_context,
                                &target_inode_context,
                                0,
                                updated_target,
                                strlen(updated_target) + 1,
                                &bytes_written);

          if (status != FS_STATUS_OK) {
            return status;
          }

          target_inode_context.size =
            (uint32_t)bytes_written;
          status =
            fs_index_write_inode(vfs_context->index_context,
                                 &target_inode_context);

          if (status != FS_STATUS_OK) {
            return status;
          }
        }
      }
    }
  }

  status = fs_dir_remove_entry(vfs_context->dir_context,
                               old_parent_inode_id,
                               old_name_buffer);

  if (status != FS_STATUS_OK) {
    return status;
  }

  return fs_dir_insert_entry(vfs_context->dir_context,
                             new_parent_inode_id,
                             new_name_buffer,
                             target_inode_id);
}

[[nodiscard]] fs_status_t
fs_vfs_create_directory(fs_vfs_context_t* vfs_context,
                        const char* directory_path) {
  if (vfs_context == nullptr || directory_path == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  char directory_name_buffer[FS_MAX_FILENAME_LENGTH + 1] = {
    0};
  uint32_t parent_inode_id = 0;
  fs_status_t status =
    vfs_resolve_parent_and_name(vfs_context,
                                directory_path,
                                &parent_inode_id,
                                directory_name_buffer);
  if (status != FS_STATUS_OK) {
    return status;
  }

  fs_inode_t new_directory_inode = {0};
  status =
    fs_index_allocate_inode(vfs_context->index_context,
                            FS_TYPE_DIRECTORY,
                            &new_directory_inode.id);
  if (status != FS_STATUS_OK) {
    return status;
  }

  new_directory_inode.type = FS_TYPE_DIRECTORY;
  new_directory_inode.is_used = true;
  new_directory_inode.link_count = 1;

  status = fs_dir_create_new(vfs_context->dir_context,
                             new_directory_inode.id,
                             parent_inode_id);
  if (status != FS_STATUS_OK) {
    (void)fs_index_free_inode(vfs_context->index_context,
                              new_directory_inode.id);
    return status;
  }

  return fs_dir_insert_entry(vfs_context->dir_context,
                             parent_inode_id,
                             directory_name_buffer,
                             new_directory_inode.id);
}

[[nodiscard]] fs_status_t
fs_vfs_remove_directory(fs_vfs_context_t* vfs_context,
                        const char* directory_path) {
  if (vfs_context == nullptr || directory_path == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  char directory_name_buffer[FS_MAX_FILENAME_LENGTH + 1] = {
    0};
  uint32_t parent_inode_id = 0;

  fs_status_t status =
    vfs_resolve_parent_and_name(vfs_context,
                                directory_path,
                                &parent_inode_id,
                                directory_name_buffer);

  if (status != FS_STATUS_OK) {
    return status;
  }

  uint32_t target_directory_inode_id = 0;
  status = fs_dir_find_entry(vfs_context->dir_context,
                             parent_inode_id,
                             directory_name_buffer,
                             &target_directory_inode_id);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (vfs_is_inode_open(vfs_context,
                        target_directory_inode_id)) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  bool is_directory_empty = false;
  status = fs_dir_is_empty(vfs_context->dir_context,
                           target_directory_inode_id,
                           &is_directory_empty);

  if (status != FS_STATUS_OK || !is_directory_empty) {
    return status != FS_STATUS_OK
             ? status
             : FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  status = fs_dir_remove_entry(vfs_context->dir_context,
                               parent_inode_id,
                               directory_name_buffer);

  if (status != FS_STATUS_OK) {
    return status;
  }

  return fs_index_free_inode(vfs_context->index_context,
                             target_directory_inode_id);
}