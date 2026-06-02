#pragma once

#include "fs/vfs/vfs.h"
#include <stdint.h>

void cli_utils_build_absolute_path(
  const char* current_directory,
  const char* relative_path,
  char* output_buffer,
  const size_t buffer_size);

void cli_utils_normalize_fs_path(const char* input_path,
                                 char* output_buffer,
                                 const size_t buffer_size);

void cli_utils_get_current_host_directory(
  char* output_buffer, const size_t buffer_size);

bool cli_utils_file_exists(const char* file_path);

void cli_utils_extract_disk_name_from_path(
  const char* disk_path,
  char* output_name,
  const size_t name_buffer_size);

size_t cli_utils_tokenize_line(char* input_line,
                               char** output_tokens,
                               const size_t max_tokens);

[[nodiscard]] fs_status_t
cli_utils_ensure_parent_directories(
  fs_vfs_context_t* vfs_context, const char* file_path);