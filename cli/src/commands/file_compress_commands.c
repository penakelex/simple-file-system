#include "commands.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HUFFMAN_MAGIC 0x31465548U
#define HUFFMAN_SYMBOL_COUNT 256U
#define HUFFMAN_MAX_CODE_LENGTH 16U
#define HUFFMAN_HEADER_SIZE (4U + 4U + HUFFMAN_SYMBOL_COUNT)

typedef struct huffman_node {
  uint32_t frequency;
  int32_t symbol;
  int32_t left_child;
  int32_t right_child;
} huffman_node_t;

typedef struct huffman_code {
  uint32_t bits;
  uint8_t length;
} huffman_code_t;

typedef struct huffman_decode_table {
  uint32_t first_code[HUFFMAN_MAX_CODE_LENGTH + 1U];
  uint32_t code_count[HUFFMAN_MAX_CODE_LENGTH + 1U];
  uint8_t sorted_symbols[HUFFMAN_SYMBOL_COUNT];
  uint8_t max_code_length;
} huffman_decode_table_t;

static void
huffman_count_frequencies(const uint8_t* source_buffer,
                          const size_t source_length,
                          uint32_t* frequency_table) {
  memset(frequency_table,
         0,
         HUFFMAN_SYMBOL_COUNT * sizeof(uint32_t));
  for (size_t byte_index = 0; byte_index < source_length;
       ++byte_index) {
    frequency_table[source_buffer[byte_index]]++;
  }
}

static int32_t huffman_find_minimum_frequency(
  const huffman_node_t* node_array,
  const uint32_t node_count,
  const bool* is_used) {
  int32_t minimum_index = -1;
  uint32_t minimum_frequency = UINT32_MAX;

  for (uint32_t node_index = 0; node_index < node_count;
       ++node_index) {
    if (!is_used[node_index]
        && node_array[node_index].frequency > 0U) {
      if (node_array[node_index].frequency
          < minimum_frequency) {
        minimum_frequency =
          node_array[node_index].frequency;
        minimum_index = (int32_t)node_index;
      }
    }
  }

  return minimum_index;
}

static uint32_t
huffman_build_tree(const uint32_t* frequency_table,
                   huffman_node_t* node_array,
                   const uint32_t max_node_count) {
  uint32_t node_count = 0;
  bool* is_used = calloc(max_node_count, sizeof(bool));

  if (is_used == nullptr) {
    return 0;
  }

  for (uint32_t symbol = 0; symbol < HUFFMAN_SYMBOL_COUNT;
       ++symbol) {
    if (frequency_table[symbol] > 0U) {
      node_array[node_count].frequency =
        frequency_table[symbol];
      node_array[node_count].symbol = (int32_t)symbol;
      node_array[node_count].left_child = -1;
      node_array[node_count].right_child = -1;
      node_count++;
    }
  }

  if (node_count == 0U) {
    free(is_used);
    return 0;
  }

  if (node_count == 1U) {
    node_array[1].frequency = node_array[0].frequency;
    node_array[1].symbol = -1;
    node_array[1].left_child = 0;
    node_array[1].right_child = -1;
    free(is_used);
    return 2U;
  }

  while (true) {
    const int32_t first_minimum =
      huffman_find_minimum_frequency(
        node_array, node_count, is_used);

    if (first_minimum < 0) {
      break;
    }

    is_used[first_minimum] = true;

    const int32_t second_minimum =
      huffman_find_minimum_frequency(
        node_array, node_count, is_used);

    if (second_minimum < 0) {
      is_used[first_minimum] = false;
      break;
    }

    is_used[second_minimum] = true;

    if (node_count >= max_node_count) {
      break;
    }

    node_array[node_count].frequency =
      node_array[first_minimum].frequency
      + node_array[second_minimum].frequency;
    node_array[node_count].symbol = -1;
    node_array[node_count].left_child = first_minimum;
    node_array[node_count].right_child = second_minimum;
    node_count++;
  }

  free(is_used);
  return node_count;
}

