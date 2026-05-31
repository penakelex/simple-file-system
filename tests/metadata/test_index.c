#include "framework/test_framework.h"
#include "framework/test_helpers.h"
#include "fs/config.h"
#include "fs/metadata/index.h"
#include "fs/storage/superblock.h"

static void test_index_create_destroy() {
  fs_disk_t* disk_context = nullptr;
  test_cleanup_disk_file(TEST_DISK_PATH);

  (void)fs_disk_create_or_open(&disk_context,
                               TEST_DISK_PATH,
                               TEST_CLUSTER_COUNT,
                               false);

  fs_superblock_t superblock = {0};
  (void)fs_superblock_initialize(&superblock,
                                 TEST_CLUSTER_COUNT);
  (void)fs_superblock_write_to_disk(disk_context,
                                    &superblock);

  fs_index_t* index_context = nullptr;
  const fs_status_t status = fs_index_create(
    &index_context, disk_context, &superblock);
  TEST_ASSERT_STATUS_OK(status,
                        "index create should succeed");
  TEST_ASSERT(index_context != nullptr,
              "index should not be null");

  const fs_status_t destroy_status =
    fs_index_destroy(index_context);
  TEST_ASSERT_STATUS_OK(destroy_status,
                        "index destroy should succeed");

  (void)fs_disk_close(disk_context);
  test_cleanup_disk_file(TEST_DISK_PATH);
}

static void test_index_allocate_and_read_inode() {
  test_environment_t environment = {0};
  const fs_status_t setup_status =
    test_setup_full_environment(&environment);
  TEST_ASSERT_STATUS_OK(setup_status,
                        "setup should succeed");

  uint32_t new_inode_id = UINT32_MAX;
  fs_status_t status =
    fs_index_allocate_inode(environment.index_context,
                            FS_TYPE_REGULAR,
                            &new_inode_id);
  TEST_ASSERT_STATUS_OK(status,
                        "allocate inode should succeed");
  TEST_ASSERT(new_inode_id >= 3U,
              "allocated inode should be >= 3");

  fs_inode_t read_inode = {0};
  status = fs_index_read_inode(
    environment.index_context, new_inode_id, &read_inode);
  TEST_ASSERT_STATUS_OK(status,
                        "read inode should succeed");
  TEST_ASSERT_EQUAL_UINT(
    new_inode_id, read_inode.id, "inode id should match");
  TEST_ASSERT_EQUAL_INT(FS_TYPE_REGULAR,
                        read_inode.type,
                        "type should be regular");
  TEST_ASSERT(read_inode.is_used,
              "inode should be marked as used");
  TEST_ASSERT_EQUAL_UINT(
    1U, read_inode.link_count, "link count should be 1");
  TEST_ASSERT_EQUAL_UINT(
    0U, read_inode.size, "size should be 0");

  (void)test_teardown_full_environment(&environment);
}

static void test_index_write_inode() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t new_inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_REGULAR,
                                &new_inode_id);

  fs_inode_t modified_inode = {0};
  (void)fs_index_read_inode(environment.index_context,
                            new_inode_id,
                            &modified_inode);
  modified_inode.size = 4096U;
  modified_inode.cluster_count = 1U;
  modified_inode.link_count = 2U;
  modified_inode.direct_clusters[0] = 50U;

  const fs_status_t write_status = fs_index_write_inode(
    environment.index_context, &modified_inode);
  TEST_ASSERT_STATUS_OK(write_status,
                        "write inode should succeed");

  fs_inode_t verify_inode = {0};
  (void)fs_index_read_inode(
    environment.index_context, new_inode_id, &verify_inode);
  TEST_ASSERT_EQUAL_UINT(
    4096U, verify_inode.size, "size should be updated");
  TEST_ASSERT_EQUAL_UINT(1U,
                         verify_inode.cluster_count,
                         "cluster count should be updated");
  TEST_ASSERT_EQUAL_UINT(2U,
                         verify_inode.link_count,
                         "link count should be updated");
  TEST_ASSERT_EQUAL_UINT(
    50U,
    verify_inode.direct_clusters[0],
    "direct cluster should be updated");

  (void)test_teardown_full_environment(&environment);
}

