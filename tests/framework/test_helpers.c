#include "framework/test_helpers.h"
#include <stdio.h>

void test_cleanup_disk_file(const char* file_path) {
  if (file_path != nullptr) {
    remove(file_path);
  }
}

fs_status_t test_setup_full_environment(
  test_environment_t* environment) {
  if (environment == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  memset(environment, 0, sizeof(test_environment_t));
  test_cleanup_disk_file(TEST_DISK_PATH);

  fs_status_t status =
    fs_disk_create_or_open(&environment->disk_context,
                           TEST_DISK_PATH,
                           TEST_CLUSTER_COUNT,
                           false);

  if (status != FS_STATUS_OK) {
    return status;
  }

  status = fs_format_initialize_disk(
    environment->disk_context, TEST_CLUSTER_COUNT);

  if (status != FS_STATUS_OK) {
    goto cleanup_disk;
  }

  fs_superblock_t superblock = {0};
  status = fs_superblock_read_from_disk(
    environment->disk_context, &superblock);

  if (status != FS_STATUS_OK) {
    goto cleanup_disk;
  }

  environment->root_inode_id = superblock.root_inode_id;

  status = fs_bitmap_create(&environment->bitmap_context,
                            TEST_CLUSTER_COUNT);

  if (status != FS_STATUS_OK) {
    goto cleanup_disk;
  }

  status = fs_bitmap_deserialize_from_disk(
    environment->bitmap_context, environment->disk_context);

  if (status != FS_STATUS_OK) {
    goto cleanup_bitmap;
  }

  status = fs_index_create(&environment->index_context,
                           environment->disk_context,
                           &superblock);

  if (status != FS_STATUS_OK) {
    goto cleanup_bitmap;
  }

  status =
    fs_index_load_from_disk(environment->index_context);

  if (status != FS_STATUS_OK) {
    goto cleanup_index;
  }

  status =
    fs_alloc_create_context(&environment->alloc_context,
                            environment->disk_context,
                            environment->bitmap_context,
                            environment->index_context);

  if (status != FS_STATUS_OK) {
    goto cleanup_index;
  }

  status =
    fs_dir_create_context(&environment->dir_context,
                          environment->alloc_context,
                          environment->index_context);

  if (status != FS_STATUS_OK) {
    goto cleanup_alloc;
  }

  status = fs_vfs_create_context(&environment->vfs_context,
                                 environment->alloc_context,
                                 environment->dir_context,
                                 environment->index_context,
                                 superblock.root_inode_id);

  if (status != FS_STATUS_OK) {
    goto cleanup_dir;
  }

  return FS_STATUS_OK;

cleanup_dir:
  if (environment->dir_context != nullptr) {
    (void)fs_dir_destroy_context(environment->dir_context);
  }
cleanup_alloc:
  if (environment->alloc_context != nullptr) {
    (void)fs_alloc_destroy_context(
      environment->alloc_context);
  }
cleanup_index:
  if (environment->index_context != nullptr) {
    (void)fs_index_destroy(environment->index_context);
  }
cleanup_bitmap:
  if (environment->bitmap_context != nullptr) {
    (void)fs_bitmap_destroy(environment->bitmap_context);
  }
cleanup_disk:
  if (environment->disk_context != nullptr) {
    (void)fs_disk_close(environment->disk_context);
  }
  test_cleanup_disk_file(TEST_DISK_PATH);
  return status;
}

fs_status_t
test_close_environment(test_environment_t* environment) {
  if (environment == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (environment->vfs_context != nullptr) {
    (void)fs_vfs_destroy_context(environment->vfs_context);
  }
  if (environment->dir_context != nullptr) {
    (void)fs_dir_destroy_context(environment->dir_context);
  }
  if (environment->alloc_context != nullptr) {
    (void)fs_alloc_destroy_context(
      environment->alloc_context);
  }
  if (environment->index_context != nullptr) {
    (void)fs_index_flush(environment->index_context);
    (void)fs_index_destroy(environment->index_context);
  }
  if (environment->bitmap_context != nullptr
      && environment->disk_context != nullptr) {
    (void)fs_bitmap_serialize_to_disk(
      environment->bitmap_context,
      environment->disk_context);
    (void)fs_bitmap_destroy(environment->bitmap_context);
  }
  if (environment->disk_context != nullptr) {
    (void)fs_disk_flush(environment->disk_context);
    (void)fs_disk_close(environment->disk_context);
  }

  return FS_STATUS_OK;
}

fs_status_t
test_reopen_environment(test_environment_t* environment) {
  if (environment == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  memset(environment, 0, sizeof(test_environment_t));

  fs_status_t status =
    fs_disk_create_or_open(&environment->disk_context,
                           TEST_DISK_PATH,
                           TEST_CLUSTER_COUNT,
                           false);
  if (status != FS_STATUS_OK) {
    goto cleanup_disk;
  }

  fs_superblock_t superblock = {0};
  status = fs_superblock_read_from_disk(
    environment->disk_context, &superblock);

  if (status != FS_STATUS_OK) {
    goto cleanup_disk;
  }

  if (superblock.magic != FS_MAGIC_NUMBER) {
    status = FS_STATUS_ERROR_INVALID_ARGUMENT;
    goto cleanup_disk;
  }

  environment->root_inode_id = superblock.root_inode_id;
  status = fs_bitmap_create(&environment->bitmap_context,
                            superblock.total_clusters);
  if (status != FS_STATUS_OK) {
    goto cleanup_disk;
  }

  status = fs_bitmap_deserialize_from_disk(
    environment->bitmap_context, environment->disk_context);
  if (status != FS_STATUS_OK) {
    goto cleanup_bitmap;
  }

  status = fs_index_create(&environment->index_context,
                           environment->disk_context,
                           &superblock);

  if (status != FS_STATUS_OK) {
    goto cleanup_bitmap;
  }

  status =
    fs_index_load_from_disk(environment->index_context);

  if (status != FS_STATUS_OK) {
    goto cleanup_index;
  }

  status =
    fs_alloc_create_context(&environment->alloc_context,
                            environment->disk_context,
                            environment->bitmap_context,
                            environment->index_context);

  if (status != FS_STATUS_OK) {
    goto cleanup_index;
  }

  status =
    fs_dir_create_context(&environment->dir_context,
                          environment->alloc_context,
                          environment->index_context);

  if (status != FS_STATUS_OK) {
    goto cleanup_alloc;
  }

  status = fs_vfs_create_context(&environment->vfs_context,
                                 environment->alloc_context,
                                 environment->dir_context,
                                 environment->index_context,
                                 superblock.root_inode_id);

  if (status != FS_STATUS_OK) {
    goto cleanup_dir;
  }

  return FS_STATUS_OK;

cleanup_dir:
  if (environment->dir_context != nullptr) {
    (void)fs_dir_destroy_context(environment->dir_context);
    environment->dir_context = nullptr;
  }
cleanup_alloc:
  if (environment->alloc_context != nullptr) {
    (void)fs_alloc_destroy_context(
      environment->alloc_context);
    environment->alloc_context = nullptr;
  }
cleanup_index:
  if (environment->index_context != nullptr) {
    (void)fs_index_destroy(environment->index_context);
    environment->index_context = nullptr;
  }
cleanup_bitmap:
  if (environment->bitmap_context != nullptr) {
    (void)fs_bitmap_destroy(environment->bitmap_context);
    environment->bitmap_context = nullptr;
  }
cleanup_disk:
  if (environment->disk_context != nullptr) {
    (void)fs_disk_close(environment->disk_context);
    environment->disk_context = nullptr;
  }

  return status;
}

fs_status_t test_teardown_full_environment(
  test_environment_t* environment) {
  const fs_status_t close_status =
    test_close_environment(environment);
  test_cleanup_disk_file(TEST_DISK_PATH);
  return close_status;
}