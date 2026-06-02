#include "fs/storage/superblock.h"
#include <string.h>

[[nodiscard]] fs_status_t
fs_superblock_initialize(fs_superblock_t* output_superblock,
                         uint32_t total_cluster_count) {
  if (output_superblock == nullptr
      || total_cluster_count == 0U) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  output_superblock->magic = FS_MAGIC_NUMBER;
  output_superblock->total_clusters = total_cluster_count;
  output_superblock->root_inode_id = 0;
  output_superblock->inode_table_start_cluster = 2;

  const size_t total_inode_bytes =
    (size_t)FS_INODE_COUNT * sizeof(fs_inode_t);
  const uint32_t cluster_size = fs_disk_get_cluster_size();
  output_superblock->inode_table_size_clusters =
    (uint32_t)((total_inode_bytes + cluster_size - 1)
               / cluster_size);

  output_superblock->data_start_cluster =
    output_superblock->inode_table_start_cluster
    + output_superblock->inode_table_size_clusters;

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t fs_superblock_validate(
  const fs_superblock_t* superblock_context) {
  if (superblock_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (superblock_context->magic != FS_MAGIC_NUMBER) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t fs_superblock_read_from_disk(
  const fs_disk_t* disk_context,
  fs_superblock_t* output_superblock) {
  if (disk_context == nullptr
      || output_superblock == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  uint8_t cluster_buffer[FS_CLUSTER_SIZE] = {0};
  const fs_status_t read_status =
    fs_disk_read_cluster(disk_context, 0, cluster_buffer);
  if (read_status != FS_STATUS_OK) {
    return read_status;
  }

  memcpy(output_superblock,
         cluster_buffer,
         sizeof(fs_superblock_t));
  return fs_superblock_validate(output_superblock);
}

[[nodiscard]] fs_status_t fs_superblock_write_to_disk(
  fs_disk_t* disk_context,
  const fs_superblock_t* superblock_context) {
  if (disk_context == nullptr
      || superblock_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  uint8_t cluster_buffer[FS_CLUSTER_SIZE] = {0};
  memcpy(cluster_buffer,
         superblock_context,
         sizeof(fs_superblock_t));

  return fs_disk_write_cluster(
    disk_context, 0, cluster_buffer);
}