#include "framework/test_framework.h"
#include "fs/platform_io.h"

#define SEEK_TEST_CHUNK_SIZE 4096U
#define SEEK_TEST_TOTAL_CHUNKS 256U
#define SEEK_TEST_FILE_PATH "test_seek_validation.bin"

static void
verify_file_position(FILE* file_stream,
                     const int64_t expected_offset) {
  const int64_t actual_offset = fs_file_tell(file_stream);
  TEST_ASSERT_EQUAL_INT64(
    expected_offset,
    actual_offset,
    "file position should match expected offset");
}

static void test_seek_to_beginning() {
  FILE* file_stream = fopen(SEEK_TEST_FILE_PATH, "w+b");
  TEST_ASSERT(file_stream != nullptr,
              "file should be created");

  uint8_t write_buffer[SEEK_TEST_CHUNK_SIZE];
  memset(write_buffer, 0xAA, SEEK_TEST_CHUNK_SIZE);

  for (size_t chunk_index = 0;
       chunk_index < SEEK_TEST_TOTAL_CHUNKS;
       ++chunk_index) {
    fwrite(
      write_buffer, 1, SEEK_TEST_CHUNK_SIZE, file_stream);
  }
  fflush(file_stream);

  const int seek_result =
    fs_file_seek(file_stream, 0, SEEK_SET);
  TEST_ASSERT_EQUAL_INT(
    0, seek_result, "seek to beginning should succeed");
  verify_file_position(file_stream, 0);

  fclose(file_stream);
  remove(SEEK_TEST_FILE_PATH);
}

static void test_seek_to_middle() {
  FILE* file_stream = fopen(SEEK_TEST_FILE_PATH, "w+b");
  TEST_ASSERT(file_stream != nullptr,
              "file should be created");

  uint8_t write_buffer[SEEK_TEST_CHUNK_SIZE];
  memset(write_buffer, 0xAA, SEEK_TEST_CHUNK_SIZE);

  for (size_t chunk_index = 0;
       chunk_index < SEEK_TEST_TOTAL_CHUNKS;
       ++chunk_index) {
    fwrite(
      write_buffer, 1, SEEK_TEST_CHUNK_SIZE, file_stream);
  }
  fflush(file_stream);

  const int64_t middle_offset = 512000LL;
  const int seek_result =
    fs_file_seek(file_stream, middle_offset, SEEK_SET);
  TEST_ASSERT_EQUAL_INT(
    0, seek_result, "seek to middle should succeed");
  verify_file_position(file_stream, middle_offset);

  fclose(file_stream);
  remove(SEEK_TEST_FILE_PATH);
}

static void test_seek_relative_current() {
  FILE* file_stream = fopen(SEEK_TEST_FILE_PATH, "w+b");
  TEST_ASSERT(file_stream != nullptr,
              "file should be created");

  uint8_t write_buffer[SEEK_TEST_CHUNK_SIZE];
  memset(write_buffer, 0xAA, SEEK_TEST_CHUNK_SIZE);

  for (size_t chunk_index = 0;
       chunk_index < SEEK_TEST_TOTAL_CHUNKS;
       ++chunk_index) {
    fwrite(
      write_buffer, 1, SEEK_TEST_CHUNK_SIZE, file_stream);
  }
  fflush(file_stream);

  const int64_t middle_offset = 512000LL;
  fs_file_seek(file_stream, middle_offset, SEEK_SET);

  const int seek_result =
    fs_file_seek(file_stream, -1024, SEEK_CUR);
  TEST_ASSERT_EQUAL_INT(
    0, seek_result, "relative seek should succeed");
  verify_file_position(file_stream, middle_offset - 1024);

  fclose(file_stream);
  remove(SEEK_TEST_FILE_PATH);
}

