#include "fs/storage/format.h"
#include "fs/alloc/alloc.h"
#include "fs/logical/dir.h"
#include "fs/metadata/index.h"
#include "fs/space/bitmap.h"
#include "fs/storage/superblock.h"
#include "fs/types.h"
#include <stdlib.h>

struct fs_format_context {
  fs_superblock_t superblock;
  fs_bitmap_t* bitmap_context;
  fs_index_t* index_context;
  bool is_mounted;
};

[[nodiscard]] fs_status_t fs_format_create_context(
  fs_format_context_t** output_format_context) {
  if (output_format_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_format_context_t* context =
    calloc(1, sizeof(fs_format_context_t));

  if (context == nullptr) {
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  context->is_mounted = false;
  *output_format_context = context;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t fs_format_destroy_context(
  fs_format_context_t* format_context) {
  if (format_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (format_context->index_context != nullptr) {
    if (format_context->is_mounted) {
      (void)fs_index_flush(format_context->index_context);
    }

    (void)fs_index_destroy(format_context->index_context);
  }

  if (format_context->bitmap_context != nullptr) {
    (void)fs_bitmap_destroy(format_context->bitmap_context);
  }

  free(format_context);
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_format_initialize_disk(fs_disk_t* disk_context,
                          uint32_t total_cluster_count) {
  if (disk_context == nullptr || total_cluster_count == 0U) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_superblock_t superblock = {0};
  fs_status_t status = fs_superblock_initialize(
    &superblock, total_cluster_count);
  if (status != FS_STATUS_OK)
    return status;

  status =
    fs_superblock_write_to_disk(disk_context, &superblock);
  if (status != FS_STATUS_OK)
    return status;

  fs_bitmap_t* bitmap_context = nullptr;
  status =
    fs_bitmap_create(&bitmap_context, total_cluster_count);
  if (status != FS_STATUS_OK)
    return status;

  status = fs_bitmap_mark_cluster_used(
    bitmap_context, FS_SUPERBLOCK_CLUSTER_INDEX);
  if (status != FS_STATUS_OK) {
    (void)fs_bitmap_destroy(bitmap_context);
    return status;
  }

  const size_t bitmap_byte_length =
    fs_bitmap_get_byte_length(bitmap_context);
  const uint32_t cluster_size = fs_disk_get_cluster_size();
  const uint32_t bitmap_cluster_count =
    (uint32_t)((bitmap_byte_length + cluster_size - 1)
               / cluster_size);
  for (uint32_t cluster_index =
         FS_BITMAP_START_CLUSTER_INDEX;
       cluster_index < FS_BITMAP_START_CLUSTER_INDEX
                         + bitmap_cluster_count;
       ++cluster_index) {
    status = fs_bitmap_mark_cluster_used(bitmap_context,
                                         cluster_index);
    if (status != FS_STATUS_OK) {
      (void)fs_bitmap_destroy(bitmap_context);
      return status;
    }
  }

  for (uint32_t cluster_index =
         superblock.inode_table_start_cluster;
       cluster_index
       < superblock.inode_table_start_cluster
           + superblock.inode_table_size_clusters;
       ++cluster_index) {
    status = fs_bitmap_mark_cluster_used(bitmap_context,
                                         cluster_index);
    if (status != FS_STATUS_OK) {
      (void)fs_bitmap_destroy(bitmap_context);
      return status;
    }
  }

  fs_index_t* index_context = nullptr;
  status = fs_index_create(
    &index_context, disk_context, &superblock);
  if (status != FS_STATUS_OK) {
    (void)fs_bitmap_destroy(bitmap_context);
    return status;
  }

  fs_inode_t bitmap_inode = {.id = FS_BITMAP_INODE_ID,
                             .type = FS_TYPE_REGULAR,
                             .is_used = true};
  status =
    fs_index_write_inode(index_context, &bitmap_inode);
  if (status != FS_STATUS_OK)
    goto cleanup_index;

  fs_inode_t index_inode = {.id = FS_INDEX_TABLE_INODE_ID,
                            .type = FS_TYPE_REGULAR,
                            .is_used = true};
  status =
    fs_index_write_inode(index_context, &index_inode);
  if (status != FS_STATUS_OK)
    goto cleanup_index;

  uint32_t root_inode_id = 0;
  status = fs_index_allocate_inode(
    index_context, FS_TYPE_DIRECTORY, &root_inode_id);
  if (status != FS_STATUS_OK)
    goto cleanup_index;
  superblock.root_inode_id = root_inode_id;

  fs_alloc_context_t* alloc_context = nullptr;
  status = fs_alloc_create_context(&alloc_context,
                                   disk_context,
                                   bitmap_context,
                                   index_context);
  if (status != FS_STATUS_OK)
    goto cleanup_index;

  fs_dir_context_t* dir_context = nullptr;
  status = fs_dir_create_context(
    &dir_context, alloc_context, index_context);
  if (status != FS_STATUS_OK) {
    (void)fs_alloc_destroy_context(alloc_context);
    goto cleanup_index;
  }

  status = fs_dir_create_new(
    dir_context, root_inode_id, root_inode_id);
  (void)fs_dir_destroy_context(dir_context);
  (void)fs_alloc_destroy_context(alloc_context);
  if (status != FS_STATUS_OK)
    goto cleanup_index;

  status =
    fs_superblock_write_to_disk(disk_context, &superblock);
  if (status != FS_STATUS_OK)
    goto cleanup_index;

  status = fs_index_flush(index_context);
  if (status != FS_STATUS_OK)
    goto cleanup_index;

  status = fs_bitmap_serialize_to_disk(bitmap_context,
                                       disk_context);
  if (status != FS_STATUS_OK)
    goto cleanup_index;

  (void)fs_index_destroy(index_context);
  (void)fs_bitmap_destroy(bitmap_context);
  return fs_disk_flush(disk_context);

cleanup_index:
  (void)fs_index_destroy(index_context);
  (void)fs_bitmap_destroy(bitmap_context);
  return status;
}

[[nodiscard]] fs_status_t
fs_format_mount_disk(fs_disk_t* disk_context,
                     fs_format_context_t* format_context) {
  if (disk_context == nullptr
      || format_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (format_context->is_mounted) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_status_t status = fs_superblock_read_from_disk(
    disk_context, &format_context->superblock);

  if (status != FS_STATUS_OK) {
    return status;
  }

  format_context->index_context = nullptr;
  status = fs_index_create(&format_context->index_context,
                           disk_context,
                           &format_context->superblock);

  if (status != FS_STATUS_OK) {
    return status;
  }

  status =
    fs_index_load_from_disk(format_context->index_context);

  if (status != FS_STATUS_OK) {
    (void)fs_index_destroy(format_context->index_context);
    format_context->index_context = nullptr;
    return status;
  }

  format_context->bitmap_context = nullptr;
  status = fs_bitmap_create(
    &format_context->bitmap_context,
    format_context->superblock.total_clusters);

  if (status != FS_STATUS_OK) {
    (void)fs_index_destroy(format_context->index_context);
    format_context->index_context = nullptr;
    return status;
  }

  status = fs_bitmap_deserialize_from_disk(
    format_context->bitmap_context, disk_context);

  if (status != FS_STATUS_OK) {
    (void)fs_index_destroy(format_context->index_context);
    format_context->index_context = nullptr;
    (void)fs_bitmap_destroy(format_context->bitmap_context);
    format_context->bitmap_context = nullptr;
    return status;
  }

  format_context->is_mounted = true;
  return FS_STATUS_OK;
}

bool fs_format_is_mounted(
  const fs_format_context_t* format_context) {
  return format_context != nullptr
         && format_context->is_mounted;
}