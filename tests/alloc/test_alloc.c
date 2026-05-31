#include "framework/test_framework.h"
#include "framework/test_helpers.h"
#include "fs/alloc/alloc.h"
#include "fs/config.h"

static void test_alloc_resolve_direct_clusters() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t inode_id = 0;
  (void)fs_index_allocate_inode(
    environment.index_context, FS_TYPE_REGULAR, &inode_id);

  fs_inode_t inode_context = {0};
  (void)fs_index_read_inode(
    environment.index_context, inode_id, &inode_context);

  for (uint32_t logical_index = 0U;
       logical_index < FS_DIRECT_POINTERS;
       ++logical_index) {
    uint32_t physical_index = 0;
    const fs_status_t status =
      fs_alloc_resolve_cluster(environment.alloc_context,
                               &inode_context,
                               logical_index,
                               true,
                               &physical_index);
    TEST_ASSERT_STATUS_OK(
      status, "resolve direct cluster should succeed");
    TEST_ASSERT(physical_index > 0U,
                "physical cluster should be allocated");
  }

  (void)fs_index_write_inode(environment.index_context,
                             &inode_context);
  (void)test_teardown_full_environment(&environment);
}

static void test_alloc_resolve_single_indirect() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t inode_id = 0;
  (void)fs_index_allocate_inode(
    environment.index_context, FS_TYPE_REGULAR, &inode_id);

  fs_inode_t inode_context = {0};
  (void)fs_index_read_inode(
    environment.index_context, inode_id, &inode_context);

  const uint32_t logical_index = FS_DIRECT_POINTERS;
  uint32_t physical_index = 0;
  const fs_status_t status =
    fs_alloc_resolve_cluster(environment.alloc_context,
                             &inode_context,
                             logical_index,
                             true,
                             &physical_index);
  TEST_ASSERT_STATUS_OK(
    status, "resolve single indirect should succeed");
  TEST_ASSERT(physical_index > 0U,
              "physical cluster should be allocated");
  TEST_ASSERT(
    inode_context.single_indirect_cluster > 0U,
    "single indirect cluster should be allocated");

  (void)fs_index_write_inode(environment.index_context,
                             &inode_context);
  (void)test_teardown_full_environment(&environment);
}

static void test_alloc_write_and_read_data() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t inode_id = 0;
  (void)fs_index_allocate_inode(
    environment.index_context, FS_TYPE_REGULAR, &inode_id);

  fs_inode_t inode_context = {0};
  (void)fs_index_read_inode(
    environment.index_context, inode_id, &inode_context);

  const char* test_data =
    "Hello, File System! This is a test data string.";
  const size_t data_length = strlen(test_data) + 1U;

  size_t bytes_written = 0;
  fs_status_t status =
    fs_alloc_write_data(environment.alloc_context,
                        &inode_context,
                        0,
                        test_data,
                        data_length,
                        &bytes_written);
  TEST_ASSERT_STATUS_OK(status,
                        "write data should succeed");
  TEST_ASSERT_EQUAL_SIZE(data_length,
                         bytes_written,
                         "bytes written should match");
  TEST_ASSERT_EQUAL_UINT((uint32_t)data_length,
                         inode_context.size,
                         "inode size should be updated");

  (void)fs_index_write_inode(environment.index_context,
                             &inode_context);

  char read_buffer[256] = {0};
  size_t bytes_read = 0;
  status = fs_alloc_read_data(environment.alloc_context,
                              &inode_context,
                              0,
                              read_buffer,
                              sizeof(read_buffer),
                              &bytes_read);
  TEST_ASSERT_STATUS_OK(status, "read data should succeed");
  TEST_ASSERT_EQUAL_SIZE(
    data_length, bytes_read, "bytes read should match");
  TEST_ASSERT_EQUAL_STRING(
    test_data,
    read_buffer,
    "read data should match written data");

  (void)test_teardown_full_environment(&environment);
}

static void test_alloc_write_across_clusters() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t inode_id = 0;
  (void)fs_index_allocate_inode(
    environment.index_context, FS_TYPE_REGULAR, &inode_id);

  fs_inode_t inode_context = {0};
  (void)fs_index_read_inode(
    environment.index_context, inode_id, &inode_context);

  uint8_t large_buffer[FS_CLUSTER_SIZE * 3];
  for (size_t byte_index = 0;
       byte_index < sizeof(large_buffer);
       ++byte_index) {
    large_buffer[byte_index] = (uint8_t)(byte_index % 256);
  }

  size_t bytes_written = 0;
  const fs_status_t write_status =
    fs_alloc_write_data(environment.alloc_context,
                        &inode_context,
                        0,
                        large_buffer,
                        sizeof(large_buffer),
                        &bytes_written);
  TEST_ASSERT_STATUS_OK(
    write_status, "write across clusters should succeed");
  TEST_ASSERT_EQUAL_SIZE(sizeof(large_buffer),
                         bytes_written,
                         "all bytes should be written");

  (void)fs_index_write_inode(environment.index_context,
                             &inode_context);

  uint8_t read_buffer[FS_CLUSTER_SIZE * 3] = {0};
  size_t bytes_read = 0;
  const fs_status_t read_status =
    fs_alloc_read_data(environment.alloc_context,
                       &inode_context,
                       0,
                       read_buffer,
                       sizeof(read_buffer),
                       &bytes_read);
  TEST_ASSERT_STATUS_OK(
    read_status, "read across clusters should succeed");
  TEST_ASSERT_EQUAL_SIZE(sizeof(large_buffer),
                         bytes_read,
                         "all bytes should be read");
  TEST_ASSERT(
    memcmp(large_buffer, read_buffer, sizeof(large_buffer))
      == 0,
    "multi-cluster data should match");

  (void)test_teardown_full_environment(&environment);
}

