#include "framework/test_framework.h"
#include "framework/test_helpers.h"
#include "fs/config.h"
#include "fs/vfs/vfs.h"

static void test_integration_full_workflow() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  (void)fs_vfs_create_directory(environment.vfs_context,
                                "/documents");
  (void)fs_vfs_create_directory(environment.vfs_context,
                                "/documents/work");
  (void)fs_vfs_create_directory(environment.vfs_context,
                                "/documents/personal");

  int32_t file_descriptor = -1;
  (void)fs_vfs_open(environment.vfs_context,
                    "/documents/work/report.txt",
                    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                    &file_descriptor);

  const char* report_data =
    "Quarterly Report Q4 2025\n\nRevenue: $1M\nExpenses: "
    "$500K";
  size_t bytes_written = 0;
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     report_data,
                     strlen(report_data),
                     &bytes_written);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  (void)fs_vfs_open(environment.vfs_context,
                    "/documents/personal/notes.txt",
                    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                    &file_descriptor);

  const char* notes_data = "Personal notes and reminders";
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     notes_data,
                     strlen(notes_data),
                     &bytes_written);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  (void)fs_vfs_rename(environment.vfs_context,
                      "/documents/work/report.txt",
                      "/documents/work/final_report.txt");

  (void)fs_vfs_open(environment.vfs_context,
                    "/documents/work/final_report.txt",
                    FS_OPEN_READ_ONLY,
                    &file_descriptor);

  char read_buffer[512] = {0};
  size_t bytes_read = 0;
  (void)fs_vfs_read(environment.vfs_context,
                    file_descriptor,
                    read_buffer,
                    sizeof(read_buffer),
                    &bytes_read);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  TEST_ASSERT_EQUAL_STRING(
    report_data,
    read_buffer,
    "report data should be preserved");

  (void)fs_vfs_remove(environment.vfs_context,
                      "/documents/personal/notes.txt");
  (void)fs_vfs_remove_directory(environment.vfs_context,
                                "/documents/personal");

  fs_inode_t inode_info = {0};
  const fs_status_t status =
    fs_vfs_get_info(environment.vfs_context,
                    "/documents/personal",
                    &inode_info);
  TEST_ASSERT_STATUS_EQUAL(
    FS_STATUS_ERROR_OUT_OF_BOUNDS,
    status,
    "removed directory should not exist");

  (void)test_teardown_full_environment(&environment);
}

static void test_integration_persistence(void) {
  test_environment_t environment = {0};
  const fs_status_t setup_status =
    test_setup_full_environment(&environment);
  TEST_ASSERT_STATUS_OK(setup_status,
                        "setup should succeed");

  (void)fs_vfs_create_directory(environment.vfs_context,
                                "/persistent");

  int32_t file_descriptor = -1;
  (void)fs_vfs_open(environment.vfs_context,
                    "/persistent/data.bin",
                    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                    &file_descriptor);

  uint8_t binary_data[1024];
  for (size_t byte_index = 0;
       byte_index < sizeof(binary_data);
       ++byte_index) {
    binary_data[byte_index] = (uint8_t)(byte_index % 256);
  }

  size_t bytes_written = 0;
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     binary_data,
                     sizeof(binary_data),
                     &bytes_written);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

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

  fs_inode_t file_info = {0};
  (void)fs_vfs_get_info(environment.vfs_context,
                        "/persistent/data.bin",
                        &file_info);

  (void)fs_vfs_open(environment.vfs_context,
                    "/persistent/data.bin",
                    FS_OPEN_READ_ONLY,
                    &file_descriptor);

  uint8_t read_buffer[1024] = {0};
  size_t bytes_read = 0;
  (void)fs_vfs_read(environment.vfs_context,
                    file_descriptor,
                    read_buffer,
                    sizeof(read_buffer),
                    &bytes_read);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  TEST_ASSERT_EQUAL_SIZE(sizeof(binary_data),
                         bytes_read,
                         "all bytes should be read");
  TEST_ASSERT(
    memcmp(binary_data, read_buffer, sizeof(binary_data))
      == 0,
    "persisted data should match original");

  (void)test_teardown_full_environment(&environment);
}

