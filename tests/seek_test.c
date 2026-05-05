#include "fs/platform_io.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

[[nodiscard]] static int
verify_file_position(FILE* file_stream,
                     const int64_t expected_offset) {
  const int64_t actual_offset = fs_file_tell(file_stream);

  if (actual_offset != expected_offset) {
    fprintf(stderr,
            "Ошибка позиционирования: ожидалось %" PRId64
            ", получено %" PRId64 "\n",
            expected_offset,
            actual_offset);
    return 1;
  }

  return 0;
}

int main() {
  const char* test_file_path = "test_seek_validation.bin";
  FILE* test_file_stream = fopen(test_file_path, "w+b");

  if (test_file_stream == nullptr) {
    fprintf(stderr, "Не удалось создать тестовый файл\n");
    return 1;
  }

  const size_t chunk_size = 4096;
  const size_t total_chunks = 256;
  uint8_t write_buffer[chunk_size];
  memset(write_buffer, 0xAA, sizeof(write_buffer));

  for (size_t chunk_index = 0; chunk_index < total_chunks;
       ++chunk_index) {
    if (fwrite(
          write_buffer, 1, chunk_size, test_file_stream)
        != chunk_size) {
      fprintf(stderr, "Ошибка записи тестовых данных\n");
      fclose(test_file_stream);
      return 1;
    }
  }

  fflush(test_file_stream);

  int test_failure_count = 0;

  if (fs_file_seek(test_file_stream, 0, SEEK_SET) == 0) {
    test_failure_count +=
      verify_file_position(test_file_stream, 0);
  }

  const int64_t middle_offset = 512000;
  if (fs_file_seek(
        test_file_stream, middle_offset, SEEK_SET)
      == 0) {
    test_failure_count +=
      verify_file_position(test_file_stream, middle_offset);
  }

  if (fs_file_seek(test_file_stream, -1024, SEEK_CUR)
      == 0) {
    test_failure_count += verify_file_position(
      test_file_stream, middle_offset - 1024);
  }

  const int64_t file_end_offset = total_chunks * chunk_size;
  if (fs_file_seek(test_file_stream, 0, SEEK_END) == 0) {
    test_failure_count += verify_file_position(
      test_file_stream, file_end_offset);
  }

  const int64_t large_offset = 3000000000LL;
  if (fs_file_seek(test_file_stream, large_offset, SEEK_SET)
      == 0) {
    test_failure_count +=
      verify_file_position(test_file_stream, large_offset);
  }

  fclose(test_file_stream);
  remove(test_file_path);

  if (test_failure_count == 0) {
    printf("Все тесты fs_file_seek прошли успешно\n");
    return 0;
  }

  fprintf(stderr,
          "Найдено %d ошибок позиционирования\n",
          test_failure_count);
  return 1;
}