static void test_alloc_partial_read_write() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t inode_id = 0;
  (void)fs_index_allocate_inode(
    environment.index_context, FS_TYPE_REGULAR, &inode_id);

  fs_inode_t inode_context = {0};
  (void)fs_index_read_inode(
    environment.index_context, inode_id, &inode_context);

  const char* first_part = "First part of data. ";
  const size_t first_length = strlen(first_part);

  size_t bytes_written = 0;
  (void)fs_alloc_write_data(environment.alloc_context,
                            &inode_context,
                            0,
                            first_part,
                            first_length,
                            &bytes_written);

  const char* second_part = "Second part of data.";
  const size_t second_length = strlen(second_part);

  (void)fs_alloc_write_data(environment.alloc_context,
                            &inode_context,
                            first_length,
                            second_part,
                            second_length,
                            &bytes_written);

  (void)fs_index_write_inode(environment.index_context,
                             &inode_context);

  char read_buffer[256] = {0};
  size_t bytes_read = 0;
  (void)fs_alloc_read_data(environment.alloc_context,
                           &inode_context,
                           0,
                           read_buffer,
                           sizeof(read_buffer),
                           &bytes_read);

  const char* expected_full =
    "First part of data. Second part of data.";
  TEST_ASSERT_EQUAL_STRING(expected_full,
                           read_buffer,
                           "combined data should match");

  (void)test_teardown_full_environment(&environment);
}

static void test_alloc_truncate_file() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t inode_id = 0;
  (void)fs_index_allocate_inode(
    environment.index_context, FS_TYPE_REGULAR, &inode_id);

  fs_inode_t inode_context = {0};
  (void)fs_index_read_inode(
    environment.index_context, inode_id, &inode_context);

  uint8_t write_buffer[FS_CLUSTER_SIZE * 2];
  memset(write_buffer, 0xFF, sizeof(write_buffer));
  size_t bytes_written = 0;
  (void)fs_alloc_write_data(environment.alloc_context,
                            &inode_context,
                            0,
                            write_buffer,
                            sizeof(write_buffer),
                            &bytes_written);

  TEST_ASSERT(inode_context.size > 0U,
              "file should have data");
  TEST_ASSERT(inode_context.cluster_count > 0U,
              "file should have clusters");

  const fs_status_t truncate_status =
    fs_alloc_truncate_file(environment.alloc_context,
                           &inode_context);
  TEST_ASSERT_STATUS_OK(truncate_status,
                        "truncate should succeed");
  TEST_ASSERT_EQUAL_UINT(0U,
                         inode_context.size,
                         "size should be 0 after truncate");
  TEST_ASSERT_EQUAL_UINT(
    0U,
    inode_context.cluster_count,
    "cluster count should be 0 after truncate");

  (void)test_teardown_full_environment(&environment);
}

static void test_alloc_read_beyond_size() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t inode_id = 0;
  (void)fs_index_allocate_inode(
    environment.index_context, FS_TYPE_REGULAR, &inode_id);

  fs_inode_t inode_context = {0};
  (void)fs_index_read_inode(
    environment.index_context, inode_id, &inode_context);

  const char* test_data = "Short data";
  size_t bytes_written = 0;
  (void)fs_alloc_write_data(environment.alloc_context,
                            &inode_context,
                            0,
                            test_data,
                            strlen(test_data) + 1U,
                            &bytes_written);

  (void)fs_index_write_inode(environment.index_context,
                             &inode_context);

  char read_buffer[256] = {0};
  size_t bytes_read = 0;
  const fs_status_t status =
    fs_alloc_read_data(environment.alloc_context,
                       &inode_context,
                       1000,
                       read_buffer,
                       sizeof(read_buffer),
                       &bytes_read);
  TEST_ASSERT_STATUS_OK(status,
                        "read beyond size should succeed");
  TEST_ASSERT_EQUAL_SIZE(
    0U,
    bytes_read,
    "no bytes should be read beyond file size");

  (void)test_teardown_full_environment(&environment);
}

int main() {
  TEST_SUITE_BEGIN("Allocation Layer");
  TEST_RUN(test_alloc_resolve_direct_clusters);
  TEST_RUN(test_alloc_resolve_single_indirect);
  TEST_RUN(test_alloc_write_and_read_data);
  TEST_RUN(test_alloc_write_across_clusters);
  TEST_RUN(test_alloc_partial_read_write);
  TEST_RUN(test_alloc_truncate_file);
  TEST_RUN(test_alloc_read_beyond_size);
  TEST_SUITE_END();
  TEST_EXIT();
}