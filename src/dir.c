#include "fs/dir.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct fs_dir_context {
  fs_alloc_context_t* alloc_context;
  fs_index_t* index_context;
};

static void
fs_dir_free_dentry_array(fs_dentry_t** entry_array,
                         const uint32_t entry_count) {
  if (entry_array == nullptr) {
    return;
  }

  for (uint32_t i = 0; i < entry_count; ++i) {
    fs_dentry_destroy(entry_array[i]);
  }

  free(entry_array);
}

static int
compare_dentry_names_for_qsort(const void* first_pointer,
                               const void* second_pointer) {
  const fs_dentry_t* const* first_entry =
    (const fs_dentry_t* const*)first_pointer;
  const fs_dentry_t* const* second_entry =
    (const fs_dentry_t* const*)second_pointer;
  return strcmp((*first_entry)->name,
                (*second_entry)->name);
}

static int compare_dentry_names_for_bsearch(
  const void* target_pointer, const void* entry_pointer) {
  const char* target_name = (const char*)target_pointer;
  const fs_dentry_t* const* current_entry =
    (const fs_dentry_t* const*)entry_pointer;
  return strcmp(target_name, (*current_entry)->name);
}

static fs_status_t load_and_parse_directory_entries(
  fs_alloc_context_t* alloc_context,
  const fs_inode_t* directory_inode,
  fs_dentry_t*** output_entry_array,
  uint32_t* output_entry_count) {
  if (directory_inode->size == 0) {
    *output_entry_array = nullptr;
    *output_entry_count = 0;
    return FS_STATUS_OK;
  }

  uint8_t* directory_buffer =
    calloc(1, directory_inode->size);

  if (directory_buffer == nullptr) {
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  size_t bytes_read = 0;
  fs_status_t status =
    fs_alloc_read_data(alloc_context,
                       directory_inode,
                       0,
                       directory_buffer,
                       directory_inode->size,
                       &bytes_read);

  if (status != FS_STATUS_OK
      || bytes_read != directory_inode->size) {
    free(directory_buffer);
    return status != FS_STATUS_OK
             ? status
             : FS_STATUS_ERROR_FILE_ACCESS;
  }

  uint32_t entry_count = 0;
  size_t current_offset = 0;

  while (current_offset < directory_inode->size) {
    if (current_offset + sizeof(fs_dentry_t)
        > directory_inode->size) {
      break;
    }

    const fs_dentry_t* current_dentry =
      (const fs_dentry_t*)(directory_buffer
                           + current_offset);

    if (current_dentry->record_length == 0) {
      break;
    }
    current_offset += current_dentry->record_length;
    ++entry_count;
  }

  fs_dentry_t** entry_array =
    calloc(entry_count, sizeof(fs_dentry_t*));

  if (entry_array == nullptr) {
    free(directory_buffer);
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  current_offset = 0;

  for (uint32_t i = 0; i < entry_count; ++i) {
    const fs_dentry_t* current_dentry =
      (const fs_dentry_t*)(directory_buffer
                           + current_offset);
    fs_dentry_t* new_entry = fs_dentry_create(
      current_dentry->inode_id, current_dentry->name);

    if (new_entry == nullptr) {
      fs_dir_free_dentry_array(entry_array, i);
      free(directory_buffer);
      return FS_STATUS_ERROR_MEMORY_ALLOCATION;
    }

    entry_array[i] = new_entry;
    current_offset += current_dentry->record_length;
  }

  qsort(entry_array,
        entry_count,
        sizeof(fs_dentry_t*),
        compare_dentry_names_for_qsort);

  *output_entry_array = entry_array;
  *output_entry_count = entry_count;
  free(directory_buffer);
  return FS_STATUS_OK;
}

static fs_status_t
save_directory_entries(fs_alloc_context_t* alloc_context,
                       fs_index_t* index_context,
                       fs_inode_t* directory_inode,
                       fs_dentry_t* const* entry_array,
                       const uint32_t entry_count) {
  size_t total_buffer_size = 0;

  for (uint32_t i = 0; i < entry_count; ++i) {
    total_buffer_size += entry_array[i]->record_length;
  }

  uint8_t* write_buffer = calloc(1, total_buffer_size);

  if (write_buffer == nullptr) {
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  uint8_t* current_pointer = write_buffer;

  for (uint32_t i = 0; i < entry_count; ++i) {
    memcpy(current_pointer,
           entry_array[i],
           entry_array[i]->record_length);
    current_pointer += entry_array[i]->record_length;
  }

  size_t bytes_written = 0;
  fs_status_t status =
    fs_alloc_write_data(alloc_context,
                        directory_inode,
                        0,
                        write_buffer,
                        total_buffer_size,
                        &bytes_written);

  free(write_buffer);

  if (status != FS_STATUS_OK
      || bytes_written != total_buffer_size) {
    return status != FS_STATUS_OK
             ? status
             : FS_STATUS_ERROR_FILE_ACCESS;
  }

  directory_inode->size = (uint32_t)bytes_written;
  directory_inode->cluster_count =
    (uint32_t)((directory_inode->size
                + fs_disk_get_cluster_size() - 1)
               / fs_disk_get_cluster_size());

  return fs_index_write_inode(index_context,
                              directory_inode);
}

[[nodiscard]] fs_status_t
fs_dir_create_context(fs_dir_context_t** output_dir_context,
                      fs_alloc_context_t* alloc_context,
                      fs_index_t* index_context) {
  if (output_dir_context == nullptr
      || alloc_context == nullptr
      || index_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_dir_context_t* dir_context =
    calloc(1, sizeof(fs_dir_context_t));

  if (dir_context == nullptr) {
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  dir_context->alloc_context = alloc_context;
  dir_context->index_context = index_context;
  *output_dir_context = dir_context;

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_dir_destroy_context(fs_dir_context_t* dir_context) {
  if (dir_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  free(dir_context);
  return FS_STATUS_OK;
}

[[nodiscard]] static fs_status_t
fs_dir_validate_name(const char* name) {
  if (name == nullptr || name[0] == '\0') {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const size_t name_length = strlen(name);
  if (name_length > FS_MAX_FILENAME_LENGTH) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  for (size_t i = 0; i < name_length; ++i) {
    if (name[i] == '/' || name[i] == '\\') {
      return FS_STATUS_ERROR_INVALID_ARGUMENT;
    }
  }

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_dir_insert_entry(fs_dir_context_t* dir_context,
                    const uint32_t directory_inode_id,
                    const char* entry_name,
                    const uint32_t target_inode_id) {
  if (dir_context == nullptr || entry_name == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_status_t validation_status =
    fs_dir_validate_name(entry_name);
  if (validation_status != FS_STATUS_OK) {
    return validation_status;
  }

  fs_inode_t directory_inode = {0};
  fs_status_t status =
    fs_index_read_inode(dir_context->index_context,
                        directory_inode_id,
                        &directory_inode);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (directory_inode.type != FS_TYPE_DIRECTORY) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_dentry_t** entry_array = nullptr;
  uint32_t entry_count = 0;

  status = load_and_parse_directory_entries(
    dir_context->alloc_context,
    &directory_inode,
    &entry_array,
    &entry_count);

  if (status != FS_STATUS_OK) {
    return status;
  }

  for (uint32_t i = 0; i < entry_count; ++i) {
    if (strcmp(entry_array[i]->name, entry_name) == 0) {
      fs_dir_free_dentry_array(entry_array, entry_count);
      return FS_STATUS_ERROR_INVALID_ARGUMENT;
    }
  }

  uint32_t new_entry_count = entry_count + 1;
  fs_dentry_t** new_entry_array = realloc(
    entry_array, new_entry_count * sizeof(fs_dentry_t*));

  if (new_entry_array == nullptr) {
    fs_dir_free_dentry_array(entry_array, entry_count);
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  fs_dentry_t* new_dentry =
    fs_dentry_create(target_inode_id, entry_name);

  if (new_dentry == nullptr) {
    free(new_entry_array);
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  new_entry_array[entry_count] = new_dentry;
  qsort(new_entry_array,
        new_entry_count,
        sizeof(fs_dentry_t*),
        compare_dentry_names_for_qsort);

  status =
    save_directory_entries(dir_context->alloc_context,
                           dir_context->index_context,
                           &directory_inode,
                           new_entry_array,
                           new_entry_count);

  fs_dir_free_dentry_array(new_entry_array,
                           new_entry_count);
  return status;
}

[[nodiscard]] fs_status_t
fs_dir_remove_entry(fs_dir_context_t* dir_context,
                    const uint32_t directory_inode_id,
                    const char* entry_name) {
  if (dir_context == nullptr || entry_name == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (strcmp(entry_name, ".") == 0
      || strcmp(entry_name, "..") == 0) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_inode_t directory_inode = {0};
  fs_status_t status =
    fs_index_read_inode(dir_context->index_context,
                        directory_inode_id,
                        &directory_inode);

  if (status != FS_STATUS_OK) {
    return status;
  }

  fs_dentry_t** entry_array = nullptr;
  uint32_t entry_count = 0;

  status = load_and_parse_directory_entries(
    dir_context->alloc_context,
    &directory_inode,
    &entry_array,
    &entry_count);

  if (status != FS_STATUS_OK) {
    return status;
  }

  uint32_t found_index = entry_count;
  for (uint32_t i = 0; i < entry_count; ++i) {
    if (strcmp(entry_array[i]->name, entry_name) == 0) {
      found_index = i;
      break;
    }
  }

  if (found_index == entry_count) {
    fs_dir_free_dentry_array(entry_array, entry_count);
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  fs_dentry_destroy(entry_array[found_index]);

  for (uint32_t i = found_index; i < entry_count - 1; ++i) {
    entry_array[i] = entry_array[i + 1];
  }

  entry_array[entry_count - 1] = nullptr;

  const uint32_t new_entry_count = entry_count - 1;
  status =
    save_directory_entries(dir_context->alloc_context,
                           dir_context->index_context,
                           &directory_inode,
                           entry_array,
                           new_entry_count);

  fs_dir_free_dentry_array(entry_array, entry_count);
  return status;
}

[[nodiscard]] fs_status_t
fs_dir_find_entry(const fs_dir_context_t* dir_context,
                  const uint32_t directory_inode_id,
                  const char* entry_name,
                  uint32_t* output_inode_id) {
  if (dir_context == nullptr || entry_name == nullptr
      || output_inode_id == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_inode_t directory_inode = {0};
  fs_status_t status =
    fs_index_read_inode(dir_context->index_context,
                        directory_inode_id,
                        &directory_inode);

  if (status != FS_STATUS_OK) {
    return status;
  }

  fs_dentry_t** entry_array = nullptr;
  uint32_t entry_count = 0;

  status = load_and_parse_directory_entries(
    dir_context->alloc_context,
    &directory_inode,
    &entry_array,
    &entry_count);

  if (status != FS_STATUS_OK) {
    return status;
  }

  fs_dentry_t** found_entry =
    bsearch(entry_name,
            entry_array,
            entry_count,
            sizeof(fs_dentry_t*),
            compare_dentry_names_for_bsearch);

  if (found_entry != nullptr) {
    *output_inode_id = (*found_entry)->inode_id;
    fs_dir_free_dentry_array(entry_array, entry_count);
    return FS_STATUS_OK;
  }

  fs_dir_free_dentry_array(entry_array, entry_count);
  return FS_STATUS_ERROR_OUT_OF_BOUNDS;
}

[[nodiscard]] fs_status_t
fs_dir_list_entries(const fs_dir_context_t* dir_context,
                    const uint32_t directory_inode_id,
                    fs_dir_list_callback_t callback,
                    void* user_data) {
  if (dir_context == nullptr || callback == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_inode_t directory_inode = {0};
  fs_status_t status =
    fs_index_read_inode(dir_context->index_context,
                        directory_inode_id,
                        &directory_inode);

  if (status != FS_STATUS_OK) {
    return status;
  }

  fs_dentry_t** entry_array = nullptr;
  uint32_t entry_count = 0;

  status = load_and_parse_directory_entries(
    dir_context->alloc_context,
    &directory_inode,
    &entry_array,
    &entry_count);

  if (status != FS_STATUS_OK) {
    return status;
  }

  for (uint32_t i = 0; i < entry_count; ++i) {
    fs_inode_t child_inode = {0};
    fs_status_t child_status =
      fs_index_read_inode(dir_context->index_context,
                          entry_array[i]->inode_id,
                          &child_inode);

    if (child_status == FS_STATUS_OK) {
      callback(entry_array[i]->name,
               entry_array[i]->inode_id,
               child_inode.type,
               user_data);
    }
  }

  fs_dir_free_dentry_array(entry_array, entry_count);
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t fs_dir_create_new(
  fs_dir_context_t* dir_context,
  const uint32_t new_directory_inode_id,
  const uint32_t parent_directory_inode_id) {
  if (dir_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_inode_t new_dir_inode = {0};
  fs_status_t status =
    fs_index_read_inode(dir_context->index_context,
                        new_directory_inode_id,
                        &new_dir_inode);

  if (status != FS_STATUS_OK) {
    return status;
  }

  if (new_dir_inode.type != FS_TYPE_DIRECTORY) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_dentry_t* dot_entry =
    fs_dentry_create(new_directory_inode_id, ".");
  fs_dentry_t* dot_dot_entry =
    fs_dentry_create(parent_directory_inode_id, "..");

  if (dot_entry == nullptr || dot_dot_entry == nullptr) {
    fs_dentry_destroy(dot_entry);
    fs_dentry_destroy(dot_dot_entry);
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  const size_t total_size =
    dot_entry->record_length + dot_dot_entry->record_length;
  uint8_t* buffer = calloc(1, total_size);

  if (buffer == nullptr) {
    fs_dentry_destroy(dot_entry);
    fs_dentry_destroy(dot_dot_entry);
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  uint8_t* current_pointer = buffer;
  memcpy(
    current_pointer, dot_entry, dot_entry->record_length);
  current_pointer += dot_entry->record_length;
  memcpy(current_pointer,
         dot_dot_entry,
         dot_dot_entry->record_length);

  size_t bytes_written = 0;
  status = fs_alloc_write_data(dir_context->alloc_context,
                               &new_dir_inode,
                               0,
                               buffer,
                               total_size,
                               &bytes_written);

  free(buffer);
  fs_dentry_destroy(dot_entry);
  fs_dentry_destroy(dot_dot_entry);

  if (status != FS_STATUS_OK) {
    return status;
  }

  return fs_index_write_inode(dir_context->index_context,
                              &new_dir_inode);
}

[[nodiscard]] fs_status_t
fs_dir_rename_entry(fs_dir_context_t* dir_context,
                    const uint32_t directory_inode_id,
                    const char* old_name,
                    const char* new_name) {
  if (dir_context == nullptr || old_name == nullptr
      || new_name == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (strcmp(old_name, ".") == 0
      || strcmp(old_name, "..") == 0) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_status_t validation_status =
    fs_dir_validate_name(new_name);
  if (validation_status != FS_STATUS_OK) {
    return validation_status;
  }

  fs_inode_t directory_inode = {0};
  fs_status_t status =
    fs_index_read_inode(dir_context->index_context,
                        directory_inode_id,
                        &directory_inode);

  if (status != FS_STATUS_OK) {
    return status;
  }

  fs_dentry_t** entry_array = nullptr;
  uint32_t entry_count = 0;

  status = load_and_parse_directory_entries(
    dir_context->alloc_context,
    &directory_inode,
    &entry_array,
    &entry_count);

  if (status != FS_STATUS_OK) {
    return status;
  }

  int32_t target_index = -1;
  for (uint32_t i = 0; i < entry_count; ++i) {
    if (strcmp(entry_array[i]->name, old_name) == 0) {
      target_index = (int32_t)i;
      break;
    }
  }

  if (target_index == -1) {
    fs_dir_free_dentry_array(entry_array, entry_count);
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  for (uint32_t i = 0; i < entry_count; ++i) {
    if (strcmp(entry_array[i]->name, new_name) == 0) {
      fs_dir_free_dentry_array(entry_array, entry_count);
      return FS_STATUS_ERROR_INVALID_ARGUMENT;
    }
  }

  const uint32_t target_inode_id =
    entry_array[target_index]->inode_id;
  fs_dentry_destroy(entry_array[target_index]);

  fs_dentry_t* new_dentry =
    fs_dentry_create(target_inode_id, new_name);
  if (new_dentry == nullptr) {
    fs_dir_free_dentry_array(entry_array, entry_count);
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  entry_array[target_index] = new_dentry;
  qsort(entry_array,
        entry_count,
        sizeof(fs_dentry_t*),
        compare_dentry_names_for_qsort);

  status =
    save_directory_entries(dir_context->alloc_context,
                           dir_context->index_context,
                           &directory_inode,
                           entry_array,
                           entry_count);

  fs_dir_free_dentry_array(entry_array, entry_count);
  return status;
}

[[nodiscard]] fs_status_t
fs_dir_is_empty(const fs_dir_context_t* dir_context,
                const uint32_t directory_inode_id,
                bool* output_is_empty) {
  if (dir_context == nullptr
      || output_is_empty == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_inode_t directory_inode = {0};
  fs_status_t status =
    fs_index_read_inode(dir_context->index_context,
                        directory_inode_id,
                        &directory_inode);

  if (status != FS_STATUS_OK) {
    return status;
  }

  fs_dentry_t** entry_array = nullptr;
  uint32_t entry_count = 0;

  status = load_and_parse_directory_entries(
    dir_context->alloc_context,
    &directory_inode,
    &entry_array,
    &entry_count);

  if (status != FS_STATUS_OK) {
    return status;
  }

  *output_is_empty = (entry_count <= 2);
  fs_dir_free_dentry_array(entry_array, entry_count);
  return FS_STATUS_OK;
}