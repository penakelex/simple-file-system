#include "framework/test_framework.h"
#include "framework/test_helpers.h"
#include "fs/config.h"
#include "fs/storage/disk.h"


static void test_disk_create_and_close() {
  fs_disk_t* disk_context = nullptr;
  test_cleanup_disk_file(TEST_DISK_PATH);

  const fs_status_t status = fs_disk_create_or_open(
    &disk_context, TEST_DISK_PATH, 64U, false);
  TEST_ASSERT_STATUS_OK(status,
                        "disk create should succeed");
  TEST_ASSERT(disk_context != nullptr,
              "disk should not be null");

  const uint32_t total_clusters =
    fs_disk_get_total_cluster_count(disk_context);
  TEST_ASSERT_EQUAL_UINT(
    64U, total_clusters, "cluster count should match");

  const bool is_read_only =
    fs_disk_is_read_only(disk_context);
  TEST_ASSERT(is_read_only == false,
              "disk should not be read-only");

  const uint32_t cluster_size = fs_disk_get_cluster_size();
  TEST_ASSERT_EQUAL_UINT(FS_CLUSTER_SIZE,
                         cluster_size,
                         "cluster size should match");

  const fs_status_t close_status =
    fs_disk_close(disk_context);
  TEST_ASSERT_STATUS_OK(close_status,
                        "disk close should succeed");
  test_cleanup_disk_file(TEST_DISK_PATH);
}

static void test_disk_read_write_cluster() {
  fs_disk_t* disk_context = nullptr;
  test_cleanup_disk_file(TEST_DISK_PATH);

  (void)fs_disk_create_or_open(
    &disk_context, TEST_DISK_PATH, 64U, false);

  uint8_t write_buffer[FS_CLUSTER_SIZE];
  memset(write_buffer, 0xAB, FS_CLUSTER_SIZE);

  const fs_status_t write_status =
    fs_disk_write_cluster(disk_context, 5U, write_buffer);
  TEST_ASSERT_STATUS_OK(write_status,
                        "write cluster should succeed");

  uint8_t read_buffer[FS_CLUSTER_SIZE] = {0};
  const fs_status_t read_status =
    fs_disk_read_cluster(disk_context, 5U, read_buffer);
  TEST_ASSERT_STATUS_OK(read_status,
                        "read cluster should succeed");

  TEST_ASSERT(
    memcmp(write_buffer, read_buffer, FS_CLUSTER_SIZE) == 0,
    "read data should match written data");

  (void)fs_disk_close(disk_context);
  test_cleanup_disk_file(TEST_DISK_PATH);
}

static void test_disk_multiple_clusters() {
  fs_disk_t* disk_context = nullptr;
  test_cleanup_disk_file(TEST_DISK_PATH);

  (void)fs_disk_create_or_open(
    &disk_context, TEST_DISK_PATH, 64U, false);

  uint8_t write_buffer[FS_CLUSTER_SIZE];
  uint8_t read_buffer[FS_CLUSTER_SIZE];

  for (uint32_t cluster_index = 0; cluster_index < 10U;
       ++cluster_index) {
    memset(
      write_buffer, (int)cluster_index, FS_CLUSTER_SIZE);
    const fs_status_t write_status = fs_disk_write_cluster(
      disk_context, cluster_index, write_buffer);
    TEST_ASSERT_STATUS_OK(write_status,
                          "write should succeed");
  }

  for (uint32_t cluster_index = 0; cluster_index < 10U;
       ++cluster_index) {
    memset(read_buffer, 0, FS_CLUSTER_SIZE);
    const fs_status_t read_status = fs_disk_read_cluster(
      disk_context, cluster_index, read_buffer);
    TEST_ASSERT_STATUS_OK(read_status,
                          "read should succeed");

    const uint8_t expected_value = (uint8_t)cluster_index;
    bool all_match = true;
    for (size_t byte_index = 0;
         byte_index < FS_CLUSTER_SIZE;
         ++byte_index) {
      if (read_buffer[byte_index] != expected_value) {
        all_match = false;
        break;
      }
    }
    TEST_ASSERT(
      all_match,
      "cluster data should match written pattern");
  }

  (void)fs_disk_close(disk_context);
  test_cleanup_disk_file(TEST_DISK_PATH);
}