static void huffman_compute_code_lengths_recursive(
  const huffman_node_t* node_array,
  const int32_t node_index,
  const uint8_t current_depth,
  uint8_t* code_length_table) {
  if (node_index < 0) {
    return;
  }

  const huffman_node_t* current_node =
    &node_array[node_index];

  if (current_node->symbol >= 0) {
    code_length_table[current_node->symbol] =
      (current_depth > 0U) ? current_depth : 1U;
    return;
  }

  if (current_depth >= HUFFMAN_MAX_CODE_LENGTH) {
    return;
  }

  huffman_compute_code_lengths_recursive(
    node_array,
    current_node->left_child,
    current_depth + 1U,
    code_length_table);
  huffman_compute_code_lengths_recursive(
    node_array,
    current_node->right_child,
    current_depth + 1U,
    code_length_table);
}

static void huffman_compute_code_lengths(
  const huffman_node_t* node_array,
  const uint32_t node_count,
  uint8_t* code_length_table) {
  memset(code_length_table, 0, HUFFMAN_SYMBOL_COUNT);

  if (node_count == 0U) {
    return;
  }

  const int32_t root_index = (int32_t)(node_count - 1U);
  huffman_compute_code_lengths_recursive(
    node_array, root_index, 0U, code_length_table);
}

static void huffman_generate_canonical_codes(
  const uint8_t* code_length_table,
  huffman_code_t* code_table) {
  uint32_t length_count[HUFFMAN_MAX_CODE_LENGTH + 1U] = {0};

  for (uint32_t symbol = 0; symbol < HUFFMAN_SYMBOL_COUNT;
       ++symbol) {
    const uint8_t length = code_length_table[symbol];

    if (length > 0U && length <= HUFFMAN_MAX_CODE_LENGTH) {
      length_count[length]++;
    }
  }

  uint32_t next_code[HUFFMAN_MAX_CODE_LENGTH + 1U] = {0};
  uint32_t code = 0;

  for (uint8_t length = 1U;
       length <= HUFFMAN_MAX_CODE_LENGTH;
       ++length) {
    code = (code + length_count[length - 1U]) << 1U;
    next_code[length] = code;
  }

  for (uint32_t symbol = 0; symbol < HUFFMAN_SYMBOL_COUNT;
       ++symbol) {
    const uint8_t length = code_length_table[symbol];

    if (length > 0U) {
      code_table[symbol].bits = next_code[length];
      code_table[symbol].length = length;
      next_code[length]++;
    } else {
      code_table[symbol].bits = 0;
      code_table[symbol].length = 0;
    }
  }
}

typedef struct bit_writer {
  uint8_t* output_buffer;
  size_t output_capacity;
  size_t byte_position;
  uint8_t bit_buffer;
  uint8_t bits_in_buffer;
} bit_writer_t;

static void
bit_writer_initialize(bit_writer_t* writer,
                      uint8_t* output_buffer,
                      const size_t output_capacity) {
  writer->output_buffer = output_buffer;
  writer->output_capacity = output_capacity;
  writer->byte_position = 0;
  writer->bit_buffer = 0;
  writer->bits_in_buffer = 0;
}

static bool bit_writer_write_bits(bit_writer_t* writer,
                                  const uint32_t bits,
                                  const uint8_t bit_count) {
  for (uint8_t bit_index = 0; bit_index < bit_count;
       ++bit_index) {
    const uint8_t current_bit =
      (uint8_t)((bits >> (bit_count - 1U - bit_index))
                & 1U);
    writer->bit_buffer =
      (uint8_t)(writer->bit_buffer
                | (current_bit
                   << (7U - writer->bits_in_buffer)));
    writer->bits_in_buffer++;

    if (writer->bits_in_buffer == 8U) {
      if (writer->byte_position
          >= writer->output_capacity) {
        return false;
      }

      writer->output_buffer[writer->byte_position] =
        writer->bit_buffer;
      writer->byte_position++;
      writer->bit_buffer = 0;
      writer->bits_in_buffer = 0;
    }
  }

  return true;
}

