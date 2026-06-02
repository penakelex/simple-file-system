#include "context.h"
#include "fs/storage/format.h"
#include "fs/storage/superblock.h"
#include <stdio.h>
#include <string.h>

fs_status_t
cli_create_disk(const char* disk_path,
                const uint32_t total_cluster_count) {
  if (disk_path == nullptr || total_cluster_count == 0U) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_disk_t* disk_context = nullptr;
  fs_status_t status = fs_disk_create_or_open(
    &disk_context, disk_path, total_cluster_count, false);

  if (status != FS_STATUS_OK) {
    return status;
  }

  status = fs_format_initialize_disk(disk_context,
                                     total_cluster_count);
  (void)fs_disk_close(disk_context);
  return status;
}

fs_status_t
cli_format_disk(const char* disk_path,
                const uint32_t total_cluster_count) {
  if (disk_path == nullptr || total_cluster_count == 0U) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_disk_t* disk_context = nullptr;
  fs_status_t status = fs_disk_create_or_open(
    &disk_context, disk_path, total_cluster_count, false);

  if (status != FS_STATUS_OK) {
    return status;
  }

  status = fs_format_initialize_disk(disk_context,
                                     total_cluster_count);
  (void)fs_disk_close(disk_context);
  return status;
}

fs_status_t cli_delete_disk(const char* disk_path) {
  if (disk_path == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (remove(disk_path) != 0) {
    return FS_STATUS_ERROR_FILE_ACCESS;
  }

  return FS_STATUS_OK;
}

fs_status_t cli_mount_disk(const char* disk_path,
                           cli_context_t* context) {
  if (disk_path == nullptr || context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  memset(context, 0, sizeof(cli_context_t));

  fs_disk_t* temp_disk = nullptr;
  fs_status_t status =
    fs_disk_create_or_open(&temp_disk, disk_path, 1U, true);

  if (status != FS_STATUS_OK) {
    return status;
  }

  fs_superblock_t superblock = {0};
  status =
    fs_superblock_read_from_disk(temp_disk, &superblock);
  (void)fs_disk_close(temp_disk);

  if (status != FS_STATUS_OK) {
    return status;
  }

  status = fs_disk_create_or_open(&context->disk_context,
                                  disk_path,
                                  superblock.total_clusters,
                                  false);

  if (status != FS_STATUS_OK) {
    return status;
  }

  status = fs_bitmap_create(&context->bitmap_context,
                            superblock.total_clusters);

  if (status != FS_STATUS_OK) {
    goto cleanup;
  }

  status = fs_bitmap_deserialize_from_disk(
    context->bitmap_context, context->disk_context);

  if (status != FS_STATUS_OK) {
    goto cleanup;
  }

  status = fs_index_create(&context->index_context,
                           context->disk_context,
                           &superblock);

  if (status != FS_STATUS_OK) {
    goto cleanup;
  }

  status = fs_index_load_from_disk(context->index_context);

  if (status != FS_STATUS_OK) {
    goto cleanup;
  }

  status = fs_alloc_create_context(&context->alloc_context,
                                   context->disk_context,
                                   context->bitmap_context,
                                   context->index_context);

  if (status != FS_STATUS_OK) {
    goto cleanup;
  }

  status = fs_dir_create_context(&context->dir_context,
                                 context->alloc_context,
                                 context->index_context);

  if (status != FS_STATUS_OK) {
    goto cleanup;
  }

  context->root_inode_id = superblock.root_inode_id;
  status = fs_vfs_create_context(&context->vfs_context,
                                 context->alloc_context,
                                 context->dir_context,
                                 context->index_context,
                                 context->root_inode_id);

  if (status != FS_STATUS_OK) {
    goto cleanup;
  }

  return FS_STATUS_OK;

cleanup:
  (void)cli_unmount_disk(context);
  return status;
}

fs_status_t cli_unmount_disk(cli_context_t* context) {
  if (context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (context->vfs_context != nullptr) {
    (void)fs_vfs_destroy_context(context->vfs_context);
    context->vfs_context = nullptr;
  }

  if (context->dir_context != nullptr) {
    (void)fs_dir_destroy_context(context->dir_context);
    context->dir_context = nullptr;
  }

  if (context->alloc_context != nullptr) {
    (void)fs_alloc_destroy_context(context->alloc_context);
    context->alloc_context = nullptr;
  }

  if (context->index_context != nullptr) {
    (void)fs_index_flush(context->index_context);
    (void)fs_index_destroy(context->index_context);
    context->index_context = nullptr;
  }

  if (context->bitmap_context != nullptr
      && context->disk_context != nullptr) {
    (void)fs_bitmap_serialize_to_disk(
      context->bitmap_context, context->disk_context);
    (void)fs_bitmap_destroy(context->bitmap_context);
    context->bitmap_context = nullptr;
  }

  if (context->disk_context != nullptr) {
    (void)fs_disk_flush(context->disk_context);
    (void)fs_disk_close(context->disk_context);
    context->disk_context = nullptr;
  }

  return FS_STATUS_OK;
}