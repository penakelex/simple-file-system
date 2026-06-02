#include "utils.h"
#include "fs/vfs/internal.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define cli_getcwd _getcwd
#else
#include <unistd.h>
#define cli_getcwd getcwd
#endif

void cli_utils_build_absolute_path(
  const char* current_directory,
  const char* relative_path,
  char* output_buffer,
  const size_t buffer_size) {
  if (output_buffer == nullptr || buffer_size == 0U) {
    return;
  }
  output_buffer[0] = '\0';

  if (relative_path == nullptr
      || relative_path[0] == '\0') {
    if (current_directory != nullptr) {
      strncpy(
        output_buffer, current_directory, buffer_size - 1U);
      output_buffer[buffer_size - 1U] = '\0';
    }

    return;
  }

  if (relative_path[0] == '/') {
    strncpy(output_buffer, relative_path, buffer_size - 1U);
    output_buffer[buffer_size - 1U] = '\0';
    return;
  }

  if (current_directory == nullptr
      || current_directory[0] == '\0') {
    snprintf(
      output_buffer, buffer_size, "/%s", relative_path);
    return;
  }

  if (strcmp(current_directory, "/") == 0) {
    snprintf(
      output_buffer, buffer_size, "/%s", relative_path);
  } else {
    snprintf(output_buffer,
             buffer_size,
             "%s/%s",
             current_directory,
             relative_path);
  }
}

void cli_utils_get_current_host_directory(
  char* output_buffer, const size_t buffer_size) {
  if (output_buffer == nullptr || buffer_size == 0U) {
    return;
  }

  if (cli_getcwd(output_buffer, buffer_size) == nullptr) {
    output_buffer[0] = '\0';
  }
}

bool cli_utils_file_exists(const char* file_path) {
  if (file_path == nullptr) {
    return false;
  }

  FILE* test_file = fopen(file_path, "rb");

  if (test_file != nullptr) {
    (void)fclose(test_file);
    return true;
  }

  return false;
}

void cli_utils_extract_disk_name_from_path(
  const char* disk_path,
  char* output_name,
  const size_t name_buffer_size) {
  if (output_name == nullptr || name_buffer_size == 0) {
    return;
  }

  output_name[0] = '\0';

  if (disk_path == nullptr) {
    return;
  }

  const char* last_separator = strrchr(disk_path, '/');
  const char* back_separator = strrchr(disk_path, '\\');

  if (back_separator != nullptr
      && (last_separator == nullptr
          || back_separator > last_separator)) {
    last_separator = back_separator;
  }

  const char* name_start = (last_separator != nullptr)
                             ? last_separator + 1
                             : disk_path;
  strncpy(output_name, name_start, name_buffer_size - 1U);
  output_name[name_buffer_size - 1U] = '\0';
}

size_t cli_utils_tokenize_line(char* input_line,
                               char** output_tokens,
                               const size_t max_tokens) {
  if (input_line == nullptr || output_tokens == nullptr
      || max_tokens == 0) {
    return 0;
  }

  size_t token_count = 0;
  char* cursor = input_line;

  while (*cursor != '\0' && token_count < max_tokens) {
    while (*cursor == ' ' || *cursor == '\t') {
      ++cursor;
    }

    if (*cursor == '\0') {
      break;
    }

    if (*cursor == '"') {
      ++cursor;
      output_tokens[token_count] = cursor;

      while (*cursor != '\0' && *cursor != '"') {
        ++cursor;
      }

      if (*cursor == '"') {
        *cursor = '\0';
        ++cursor;
      }

      ++token_count;
    } else {
      output_tokens[token_count] = cursor;

      while (*cursor != '\0' && *cursor != ' '
             && *cursor != '\t') {
        ++cursor;
      }

      if (*cursor != '\0') {
        *cursor = '\0';
        ++cursor;
      }

      ++token_count;
    }
  }

  return token_count;
}

#define CLI_MAX_PATH_DEPTH 64U