static size_t bit_writer_finalize(bit_writer_t* writer) {
  if (writer->bits_in_buffer > 0U) {
    if (writer->byte_position < writer->output_capacity) {
      writer->output_buffer[writer->byte_position] =
        writer->bit_buffer;
      writer->byte_position++;
    }

    writer->bit_buffer = 0;
    writer->bits_in_buffer = 0;
  }

  return writer->byte_position;
}

typedef struct bit_reader {
  const uint8_t* input_buffer;
  size_t input_length;
  size_t byte_position;
  uint8_t bit_buffer;
  uint8_t bits_in_buffer;
} bit_reader_t;

static void
bit_reader_initialize(bit_reader_t* reader,
                      const uint8_t* input_buffer,
                      const size_t input_length) {
  reader->input_buffer = input_buffer;
  reader->input_length = input_length;
  reader->byte_position = 0;
  reader->bit_buffer = 0;
  reader->bits_in_buffer = 0;
}

static int32_t bit_reader_read_bit(bit_reader_t* reader) {
  if (reader->bits_in_buffer == 0U) {
    if (reader->byte_position >= reader->input_length) {
      return -1;
    }

    reader->bit_buffer =
      reader->input_buffer[reader->byte_position];
    reader->byte_position++;
    reader->bits_in_buffer = 8U;
  }

  const int32_t bit =
    (int32_t)((reader->bit_buffer >> 7U) & 1U);
  reader->bit_buffer = (uint8_t)(reader->bit_buffer << 1U);
  reader->bits_in_buffer--;
  return bit;
}

static bool huffman_build_decode_table(
  const uint8_t* code_length_table,
  huffman_decode_table_t* decode_table) {
  memset(decode_table, 0, sizeof(huffman_decode_table_t));

  uint32_t length_count[HUFFMAN_MAX_CODE_LENGTH + 1U] = {0};
  for (uint32_t symbol = 0; symbol < HUFFMAN_SYMBOL_COUNT;
       ++symbol) {
    const uint8_t length = code_length_table[symbol];

    if (length > 0U && length <= HUFFMAN_MAX_CODE_LENGTH) {
      length_count[length]++;
      if (length > decode_table->max_code_length) {
        decode_table->max_code_length = length;
      }
    }
  }

  for (uint8_t length = 1U;
       length <= HUFFMAN_MAX_CODE_LENGTH;
       ++length) {
    decode_table->code_count[length] = length_count[length];
  }

  uint32_t code = 0;
  for (uint8_t length = 1U;
       length <= HUFFMAN_MAX_CODE_LENGTH;
       ++length) {
    decode_table->first_code[length] = code;
    code = (code + length_count[length]) << 1U;
  }

  uint32_t symbol_offset[HUFFMAN_MAX_CODE_LENGTH + 1U] = {
    0};
  uint32_t running_offset = 0;
  for (uint8_t length = 1U;
       length <= HUFFMAN_MAX_CODE_LENGTH;
       ++length) {
    symbol_offset[length] = running_offset;
    running_offset += length_count[length];
  }

  for (uint32_t symbol = 0; symbol < HUFFMAN_SYMBOL_COUNT;
       ++symbol) {
    const uint8_t length = code_length_table[symbol];

    if (length > 0U && length <= HUFFMAN_MAX_CODE_LENGTH) {
      const uint32_t position = symbol_offset[length];
      decode_table->sorted_symbols[position] =
        (uint8_t)symbol;
      symbol_offset[length]++;
    }
  }

  return true;
}

static int32_t huffman_decode_symbol(
  bit_reader_t* reader,
  const huffman_decode_table_t* decode_table) {
  uint32_t current_code = 0;

  for (uint8_t length = 1U;
       length <= decode_table->max_code_length;
       ++length) {
    const int32_t bit = bit_reader_read_bit(reader);

    if (bit < 0) {
      return -1;
    }

    current_code = (current_code << 1U) | (uint32_t)bit;

    const uint32_t offset =
      current_code - decode_table->first_code[length];

    if (offset < decode_table->code_count[length]) {
      uint32_t base_index = 0;

      for (uint8_t previous_length = 1U;
           previous_length < length;
           ++previous_length) {
        base_index +=
          decode_table->code_count[previous_length];
      }

      return (int32_t)
        decode_table->sorted_symbols[base_index + offset];
    }
  }

  return -1;
}

