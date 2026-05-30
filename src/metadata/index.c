#include "fs/metadata/index.h"
#include <stdlib.h>
#include <string.h>

struct fs_index {
  fs_disk_t* disk_context;
  fs_inode_t* inode_table;
  uint32_t total_inodes;
  uint32_t start_cluster;
  uint32_t table_size_clusters;
  bool is_dirty;
};

[[nodiscard]] fs_status_t
fs_index_create(fs_index_t** output_index_context,
                fs_disk_t* disk_context,
                const fs_superblock_t* superblock) {
  if (output_index_context == nullptr
      || disk_context == nullptr || superblock == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_index_t* index_context = calloc(1, sizeof(fs_index_t));

  if (index_context == nullptr) {
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  index_context->disk_context = disk_context;
  index_context->total_inodes = FS_INODE_COUNT;
  index_context->start_cluster =
    superblock->inode_table_start_cluster;
  index_context->table_size_clusters =
    superblock->inode_table_size_clusters;
  index_context->is_dirty = false;

  index_context->inode_table =
    calloc(index_context->total_inodes, sizeof(fs_inode_t));

  if (index_context->inode_table == nullptr) {
    free(index_context);
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  *output_index_context = index_context;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_index_destroy(fs_index_t* index_context) {
  if (index_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (index_context->inode_table != nullptr) {
    free(index_context->inode_table);
  }

  free(index_context);
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_index_load_from_disk(fs_index_t* index_context) {
  if (index_context == nullptr
      || index_context->disk_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  uint8_t* current_buffer =
    (uint8_t*)index_context->inode_table;
  size_t remaining_bytes =
    (size_t)index_context->total_inodes
    * sizeof(fs_inode_t);
  uint32_t current_cluster = index_context->start_cluster;
  const uint32_t cluster_size = fs_disk_get_cluster_size();

  while (remaining_bytes > 0) {
    const size_t chunk_size = remaining_bytes > cluster_size
                                ? cluster_size
                                : remaining_bytes;

    uint8_t cluster_buffer[FS_CLUSTER_SIZE] = {0};
    const fs_status_t read_status =
      fs_disk_read_cluster(index_context->disk_context,
                           current_cluster,
                           cluster_buffer);
    if (read_status != FS_STATUS_OK) {
      return read_status;
    }

    memcpy(current_buffer, cluster_buffer, chunk_size);
    current_buffer += chunk_size;
    remaining_bytes -= chunk_size;
    ++current_cluster;
  }

  index_context->is_dirty = false;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_index_read_inode(const fs_index_t* index_context,
                    const uint32_t inode_id,
                    fs_inode_t* output_inode) {
  if (index_context == nullptr || output_inode == nullptr
      || inode_id >= index_context->total_inodes) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  memcpy(output_inode,
         &index_context->inode_table[inode_id],
         sizeof(fs_inode_t));
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_index_write_inode(fs_index_t* index_context,
                     const fs_inode_t* inode) {
  if (index_context == nullptr || inode == nullptr
      || inode->id >= index_context->total_inodes) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  memcpy(&index_context->inode_table[inode->id],
         inode,
         sizeof(fs_inode_t));
  index_context->is_dirty = true;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_index_allocate_inode(fs_index_t* index_context,
                        fs_file_type_t type,
                        uint32_t* output_inode_id) {
  if (index_context == nullptr
      || output_inode_id == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  for (uint32_t i = 0; i < index_context->total_inodes;
       ++i) {
    if (!index_context->inode_table[i].is_used) {
      fs_inode_t new_inode = {0};
      new_inode.id = i;
      new_inode.type = type;
      new_inode.is_used = true;
      new_inode.link_count = 1U;
      new_inode.size = 0;
      new_inode.cluster_count = 0;

      const fs_status_t write_status =
        fs_index_write_inode(index_context, &new_inode);

      if (write_status != FS_STATUS_OK) {
        return write_status;
      }

      *output_inode_id = i;
      return FS_STATUS_OK;
    }
  }

  return FS_STATUS_ERROR_OUT_OF_BOUNDS;
}

[[nodiscard]] fs_status_t
fs_index_free_inode(fs_index_t* index_context,
                    const uint32_t inode_id) {
  if (index_context == nullptr
      || inode_id >= index_context->total_inodes) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_inode_t empty_inode = {0};
  empty_inode.id = inode_id;

  return fs_index_write_inode(index_context, &empty_inode);
}

[[nodiscard]] fs_status_t
fs_index_flush(fs_index_t* index_context) {
  if (index_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (!index_context->is_dirty) {
    return FS_STATUS_OK;
  }

  const uint8_t* current_buffer =
    (const uint8_t*)index_context->inode_table;
  size_t remaining_bytes =
    (size_t)index_context->total_inodes
    * sizeof(fs_inode_t);
  uint32_t current_cluster = index_context->start_cluster;
  const uint32_t cluster_size = fs_disk_get_cluster_size();

  while (remaining_bytes > 0) {
    const size_t chunk_size = remaining_bytes > cluster_size
                                ? cluster_size
                                : remaining_bytes;

    uint8_t cluster_buffer[FS_CLUSTER_SIZE] = {0};
    memcpy(cluster_buffer, current_buffer, chunk_size);

    const fs_status_t write_status =
      fs_disk_write_cluster(index_context->disk_context,
                            current_cluster,
                            cluster_buffer);

    if (write_status != FS_STATUS_OK) {
      return write_status;
    }

    current_buffer += chunk_size;
    remaining_bytes -= chunk_size;
    ++current_cluster;
  }

  index_context->is_dirty = false;
  return FS_STATUS_OK;
}