static void test_seek_from_end() {
  FILE* file_stream = fopen(SEEK_TEST_FILE_PATH, "w+b");
  TEST_ASSERT(file_stream != nullptr,
              "file should be created");

  uint8_t write_buffer[SEEK_TEST_CHUNK_SIZE];
  memset(write_buffer, 0xAA, SEEK_TEST_CHUNK_SIZE);

  for (size_t chunk_index = 0;
       chunk_index < SEEK_TEST_TOTAL_CHUNKS;
       ++chunk_index) {
    fwrite(
      write_buffer, 1, SEEK_TEST_CHUNK_SIZE, file_stream);
  }
  fflush(file_stream);

  const int64_t file_end_offset =
    (int64_t)SEEK_TEST_TOTAL_CHUNKS * SEEK_TEST_CHUNK_SIZE;
  const int seek_result =
    fs_file_seek(file_stream, 0, SEEK_END);
  TEST_ASSERT_EQUAL_INT(
    0, seek_result, "seek from end should succeed");
  verify_file_position(file_stream, file_end_offset);

  fclose(file_stream);
  remove(SEEK_TEST_FILE_PATH);
}

static void test_seek_large_offset() {
  FILE* file_stream = fopen(SEEK_TEST_FILE_PATH, "w+b");
  TEST_ASSERT(file_stream != nullptr,
              "file should be created");

  uint8_t write_buffer[SEEK_TEST_CHUNK_SIZE];
  memset(write_buffer, 0xAA, SEEK_TEST_CHUNK_SIZE);

  for (size_t chunk_index = 0;
       chunk_index < SEEK_TEST_TOTAL_CHUNKS;
       ++chunk_index) {
    fwrite(
      write_buffer, 1, SEEK_TEST_CHUNK_SIZE, file_stream);
  }
  fflush(file_stream);

  const int64_t large_offset = 3000000000LL;
  const int seek_result =
    fs_file_seek(file_stream, large_offset, SEEK_SET);
  TEST_ASSERT_EQUAL_INT(
    0, seek_result, "large offset seek should succeed");
  verify_file_position(file_stream, large_offset);

  fclose(file_stream);
  remove(SEEK_TEST_FILE_PATH);
}

static void test_seek_platform_functions() {
  FILE* file_stream = fopen(SEEK_TEST_FILE_PATH, "w+b");
  TEST_ASSERT(file_stream != nullptr,
              "file should be created");

  uint8_t write_buffer[SEEK_TEST_CHUNK_SIZE];
  memset(write_buffer, 0xBB, SEEK_TEST_CHUNK_SIZE);
  fwrite(
    write_buffer, 1, SEEK_TEST_CHUNK_SIZE, file_stream);
  fflush(file_stream);

  const int seek_result =
    fs_file_seek(file_stream, 0, SEEK_SET);
  TEST_ASSERT_EQUAL_INT(
    0, seek_result, "seek should succeed");

  uint8_t read_buffer[SEEK_TEST_CHUNK_SIZE] = {0};
  const size_t bytes_read = fread(
    read_buffer, 1, SEEK_TEST_CHUNK_SIZE, file_stream);
  TEST_ASSERT_EQUAL_SIZE(SEEK_TEST_CHUNK_SIZE,
                         bytes_read,
                         "should read full chunk");
  TEST_ASSERT(
    memcmp(write_buffer, read_buffer, SEEK_TEST_CHUNK_SIZE)
      == 0,
    "read data should match written data");

  const int sync_result = fs_file_sync(file_stream);
  TEST_ASSERT_EQUAL_INT(
    0, sync_result, "sync should succeed");

  fclose(file_stream);
  remove(SEEK_TEST_FILE_PATH);
}

int main() {
  TEST_SUITE_BEGIN("Platform Seek Operations");
  TEST_RUN(test_seek_to_beginning);
  TEST_RUN(test_seek_to_middle);
  TEST_RUN(test_seek_relative_current);
  TEST_RUN(test_seek_from_end);
  TEST_RUN(test_seek_large_offset);
  TEST_RUN(test_seek_platform_functions);
  TEST_SUITE_END();
  TEST_EXIT();
}