static void test_disk_out_of_bounds() {
  fs_disk_t* disk_context = nullptr;
  test_cleanup_disk_file(TEST_DISK_PATH);

  (void)fs_disk_create_or_open(
    &disk_context, TEST_DISK_PATH, 64U, false);

  uint8_t buffer[FS_CLUSTER_SIZE] = {0};
  fs_status_t status =
    fs_disk_read_cluster(disk_context, 100U, buffer);
  TEST_ASSERT_STATUS_EQUAL(
    FS_STATUS_ERROR_OUT_OF_BOUNDS,
    status,
    "read out of bounds should fail");

  status =
    fs_disk_write_cluster(disk_context, 100U, buffer);
  TEST_ASSERT_STATUS_EQUAL(
    FS_STATUS_ERROR_OUT_OF_BOUNDS,
    status,
    "write out of bounds should fail");

  (void)fs_disk_close(disk_context);
  test_cleanup_disk_file(TEST_DISK_PATH);
}

static void test_disk_read_only_mode() {
  fs_disk_t* disk_context = nullptr;
  test_cleanup_disk_file(TEST_DISK_PATH);

  fs_status_t status = fs_disk_create_or_open(
    &disk_context, TEST_DISK_PATH, 64U, false);
  TEST_ASSERT_STATUS_OK(status,
                        "create disk should succeed");

  uint8_t write_buffer[FS_CLUSTER_SIZE];
  memset(write_buffer, 0xCD, FS_CLUSTER_SIZE);
  (void)fs_disk_write_cluster(
    disk_context, 0U, write_buffer);
  (void)fs_disk_close(disk_context);

  status = fs_disk_create_or_open(
    &disk_context, TEST_DISK_PATH, 64U, true);
  TEST_ASSERT_STATUS_OK(status,
                        "open read-only should succeed");

  const bool is_read_only =
    fs_disk_is_read_only(disk_context);
  TEST_ASSERT(is_read_only, "disk should be read-only");

  uint8_t buffer[FS_CLUSTER_SIZE] = {0};
  status = fs_disk_write_cluster(disk_context, 0U, buffer);
  TEST_ASSERT_STATUS_EQUAL(
    FS_STATUS_ERROR_READ_ONLY,
    status,
    "write to read-only should fail");

  status = fs_disk_read_cluster(disk_context, 0U, buffer);
  TEST_ASSERT_STATUS_OK(
    status, "read from read-only should succeed");

  TEST_ASSERT(
    memcmp(buffer, write_buffer, FS_CLUSTER_SIZE) == 0,
    "read data should match original write");

  (void)fs_disk_close(disk_context);
  test_cleanup_disk_file(TEST_DISK_PATH);
}

static void test_disk_flush() {
  fs_disk_t* disk_context = nullptr;
  test_cleanup_disk_file(TEST_DISK_PATH);

  (void)fs_disk_create_or_open(
    &disk_context, TEST_DISK_PATH, 64U, false);

  const fs_status_t status = fs_disk_flush(disk_context);
  TEST_ASSERT_STATUS_OK(status, "flush should succeed");

  (void)fs_disk_close(disk_context);
  test_cleanup_disk_file(TEST_DISK_PATH);
}

static void test_disk_null_arguments() {
  fs_disk_t* disk_context = nullptr;

  fs_status_t status = fs_disk_create_or_open(
    nullptr, TEST_DISK_PATH, 64U, false);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "null output should fail");

  status = fs_disk_create_or_open(
    &disk_context, nullptr, 64U, false);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "null path should fail");

  status = fs_disk_create_or_open(
    &disk_context, TEST_DISK_PATH, 0U, false);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "zero clusters should fail");

  status = fs_disk_close(nullptr);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "close null should fail");

  status = fs_disk_read_cluster(nullptr, 0U, nullptr);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "read null disk should fail");

  status = fs_disk_write_cluster(nullptr, 0U, nullptr);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "write null disk should fail");
}

int main() {
  TEST_SUITE_BEGIN("Disk Layer");
  TEST_RUN(test_disk_create_and_close);
  TEST_RUN(test_disk_read_write_cluster);
  TEST_RUN(test_disk_multiple_clusters);
  TEST_RUN(test_disk_out_of_bounds);
  TEST_RUN(test_disk_read_only_mode);
  TEST_RUN(test_disk_flush);
  TEST_RUN(test_disk_null_arguments);
  TEST_SUITE_END();
  TEST_EXIT();
}