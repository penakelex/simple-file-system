#include "fs/vfs/internal.h"
#include <string.h>

static void
vfs_normalize_path_component(char* path_buffer,
                             size_t buffer_size) {
  if (path_buffer == nullptr || buffer_size == 0U) {
    return;
  }

  size_t read_index = 0;
  size_t write_index = 0;
  bool previous_was_slash = false;

  while (read_index < buffer_size
         && path_buffer[read_index] != '\0') {
    const char current_char = path_buffer[read_index];

    if (current_char == '/') {
      if (!previous_was_slash) {
        path_buffer[write_index++] = '/';
        previous_was_slash = true;
      }
    } else {
      path_buffer[write_index++] = current_char;
      previous_was_slash = false;
    }

    ++read_index;
  }

  if (write_index > 1
      && path_buffer[write_index - 1] == '/') {
    --write_index;
  }

  path_buffer[write_index] = '\0';
}

[[nodiscard]] static fs_status_t
vfs_resolve_relative_components(char* normalized_path,
                                size_t buffer_size) {
  if (normalized_path == nullptr || buffer_size == 0U) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  char* path_pointer = normalized_path;
  char* write_pointer = normalized_path;

  while (*path_pointer != '\0') {
    if (*path_pointer == '/') {
      *write_pointer++ = *path_pointer++;
      continue;
    }

    char* component_start = path_pointer;
    while (*path_pointer != '/' && *path_pointer != '\0') {
      ++path_pointer;
    }

    const size_t component_length =
      (size_t)(path_pointer - component_start);

    if (component_length == 1
        && component_start[0] == '.') {
      continue;
    }

    if (component_length == 2 && component_start[0] == '.'
        && component_start[1] == '.') {
      if (write_pointer > normalized_path + 1) {
        --write_pointer;

        while (write_pointer > normalized_path
               && *(write_pointer - 1) != '/') {
          --write_pointer;
        }
      }

      continue;
    }

    if (write_pointer + component_length + 1
        > normalized_path + buffer_size) {
      return FS_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    memcpy(
      write_pointer, component_start, component_length);
    write_pointer += component_length;
  }

  if (write_pointer > normalized_path + 1
      && *(write_pointer - 1) == '/') {
    --write_pointer;
  }

  *write_pointer = '\0';
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
vfs_resolve_path(fs_vfs_context_t* vfs_context,
                 const char* file_path,
                 uint32_t* output_inode_id) {
  if (vfs_context == nullptr || file_path == nullptr
      || output_inode_id == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  char normalized_path[FS_MAX_PATH_LENGTH] = {0};
  const size_t path_length = strlen(file_path);

  if (path_length >= FS_MAX_PATH_LENGTH) {
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  memcpy(normalized_path, file_path, path_length);
  normalized_path[path_length] = '\0';

  vfs_normalize_path_component(normalized_path,
                               sizeof(normalized_path));

  fs_status_t status = vfs_resolve_relative_components(
    normalized_path, sizeof(normalized_path));

  if (status != FS_STATUS_OK) {
    return status;
  }

  uint32_t current_inode_id = vfs_context->root_inode_id;
  const char* path_cursor = normalized_path;

  if (*path_cursor == '/') {
    ++path_cursor;
  }

  char path_component[FS_MAX_FILENAME_LENGTH + 1];
  uint32_t symlink_depth = 0;
  const uint32_t max_symlink_depth = 8U;

  while (*path_cursor != '\0'
         && symlink_depth < max_symlink_depth) {
    if (*path_cursor == '/') {
      ++path_cursor;
      continue;
    }

    uint32_t component_length = 0;

    while (path_cursor[component_length] != '/'
           && path_cursor[component_length] != '\0'
           && component_length < FS_MAX_FILENAME_LENGTH) {
      ++component_length;
    }

    if (component_length == 0) {
      break;
    }

    memcpy(path_component, path_cursor, component_length);
    path_component[component_length] = '\0';
    path_cursor += component_length;

    if (strcmp(path_component, ".") == 0) {
      continue;
    }

    if (strcmp(path_component, "..") == 0) {
      uint32_t parent_inode_id = 0;
      status = fs_dir_find_entry(vfs_context->dir_context,
                                 current_inode_id,
                                 "..",
                                 &parent_inode_id);

      if (status != FS_STATUS_OK) {
        return status;
      }

      current_inode_id = parent_inode_id;
      continue;
    }

    uint32_t next_inode_id = 0;
    status = fs_dir_find_entry(vfs_context->dir_context,
                               current_inode_id,
                               path_component,
                               &next_inode_id);

    if (status != FS_STATUS_OK) {
      return status;
    }

    fs_inode_t next_inode = {0};
    status = fs_index_read_inode(vfs_context->index_context,
                                 next_inode_id,
                                 &next_inode);

    if (status != FS_STATUS_OK) {
      return status;
    }

    if (next_inode.type == FS_TYPE_SYMLINK) {
      ++symlink_depth;

      char symlink_target[FS_MAX_PATH_LENGTH] = {0};
      size_t bytes_read = 0;
      status =
        fs_alloc_read_data(vfs_context->alloc_context,
                           &next_inode,
                           0,
                           symlink_target,
                           sizeof(symlink_target) - 1,
                           &bytes_read);

      if (status != FS_STATUS_OK) {
        return status;
      }

      symlink_target[bytes_read] = '\0';

      if (symlink_target[0] == '/') {
        current_inode_id = vfs_context->root_inode_id;
        path_cursor = symlink_target + 1;
      } else {
        path_cursor = symlink_target;
      }

      continue;
    }

    current_inode_id = next_inode_id;
  }

  if (symlink_depth >= max_symlink_depth) {
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  *output_inode_id = current_inode_id;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t vfs_resolve_parent_and_name(
  fs_vfs_context_t* vfs_context,
  const char* file_path,
  uint32_t* output_parent_inode_id,
  char* output_file_name) {
  if (vfs_context == nullptr || file_path == nullptr
      || output_parent_inode_id == nullptr
      || output_file_name == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const char* last_slash = strrchr(file_path, '/');
  const char* name_start =
    last_slash != nullptr ? last_slash + 1 : file_path;

  const size_t name_length = strlen(name_start);

  if (name_length == 0
      || name_length > FS_MAX_FILENAME_LENGTH) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  memcpy(output_file_name, name_start, name_length);
  output_file_name[name_length] = '\0';

  if (last_slash == nullptr || last_slash == file_path) {
    *output_parent_inode_id = vfs_context->root_inode_id;
  } else {
    size_t parent_path_length =
      (size_t)(last_slash - file_path);
    char parent_path_buffer[FS_MAX_PATH_LENGTH];

    if (parent_path_length >= FS_MAX_PATH_LENGTH) {
      return FS_STATUS_ERROR_INVALID_ARGUMENT;
    }

    memcpy(
      parent_path_buffer, file_path, parent_path_length);
    parent_path_buffer[parent_path_length] = '\0';

    fs_status_t status =
      vfs_resolve_path(vfs_context,
                       parent_path_buffer,
                       output_parent_inode_id);

    if (status != FS_STATUS_OK) {
      return status;
    }
  }

  return FS_STATUS_OK;
}