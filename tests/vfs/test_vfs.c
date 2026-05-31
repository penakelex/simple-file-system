#include "framework/test_framework.h"
#include "framework/test_helpers.h"
#include "fs/vfs/vfs.h"

static void test_vfs_open_create_file() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  int32_t file_descriptor = -1;
  const fs_status_t status =
    fs_vfs_open(environment.vfs_context,
                "/new_file.txt",
                FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                &file_descriptor);
  TEST_ASSERT_STATUS_OK(status,
                        "open with create should succeed");
  TEST_ASSERT(file_descriptor >= 0,
              "file descriptor should be valid");

  const fs_status_t close_status =
    fs_vfs_close(environment.vfs_context, file_descriptor);
  TEST_ASSERT_STATUS_OK(close_status,
                        "close should succeed");

  (void)test_teardown_full_environment(&environment);
}

static void test_vfs_write_and_read() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  int32_t file_descriptor = -1;
  (void)fs_vfs_open(environment.vfs_context,
                    "/test_rw.txt",
                    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                    &file_descriptor);

  const char* test_data = "Hello, VFS Layer!";
  const size_t data_length = strlen(test_data);

  size_t bytes_written = 0;
  fs_status_t status = fs_vfs_write(environment.vfs_context,
                                    file_descriptor,
                                    test_data,
                                    data_length,
                                    &bytes_written);
  TEST_ASSERT_STATUS_OK(status, "write should succeed");
  TEST_ASSERT_EQUAL_SIZE(data_length,
                         bytes_written,
                         "bytes written should match");

  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  (void)fs_vfs_open(environment.vfs_context,
                    "/test_rw.txt",
                    FS_OPEN_READ_ONLY,
                    &file_descriptor);

  char read_buffer[256] = {0};
  size_t bytes_read = 0;
  status = fs_vfs_read(environment.vfs_context,
                       file_descriptor,
                       read_buffer,
                       sizeof(read_buffer),
                       &bytes_read);
  TEST_ASSERT_STATUS_OK(status, "read should succeed");
  TEST_ASSERT_EQUAL_SIZE(
    data_length, bytes_read, "bytes read should match");
  TEST_ASSERT(
    memcmp(test_data, read_buffer, data_length) == 0,
    "read data should match written data");

  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);
  (void)test_teardown_full_environment(&environment);
}

static void test_vfs_seek_operations() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  int32_t file_descriptor = -1;
  (void)fs_vfs_open(environment.vfs_context,
                    "/seek_test.txt",
                    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                    &file_descriptor);

  const char* test_data = "0123456789ABCDEF";
  size_t bytes_written = 0;
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     test_data,
                     strlen(test_data),
                     &bytes_written);

  fs_status_t status = fs_vfs_seek(
    environment.vfs_context, file_descriptor, 0, SEEK_SET);
  TEST_ASSERT_STATUS_OK(status,
                        "seek to beginning should succeed");

  char read_buffer[4] = {0};
  size_t bytes_read = 0;
  (void)fs_vfs_read(environment.vfs_context,
                    file_descriptor,
                    read_buffer,
                    4,
                    &bytes_read);
  TEST_ASSERT(memcmp("0123", read_buffer, 4) == 0,
              "should read from beginning");

  status = fs_vfs_seek(
    environment.vfs_context, file_descriptor, 10, SEEK_SET);
  TEST_ASSERT_STATUS_OK(
    status, "seek to position 10 should succeed");

  memset(read_buffer, 0, sizeof(read_buffer));
  (void)fs_vfs_read(environment.vfs_context,
                    file_descriptor,
                    read_buffer,
                    4,
                    &bytes_read);
  TEST_ASSERT(memcmp("ABCD", read_buffer, 4) == 0,
              "should read from position 10");

  status = fs_vfs_seek(
    environment.vfs_context, file_descriptor, -4, SEEK_CUR);
  TEST_ASSERT_STATUS_OK(status,
                        "relative seek should succeed");

  memset(read_buffer, 0, sizeof(read_buffer));
  (void)fs_vfs_read(environment.vfs_context,
                    file_descriptor,
                    read_buffer,
                    4,
                    &bytes_read);
  TEST_ASSERT(memcmp("ABCD", read_buffer, 4) == 0,
              "should read from relative position");

  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);
  (void)test_teardown_full_environment(&environment);
}

static void test_vfs_get_info() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  int32_t file_descriptor = -1;
  (void)fs_vfs_open(environment.vfs_context,
                    "/info_test.txt",
                    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                    &file_descriptor);

  const char* test_data = "Some test data for info";
  size_t bytes_written = 0;
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     test_data,
                     strlen(test_data),
                     &bytes_written);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  fs_inode_t inode_info = {0};
  const fs_status_t status = fs_vfs_get_info(
    environment.vfs_context, "/info_test.txt", &inode_info);
  TEST_ASSERT_STATUS_OK(status, "get info should succeed");
  TEST_ASSERT_EQUAL_INT(FS_TYPE_REGULAR,
                        inode_info.type,
                        "type should be regular");
  TEST_ASSERT_EQUAL_UINT((uint32_t)strlen(test_data),
                         inode_info.size,
                         "size should match");
  TEST_ASSERT(inode_info.is_used, "inode should be used");

  (void)test_teardown_full_environment(&environment);
}