static size_t
huffman_compress(const uint8_t* source_buffer,
                 const size_t source_length,
                 uint8_t* destination_buffer,
                 const size_t destination_capacity) {
  if (destination_capacity < HUFFMAN_HEADER_SIZE) {
    return 0;
  }

  uint32_t frequency_table[HUFFMAN_SYMBOL_COUNT];
  huffman_count_frequencies(
    source_buffer, source_length, frequency_table);

  const uint32_t max_nodes = HUFFMAN_SYMBOL_COUNT * 2U;
  huffman_node_t* node_array =
    calloc(max_nodes, sizeof(huffman_node_t));

  if (node_array == nullptr) {
    return 0;
  }

  const uint32_t node_count = huffman_build_tree(
    frequency_table, node_array, max_nodes);

  uint8_t code_length_table[HUFFMAN_SYMBOL_COUNT];
  huffman_compute_code_lengths(
    node_array, node_count, code_length_table);
  free(node_array);

  const uint32_t magic = HUFFMAN_MAGIC;
  const uint32_t original_size = (uint32_t)source_length;
  memcpy(destination_buffer, &magic, sizeof(uint32_t));
  memcpy(destination_buffer + 4U,
         &original_size,
         sizeof(uint32_t));
  memcpy(destination_buffer + 8U,
         code_length_table,
         HUFFMAN_SYMBOL_COUNT);

  huffman_code_t code_table[HUFFMAN_SYMBOL_COUNT];
  huffman_generate_canonical_codes(code_length_table,
                                   code_table);

  bit_writer_t writer;
  bit_writer_initialize(
    &writer,
    destination_buffer + HUFFMAN_HEADER_SIZE,
    destination_capacity - HUFFMAN_HEADER_SIZE);

  for (size_t byte_index = 0; byte_index < source_length;
       ++byte_index) {
    const uint8_t symbol = source_buffer[byte_index];
    const huffman_code_t* code = &code_table[symbol];

    if (code->length == 0U) {
      continue;
    }

    if (!bit_writer_write_bits(
          &writer, code->bits, code->length)) {
      return 0;
    }
  }

  const size_t compressed_data_size =
    bit_writer_finalize(&writer);
  return HUFFMAN_HEADER_SIZE + compressed_data_size;
}

static size_t
huffman_decompress(const uint8_t* source_buffer,
                   const size_t source_length,
                   uint8_t* destination_buffer,
                   const size_t destination_capacity) {
  if (source_length < HUFFMAN_HEADER_SIZE) {
    return 0;
  }

  uint32_t magic = 0;
  uint32_t original_size = 0;
  memcpy(&magic, source_buffer, sizeof(uint32_t));
  memcpy(
    &original_size, source_buffer + 4U, sizeof(uint32_t));

  if (magic != HUFFMAN_MAGIC) {
    return 0;
  }

  if (original_size > destination_capacity) {
    return 0;
  }

  uint8_t code_length_table[HUFFMAN_SYMBOL_COUNT];
  memcpy(code_length_table,
         source_buffer + 8U,
         HUFFMAN_SYMBOL_COUNT);

  huffman_decode_table_t decode_table;

  if (!huffman_build_decode_table(code_length_table,
                                  &decode_table)) {
    return 0;
  }

  if (original_size == 0U) {
    return 0;
  }

  if (decode_table.max_code_length == 0U) {
    return 0;
  }

  bit_reader_t reader;
  bit_reader_initialize(
    &reader,
    source_buffer + HUFFMAN_HEADER_SIZE,
    source_length - HUFFMAN_HEADER_SIZE);

  for (uint32_t byte_index = 0; byte_index < original_size;
       ++byte_index) {
    const int32_t symbol =
      huffman_decode_symbol(&reader, &decode_table);

    if (symbol < 0) {
      return 0;
    }

    destination_buffer[byte_index] = (uint8_t)symbol;
  }

  return (size_t)original_size;
}