void cli_utils_normalize_fs_path(const char* input_path,
                                 char* output_buffer,
                                 const size_t buffer_size) {
  if (input_path == nullptr || output_buffer == nullptr
      || buffer_size == 0U) {
    return;
  }

  char temp_path[FS_MAX_PATH_LENGTH];
  strncpy(temp_path, input_path, sizeof(temp_path) - 1U);
  temp_path[sizeof(temp_path) - 1U] = '\0';

  char components[CLI_MAX_PATH_DEPTH]
                 [FS_MAX_FILENAME_LENGTH + 1U];
  size_t component_count = 0U;

  char* cursor = temp_path;

  while (*cursor != '\0') {
    while (*cursor == '/') {
      cursor++;
    }

    if (*cursor == '\0') {
      break;
    }

    char* start = cursor;

    while (*cursor != '\0' && *cursor != '/') {
      cursor++;
    }

    const size_t length = (size_t)(cursor - start);
    char component[FS_MAX_FILENAME_LENGTH + 1U];
    const size_t copy_length =
      (length < FS_MAX_FILENAME_LENGTH)
        ? length
        : FS_MAX_FILENAME_LENGTH;
    memcpy(component, start, copy_length);
    component[copy_length] = '\0';

    if (strcmp(component, ".") == 0) {
      continue;
    }

    if (strcmp(component, "..") == 0) {
      if (component_count > 0U) {
        component_count--;
      }

      continue;
    }

    if (component_count < CLI_MAX_PATH_DEPTH) {
      strncpy(components[component_count],
              component,
              FS_MAX_FILENAME_LENGTH);
      components[component_count][FS_MAX_FILENAME_LENGTH] =
        '\0';
      component_count++;
    }
  }

  if (component_count == 0U) {
    strncpy(output_buffer, "/", buffer_size - 1U);
    output_buffer[buffer_size - 1U] = '\0';
  } else {
    output_buffer[0] = '\0';

    for (size_t i = 0U; i < component_count; ++i) {
      const size_t current_length = strlen(output_buffer);
      (void)snprintf(output_buffer + current_length,
                     buffer_size - current_length,
                     "/%s",
                     components[i]);
    }
  }
}

fs_status_t cli_utils_ensure_parent_directories(
  fs_vfs_context_t* vfs_context, const char* file_path) {
  if (vfs_context == nullptr || file_path == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  strncpy(
    absolute_path, file_path, sizeof(absolute_path) - 1U);
  absolute_path[sizeof(absolute_path) - 1U] = '\0';

  char* last_slash = strrchr(absolute_path, '/');
  if (last_slash == nullptr
      || last_slash == absolute_path) {
    return FS_STATUS_OK;
  }

  *last_slash = '\0';

  char normalized_parent[FS_MAX_PATH_LENGTH];
  cli_utils_normalize_fs_path(absolute_path,
                              normalized_parent,
                              sizeof(normalized_parent));

  if (strcmp(normalized_parent, "/") == 0
      || normalized_parent[0] == '\0') {
    return FS_STATUS_OK;
  }

  uint32_t parent_inode_id = 0U;
  fs_status_t status = vfs_resolve_path(
    vfs_context, normalized_parent, &parent_inode_id);

  if (status == FS_STATUS_OK) {
    return FS_STATUS_OK;
  }

  char current_path[FS_MAX_PATH_LENGTH] = {0};
  const char* cursor = normalized_parent;

  if (*cursor == '/') {
    current_path[0] = '/';
    current_path[1] = '\0';
    cursor++;
  }

  while (*cursor != '\0') {
    const char* component_start = cursor;

    while (*cursor != '\0' && *cursor != '/') {
      cursor++;
    }

    const size_t component_length =
      (size_t)(cursor - component_start);

    if (component_length == 0U) {
      if (*cursor == '/') {
        cursor++;
      }

      continue;
    }

    char component[FS_MAX_FILENAME_LENGTH + 1U];
    memcpy(component, component_start, component_length);
    component[component_length] = '\0';

    if (strcmp(current_path, "/") == 0) {
      snprintf(current_path,
               sizeof(current_path),
               "/%s",
               component);
    } else {
      const size_t current_length = strlen(current_path);
      snprintf(current_path + current_length,
               sizeof(current_path) - current_length,
               "/%s",
               component);
    }

    uint32_t existing_inode_id = 0U;
    const fs_status_t resolve_status = vfs_resolve_path(
      vfs_context, current_path, &existing_inode_id);

    if (resolve_status == FS_STATUS_OK) {
      fs_inode_t inode_info = {0};
      const fs_status_t info_status = fs_vfs_get_info(
        vfs_context, current_path, &inode_info);

      if (info_status == FS_STATUS_OK
          && inode_info.type != FS_TYPE_DIRECTORY) {
        return FS_STATUS_ERROR_INVALID_ARGUMENT;
      }
    } else {
      const fs_status_t create_status =
        fs_vfs_create_directory(vfs_context, current_path);

      if (create_status != FS_STATUS_OK) {
        return create_status;
      }
    }

    if (*cursor == '/') {
      cursor++;
    }
  }

  return FS_STATUS_OK;
}