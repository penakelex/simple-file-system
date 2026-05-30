#pragma once

#include "fs/alloc/alloc.h"
#include "fs/metadata/index.h"
#include "fs/types.h"
#include <stdint.h>

typedef struct fs_dir_context fs_dir_context_t;

typedef void (*fs_dir_list_callback_t)(
  const char* entry_name,
  const uint32_t entry_inode_id,
  const fs_file_type_t entry_type,
  void* user_data);

[[nodiscard]] fs_status_t
fs_dir_create_context(fs_dir_context_t** output_dir_context,
                      fs_alloc_context_t* alloc_context,
                      fs_index_t* index_context);

[[nodiscard]] fs_status_t
fs_dir_destroy_context(fs_dir_context_t* dir_context);

[[nodiscard]] fs_status_t
fs_dir_insert_entry(fs_dir_context_t* dir_context,
                    const uint32_t directory_inode_id,
                    const char* entry_name,
                    const uint32_t target_inode_id);

[[nodiscard]] fs_status_t
fs_dir_remove_entry(fs_dir_context_t* dir_context,
                    const uint32_t directory_inode_id,
                    const char* entry_name);

[[nodiscard]] fs_status_t
fs_dir_find_entry(const fs_dir_context_t* dir_context,
                  const uint32_t directory_inode_id,
                  const char* entry_name,
                  uint32_t* output_inode_id);

[[nodiscard]] fs_status_t
fs_dir_list_entries(const fs_dir_context_t* dir_context,
                    const uint32_t directory_inode_id,
                    fs_dir_list_callback_t callback,
                    void* user_data);

[[nodiscard]] fs_status_t
fs_dir_create_new(fs_dir_context_t* dir_context,
                  const uint32_t new_directory_inode_id,
                  const uint32_t parent_directory_inode_id);

[[nodiscard]] fs_status_t
fs_dir_rename_entry(fs_dir_context_t* dir_context,
                    const uint32_t directory_inode_id,
                    const char* old_name,
                    const char* new_name);

[[nodiscard]] fs_status_t
fs_dir_is_empty(const fs_dir_context_t* dir_context,
                const uint32_t directory_inode_id,
                bool* output_is_empty);