void cli_command_compress(cli_main_state_t* state,
                          const int argc,
                          char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: compress <path>\n");
    return;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_path,
    sizeof(absolute_path));

  fs_inode_t inode_info = {0};
  fs_status_t status =
    fs_vfs_get_info(state->fs_state.fs_context.vfs_context,
                    absolute_path,
                    &inode_info);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "compress: cannot get info for '%s'\n",
            absolute_path);
    return;
  }

  if (inode_info.size == 0) {
    printf("compress: file is empty\n");
    return;
  }

  int32_t file_descriptor = -1;
  status =
    fs_vfs_open(state->fs_state.fs_context.vfs_context,
                absolute_path,
                FS_OPEN_READ_ONLY,
                &file_descriptor);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "compress: cannot open '%s'\n",
            absolute_path);
    return;
  }

  uint8_t* original_data = malloc(inode_info.size);

  if (original_data == nullptr) {
    (void)fs_vfs_close(
      state->fs_state.fs_context.vfs_context,
      file_descriptor);
    fprintf(stderr, "compress: memory allocation failed\n");
    return;
  }

  size_t bytes_read = 0;
  status =
    fs_vfs_read(state->fs_state.fs_context.vfs_context,
                file_descriptor,
                original_data,
                inode_info.size,
                &bytes_read);
  (void)fs_vfs_close(state->fs_state.fs_context.vfs_context,
                     file_descriptor);

  if (status != FS_STATUS_OK
      || bytes_read != inode_info.size) {
    free(original_data);
    fprintf(stderr, "compress: read error\n");
    return;
  }

  const size_t max_compressed_size =
    inode_info.size + HUFFMAN_HEADER_SIZE + 1024U;
  uint8_t* compressed_data = malloc(max_compressed_size);

  if (compressed_data == nullptr) {
    free(original_data);
    fprintf(stderr, "compress: memory allocation failed\n");
    return;
  }

  const size_t compressed_size =
    huffman_compress(original_data,
                     bytes_read,
                     compressed_data,
                     max_compressed_size);
  free(original_data);

  if (compressed_size == 0) {
    free(compressed_data);
    fprintf(stderr, "compress: compression failed\n");
    return;
  }

  if (bytes_read < HUFFMAN_HEADER_SIZE) {
    fprintf(
      stderr,
      "Warning: file is smaller than compression header "
      "(%u bytes). "
      "Compressed file will be larger than the original.\n",
      HUFFMAN_HEADER_SIZE);
  } else if (compressed_size > bytes_read) {
    fprintf(stderr,
            "Warning: compressed size (%zu bytes) exceeds "
            "original (%zu bytes). "
            "File lacks sufficient redundancy for "
            "effective compression.\n",
            compressed_size,
            bytes_read);
  }

  char compressed_path[FS_MAX_PATH_LENGTH];
  snprintf(compressed_path,
           sizeof(compressed_path),
           "%s.sz",
           absolute_path);

  int32_t compressed_fd = -1;
  status = fs_vfs_open(
    state->fs_state.fs_context.vfs_context,
    compressed_path,
    FS_OPEN_CREATE | FS_OPEN_WRITE_ONLY | FS_OPEN_TRUNCATE,
    &compressed_fd);

  if (status != FS_STATUS_OK) {
    free(compressed_data);
    fprintf(stderr,
            "compress: cannot create compressed file\n");
    return;
  }

  size_t bytes_written = 0;
  status =
    fs_vfs_write(state->fs_state.fs_context.vfs_context,
                 compressed_fd,
                 compressed_data,
                 compressed_size,
                 &bytes_written);
  (void)fs_vfs_close(state->fs_state.fs_context.vfs_context,
                     compressed_fd);
  free(compressed_data);

  if (status != FS_STATUS_OK) {
    fprintf(stderr, "compress: write error\n");
    return;
  }

  const double ratio =
    (double)compressed_size / (double)bytes_read * 100.0;
  printf("Compressed '%s' -> '%s'\n",
         absolute_path,
         compressed_path);
  printf("  Original:   %u bytes\n", (uint32_t)bytes_read);
  printf("  Compressed: %zu bytes (%.1f%%)\n",
         compressed_size,
         ratio);
}

