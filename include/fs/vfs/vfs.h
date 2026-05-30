#pragma once

#include "fs/alloc/alloc.h"
#include "fs/logical/dir.h"
#include "fs/metadata/index.h"
#include <stdint.h>

#define FS_MAX_OPEN_FILES 64U
#define FS_MAX_PATH_LENGTH 1024U

#define FS_OPEN_READ_ONLY (1U << 0)
#define FS_OPEN_WRITE_ONLY (1U << 1)
#define FS_OPEN_READ_WRITE                                 \
  (FS_OPEN_READ_ONLY | FS_OPEN_WRITE_ONLY)
#define FS_OPEN_APPEND (1U << 2)
#define FS_OPEN_CREATE (1U << 3)
#define FS_OPEN_TRUNCATE (1U << 4)

typedef struct fs_vfs_context fs_vfs_context_t;

[[nodiscard]] fs_status_t
fs_vfs_create_context(fs_vfs_context_t** output_vfs_context,
                      fs_alloc_context_t* alloc_context,
                      fs_dir_context_t* dir_context,
                      fs_index_t* index_context,
                      uint32_t root_inode_id);

[[nodiscard]] fs_status_t
fs_vfs_destroy_context(fs_vfs_context_t* vfs_context);

[[nodiscard]] fs_status_t
fs_vfs_open(fs_vfs_context_t* vfs_context,
            const char* file_path,
            uint32_t open_flags,
            int32_t* output_file_descriptor);

[[nodiscard]] fs_status_t
fs_vfs_close(fs_vfs_context_t* vfs_context,
             int32_t file_descriptor);

[[nodiscard]] fs_status_t
fs_vfs_seek(fs_vfs_context_t* vfs_context,
            int32_t file_descriptor,
            int64_t offset,
            int origin);

[[nodiscard]] fs_status_t
fs_vfs_get_info(fs_vfs_context_t* vfs_context,
                const char* file_path,
                fs_inode_t* output_inode_info);

[[nodiscard]] fs_status_t
fs_vfs_read(fs_vfs_context_t* vfs_context,
            int32_t file_descriptor,
            void* destination_buffer,
            size_t bytes_to_read,
            size_t* bytes_actually_read);

[[nodiscard]] fs_status_t
fs_vfs_write(fs_vfs_context_t* vfs_context,
             int32_t file_descriptor,
             const void* source_buffer,
             size_t bytes_to_write,
             size_t* bytes_actually_written);

[[nodiscard]] fs_status_t
fs_vfs_remove(fs_vfs_context_t* vfs_context,
              const char* file_path);

[[nodiscard]] fs_status_t
fs_vfs_rename(fs_vfs_context_t* vfs_context,
              const char* old_path,
              const char* new_path);

[[nodiscard]] fs_status_t
fs_vfs_create_directory(fs_vfs_context_t* vfs_context,
                        const char* directory_path);

[[nodiscard]] fs_status_t
fs_vfs_remove_directory(fs_vfs_context_t* vfs_context,
                        const char* directory_path);