static void test_vfs_remove_file() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  int32_t file_descriptor = -1;
  (void)fs_vfs_open(environment.vfs_context,
                    "/removable.txt",
                    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                    &file_descriptor);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  const fs_status_t remove_status = fs_vfs_remove(
    environment.vfs_context, "/removable.txt");
  TEST_ASSERT_STATUS_OK(remove_status,
                        "remove should succeed");

  fs_inode_t inode_info = {0};
  const fs_status_t info_status = fs_vfs_get_info(
    environment.vfs_context, "/removable.txt", &inode_info);
  TEST_ASSERT_STATUS_EQUAL(
    FS_STATUS_ERROR_OUT_OF_BOUNDS,
    info_status,
    "removed file should not be found");

  (void)test_teardown_full_environment(&environment);
}

static void test_vfs_create_and_remove_directory() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  fs_status_t status = fs_vfs_create_directory(
    environment.vfs_context, "/test_dir");
  TEST_ASSERT_STATUS_OK(status,
                        "create directory should succeed");

  fs_inode_t dir_info = {0};
  status = fs_vfs_get_info(
    environment.vfs_context, "/test_dir", &dir_info);
  TEST_ASSERT_STATUS_OK(
    status, "get directory info should succeed");
  TEST_ASSERT_EQUAL_INT(FS_TYPE_DIRECTORY,
                        dir_info.type,
                        "type should be directory");

  const fs_status_t remove_status = fs_vfs_remove_directory(
    environment.vfs_context, "/test_dir");
  TEST_ASSERT_STATUS_OK(remove_status,
                        "remove directory should succeed");

  (void)test_teardown_full_environment(&environment);
}

static void test_vfs_rename_file() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  int32_t file_descriptor = -1;
  (void)fs_vfs_open(environment.vfs_context,
                    "/old_name.txt",
                    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                    &file_descriptor);

  const char* test_data = "Test data for rename";
  size_t bytes_written = 0;
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     test_data,
                     strlen(test_data),
                     &bytes_written);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  const fs_status_t rename_status =
    fs_vfs_rename(environment.vfs_context,
                  "/old_name.txt",
                  "/new_name.txt");
  TEST_ASSERT_STATUS_OK(rename_status,
                        "rename should succeed");

  fs_inode_t inode_info = {0};
  fs_status_t status = fs_vfs_get_info(
    environment.vfs_context, "/new_name.txt", &inode_info);
  TEST_ASSERT_STATUS_OK(status, "new name should exist");
  TEST_ASSERT_EQUAL_UINT((uint32_t)strlen(test_data),
                         inode_info.size,
                         "size should be preserved");

  status = fs_vfs_get_info(
    environment.vfs_context, "/old_name.txt", &inode_info);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_OUT_OF_BOUNDS,
                           status,
                           "old name should not exist");

  (void)test_teardown_full_environment(&environment);
}

static void test_vfs_append_mode() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  int32_t file_descriptor = -1;
  (void)fs_vfs_open(environment.vfs_context,
                    "/append_test.txt",
                    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                    &file_descriptor);

  const char* first_write = "First";
  size_t bytes_written = 0;
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     first_write,
                     strlen(first_write),
                     &bytes_written);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  (void)fs_vfs_open(environment.vfs_context,
                    "/append_test.txt",
                    FS_OPEN_READ_WRITE | FS_OPEN_APPEND,
                    &file_descriptor);

  const char* second_write = "Second";
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     second_write,
                     strlen(second_write),
                     &bytes_written);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  (void)fs_vfs_open(environment.vfs_context,
                    "/append_test.txt",
                    FS_OPEN_READ_ONLY,
                    &file_descriptor);

  char read_buffer[256] = {0};
  size_t bytes_read = 0;
  (void)fs_vfs_read(environment.vfs_context,
                    file_descriptor,
                    read_buffer,
                    sizeof(read_buffer),
                    &bytes_read);

  TEST_ASSERT_EQUAL_STRING(
    "FirstSecond", read_buffer, "append should add to end");

  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);
  (void)test_teardown_full_environment(&environment);
}

static void test_vfs_nested_directories() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  (void)fs_vfs_create_directory(environment.vfs_context,
                                "/level1");
  (void)fs_vfs_create_directory(environment.vfs_context,
                                "/level1/level2");
  (void)fs_vfs_create_directory(environment.vfs_context,
                                "/level1/level2/level3");

  int32_t file_descriptor = -1;
  const fs_status_t status =
    fs_vfs_open(environment.vfs_context,
                "/level1/level2/level3/deep_file.txt",
                FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                &file_descriptor);
  TEST_ASSERT_STATUS_OK(
    status, "open in nested directory should succeed");

  const char* test_data = "Deep file content";
  size_t bytes_written = 0;
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     test_data,
                     strlen(test_data),
                     &bytes_written);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  fs_inode_t inode_info = {0};
  const fs_status_t info_status =
    fs_vfs_get_info(environment.vfs_context,
                    "/level1/level2/level3/deep_file.txt",
                    &inode_info);
  TEST_ASSERT_STATUS_OK(
    info_status, "get info for nested file should succeed");
  TEST_ASSERT_EQUAL_UINT((uint32_t)strlen(test_data),
                         inode_info.size,
                         "size should match");

  (void)test_teardown_full_environment(&environment);
}

int main() {
  TEST_SUITE_BEGIN("VFS Layer");
  TEST_RUN(test_vfs_open_create_file);
  TEST_RUN(test_vfs_write_and_read);
  TEST_RUN(test_vfs_seek_operations);
  TEST_RUN(test_vfs_get_info);
  TEST_RUN(test_vfs_remove_file);
  TEST_RUN(test_vfs_create_and_remove_directory);
  TEST_RUN(test_vfs_rename_file);
  TEST_RUN(test_vfs_append_mode);
  TEST_RUN(test_vfs_nested_directories);
  TEST_SUITE_END();
  TEST_EXIT();
}