void cli_command_decompress(cli_main_state_t* state,
                            const int argc,
                            char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: decompress <path.sz>\n");
    return;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_path,
    sizeof(absolute_path));

  const size_t path_length = strlen(absolute_path);

  if (path_length < 3
      || strcmp(absolute_path + path_length - 3, ".sz")
           != 0) {
    fprintf(stderr,
            "decompress: file must have .sz extension\n");
    return;
  }

  fs_inode_t inode_info = {0};
  fs_status_t status =
    fs_vfs_get_info(state->fs_state.fs_context.vfs_context,
                    absolute_path,
                    &inode_info);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "decompress: cannot get info for '%s'\n",
            absolute_path);
    return;
  }

  int32_t file_descriptor = -1;
  status =
    fs_vfs_open(state->fs_state.fs_context.vfs_context,
                absolute_path,
                FS_OPEN_READ_ONLY,
                &file_descriptor);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "decompress: cannot open '%s'\n",
            absolute_path);
    return;
  }

  uint8_t* compressed_data = malloc(inode_info.size);

  if (compressed_data == nullptr) {
    (void)fs_vfs_close(
      state->fs_state.fs_context.vfs_context,
      file_descriptor);
    fprintf(stderr,
            "decompress: memory allocation failed\n");
    return;
  }

  size_t bytes_read = 0;
  status =
    fs_vfs_read(state->fs_state.fs_context.vfs_context,
                file_descriptor,
                compressed_data,
                inode_info.size,
                &bytes_read);
  (void)fs_vfs_close(state->fs_state.fs_context.vfs_context,
                     file_descriptor);

  if (status != FS_STATUS_OK
      || bytes_read != inode_info.size) {
    free(compressed_data);
    fprintf(stderr, "decompress: read error\n");
    return;
  }

  if (bytes_read < HUFFMAN_HEADER_SIZE) {
    free(compressed_data);
    fprintf(stderr,
            "decompress: file too small to be valid\n");
    return;
  }

  uint32_t original_size = 0;
  memcpy(
    &original_size, compressed_data + 4U, sizeof(uint32_t));

  const size_t max_decompressed_size =
    (size_t)original_size + 1024U;
  uint8_t* decompressed_data =
    malloc(max_decompressed_size);

  if (decompressed_data == nullptr) {
    free(compressed_data);
    fprintf(stderr,
            "decompress: memory allocation failed\n");
    return;
  }

  const size_t decompressed_size =
    huffman_decompress(compressed_data,
                       bytes_read,
                       decompressed_data,
                       max_decompressed_size);
  free(compressed_data);

  if (decompressed_size == 0) {
    free(decompressed_data);
    fprintf(
      stderr,
      "decompress: decompression failed (invalid data)\n");
    return;
  }

  char output_path[FS_MAX_PATH_LENGTH];
  strncpy(output_path, absolute_path, path_length - 3);
  output_path[path_length - 3] = '\0';

  int32_t output_fd = -1;
  status = fs_vfs_open(
    state->fs_state.fs_context.vfs_context,
    output_path,
    FS_OPEN_CREATE | FS_OPEN_WRITE_ONLY | FS_OPEN_TRUNCATE,
    &output_fd);

  if (status != FS_STATUS_OK) {
    free(decompressed_data);
    fprintf(stderr,
            "decompress: cannot create output file\n");
    return;
  }

  size_t bytes_written = 0;
  status =
    fs_vfs_write(state->fs_state.fs_context.vfs_context,
                 output_fd,
                 decompressed_data,
                 decompressed_size,
                 &bytes_written);
  (void)fs_vfs_close(state->fs_state.fs_context.vfs_context,
                     output_fd);
  free(decompressed_data);

  if (status != FS_STATUS_OK) {
    fprintf(stderr, "decompress: write error\n");
    return;
  }

  printf("Decompressed '%s' -> '%s'\n",
         absolute_path,
         output_path);
  printf("  Compressed:   %u bytes\n",
         (uint32_t)bytes_read);
  printf("  Decompressed: %zu bytes\n", decompressed_size);
}