static void test_index_free_inode() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_DIRECTORY,
                                &inode_id);

  fs_inode_t verify_inode = {0};
  (void)fs_index_read_inode(
    environment.index_context, inode_id, &verify_inode);
  TEST_ASSERT(verify_inode.is_used, "inode should be used");

  const fs_status_t free_status = fs_index_free_inode(
    environment.index_context, inode_id);
  TEST_ASSERT_STATUS_OK(free_status,
                        "free inode should succeed");

  (void)fs_index_read_inode(
    environment.index_context, inode_id, &verify_inode);
  TEST_ASSERT(verify_inode.is_used == false,
              "inode should be freed");
  TEST_ASSERT_EQUAL_UINT(
    0U, verify_inode.size, "size should be 0");
  TEST_ASSERT_EQUAL_UINT(
    0U, verify_inode.type, "type should be 0");

  (void)test_teardown_full_environment(&environment);
}

static void test_index_persistence(void) {
  test_environment_t environment = {0};
  const fs_status_t setup_status =
    test_setup_full_environment(&environment);
  TEST_ASSERT_STATUS_OK(setup_status,
                        "setup should succeed");

  uint32_t inode_id = 0;
  (void)fs_index_allocate_inode(
    environment.index_context, FS_TYPE_REGULAR, &inode_id);

  fs_inode_t write_inode = {0};
  (void)fs_index_read_inode(
    environment.index_context, inode_id, &write_inode);
  write_inode.size = 8192U;
  write_inode.direct_clusters[0] = 100U;
  write_inode.direct_clusters[1] = 101U;
  (void)fs_index_write_inode(environment.index_context,
                             &write_inode);

  (void)fs_index_flush(environment.index_context);

  const fs_status_t close_status =
    test_close_environment(&environment);
  TEST_ASSERT_STATUS_OK(close_status,
                        "close should succeed");

  const fs_status_t reopen_status =
    test_reopen_environment(&environment);
  TEST_ASSERT_STATUS_OK(reopen_status,
                        "reopen should succeed");

  if (reopen_status != FS_STATUS_OK) {
    test_cleanup_disk_file(TEST_DISK_PATH);
    return;
  }

  fs_inode_t read_inode = {0};
  const fs_status_t status = fs_index_read_inode(
    environment.index_context, inode_id, &read_inode);
  TEST_ASSERT_STATUS_OK(
    status, "read persisted inode should succeed");
  TEST_ASSERT_EQUAL_UINT(
    8192U, read_inode.size, "persisted size should match");
  TEST_ASSERT_EQUAL_UINT(
    100U,
    read_inode.direct_clusters[0],
    "persisted cluster 0 should match");
  TEST_ASSERT_EQUAL_UINT(
    101U,
    read_inode.direct_clusters[1],
    "persisted cluster 1 should match");

  (void)test_teardown_full_environment(&environment);
}

static void test_index_invalid_inode_id() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  fs_inode_t read_inode = {0};
  const fs_status_t status =
    fs_index_read_inode(environment.index_context,
                        FS_INODE_COUNT + 100U,
                        &read_inode);
  TEST_ASSERT_STATUS_EQUAL(
    FS_STATUS_ERROR_INVALID_ARGUMENT,
    status,
    "read invalid inode should fail");

  (void)test_teardown_full_environment(&environment);
}

static void test_index_null_arguments() {
  fs_status_t status =
    fs_index_create(nullptr, nullptr, nullptr);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "create with null should fail");

  status = fs_index_destroy(nullptr);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "destroy null should fail");

  status = fs_index_read_inode(nullptr, 0U, nullptr);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "read null should fail");

  status = fs_index_allocate_inode(
    nullptr, FS_TYPE_REGULAR, nullptr);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "allocate null should fail");
}

int main() {
  TEST_SUITE_BEGIN("Index Layer");
  TEST_RUN(test_index_create_destroy);
  TEST_RUN(test_index_allocate_and_read_inode);
  TEST_RUN(test_index_write_inode);
  TEST_RUN(test_index_free_inode);
  TEST_RUN(test_index_persistence);
  TEST_RUN(test_index_invalid_inode_id);
  TEST_RUN(test_index_null_arguments);
  TEST_SUITE_END();
  TEST_EXIT();
}