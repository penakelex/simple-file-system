#pragma once
#include "fs/storage/disk.h"
#include "fs/types.h"
#include <stdint.h>

typedef struct fs_index fs_index_t;

[[nodiscard]] fs_status_t
fs_index_create(fs_index_t** output_index_context,
                fs_disk_t* disk_context,
                const fs_superblock_t* superblock);

[[nodiscard]] fs_status_t
fs_index_destroy(fs_index_t* index_context);

[[nodiscard]] fs_status_t
fs_index_load_from_disk(fs_index_t* index_context);

[[nodiscard]] fs_status_t
fs_index_read_inode(const fs_index_t* index_context,
                    const uint32_t inode_id,
                    fs_inode_t* output_inode);

[[nodiscard]] fs_status_t
fs_index_write_inode(fs_index_t* index_context,
                     const fs_inode_t* inode);

[[nodiscard]] fs_status_t
fs_index_allocate_inode(fs_index_t* index_context,
                        const fs_file_type_t type,
                        uint32_t* output_inode_id);

[[nodiscard]] fs_status_t
fs_index_free_inode(fs_index_t* index_context,
                    const uint32_t inode_id);

[[nodiscard]] fs_status_t
fs_index_flush(fs_index_t* index_context);