static void test_integration_large_file() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  int32_t file_descriptor = -1;
  (void)fs_vfs_open(environment.vfs_context,
                    "/large_file.bin",
                    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                    &file_descriptor);

  const size_t large_size = FS_CLUSTER_SIZE * 5;
  uint8_t* large_buffer = (uint8_t*)malloc(large_size);
  TEST_ASSERT(large_buffer != nullptr,
              "allocate large buffer should succeed");

  for (size_t byte_index = 0; byte_index < large_size;
       ++byte_index) {
    large_buffer[byte_index] =
      (uint8_t)((byte_index * 7) % 256);
  }

  size_t bytes_written = 0;
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     large_buffer,
                     large_size,
                     &bytes_written);
  TEST_ASSERT_EQUAL_SIZE(large_size,
                         bytes_written,
                         "all bytes should be written");
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  (void)fs_vfs_open(environment.vfs_context,
                    "/large_file.bin",
                    FS_OPEN_READ_ONLY,
                    &file_descriptor);

  uint8_t* read_buffer = (uint8_t*)malloc(large_size);
  TEST_ASSERT(read_buffer != nullptr,
              "allocate read buffer should succeed");

  size_t bytes_read = 0;
  (void)fs_vfs_read(environment.vfs_context,
                    file_descriptor,
                    read_buffer,
                    large_size,
                    &bytes_read);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  TEST_ASSERT_EQUAL_SIZE(
    large_size, bytes_read, "all bytes should be read");
  TEST_ASSERT(
    memcmp(large_buffer, read_buffer, large_size) == 0,
    "large file data should match");

  free(large_buffer);
  free(read_buffer);
  (void)test_teardown_full_environment(&environment);
}

static void test_integration_multiple_files() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  const uint32_t file_count = 10U;
  char file_names[10][32];
  char file_contents[10][64];

  for (uint32_t file_index = 0; file_index < file_count;
       ++file_index) {
    snprintf(file_names[file_index],
             sizeof(file_names[file_index]),
             "/file_%02u.txt",
             file_index);
    snprintf(file_contents[file_index],
             sizeof(file_contents[file_index]),
             "Content of file number %u",
             file_index);

    int32_t file_descriptor = -1;
    (void)fs_vfs_open(environment.vfs_context,
                      file_names[file_index],
                      FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
                      &file_descriptor);

    size_t bytes_written = 0;
    (void)fs_vfs_write(environment.vfs_context,
                       file_descriptor,
                       file_contents[file_index],
                       strlen(file_contents[file_index]),
                       &bytes_written);
    (void)fs_vfs_close(environment.vfs_context,
                       file_descriptor);
  }

  for (uint32_t file_index = 0; file_index < file_count;
       ++file_index) {
    int32_t file_descriptor = -1;
    (void)fs_vfs_open(environment.vfs_context,
                      file_names[file_index],
                      FS_OPEN_READ_ONLY,
                      &file_descriptor);

    char read_buffer[128] = {0};
    size_t bytes_read = 0;
    (void)fs_vfs_read(environment.vfs_context,
                      file_descriptor,
                      read_buffer,
                      sizeof(read_buffer),
                      &bytes_read);
    (void)fs_vfs_close(environment.vfs_context,
                       file_descriptor);

    TEST_ASSERT_EQUAL_STRING(file_contents[file_index],
                             read_buffer,
                             "file content should match");
  }

  (void)test_teardown_full_environment(&environment);
}

static void test_integration_path_navigation() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  (void)fs_vfs_create_directory(environment.vfs_context,
                                "/root_dir");
  (void)fs_vfs_create_directory(environment.vfs_context,
                                "/root_dir/sub_dir");

  int32_t file_descriptor = -1;
  (void)fs_vfs_open(
    environment.vfs_context,
    "/root_dir/sub_dir/../sub_dir/./test.txt",
    FS_OPEN_CREATE | FS_OPEN_READ_WRITE,
    &file_descriptor);

  const char* test_data = "Path navigation test";
  size_t bytes_written = 0;
  (void)fs_vfs_write(environment.vfs_context,
                     file_descriptor,
                     test_data,
                     strlen(test_data),
                     &bytes_written);
  (void)fs_vfs_close(environment.vfs_context,
                     file_descriptor);

  fs_inode_t inode_info = {0};
  const fs_status_t status =
    fs_vfs_get_info(environment.vfs_context,
                    "/root_dir/sub_dir/test.txt",
                    &inode_info);
  TEST_ASSERT_STATUS_OK(
    status,
    "file should be accessible via normalized path");

  (void)test_teardown_full_environment(&environment);
}

int main() {
  TEST_SUITE_BEGIN("Integration Tests");
  TEST_RUN(test_integration_full_workflow);
  TEST_RUN(test_integration_persistence);
  TEST_RUN(test_integration_large_file);
  TEST_RUN(test_integration_multiple_files);
  TEST_RUN(test_integration_path_navigation);
  TEST_SUITE_END();
  TEST_EXIT();
}