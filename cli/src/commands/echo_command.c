#include "commands.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

#define ECHO_BUFFER_SIZE 4096U

void cli_command_echo(cli_main_state_t* state,
                      const int argc,
                      char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: echo <text...> <path>\n");
    fprintf(stderr,
            "  Writes text to a file. The last argument is "
            "the file path,\n");
    fprintf(
      stderr,
      "  all previous arguments are joined as text.\n");
    fprintf(stderr,
            "  Note: No newline is added automatically.\n");
    fprintf(stderr,
            "  Example: echo \"Hello, World!\\n\" "
            "/greeting.txt\n");
    return;
  }

  if (state->current_level != CLI_LEVEL_FS) {
    fprintf(stderr, "echo: no file system is mounted\n");
    return;
  }

  const char* file_path_argument = argv[argc - 1];

  char text_buffer[ECHO_BUFFER_SIZE];
  memset(text_buffer, 0, sizeof(text_buffer));
  size_t text_length = 0U;

  for (int argument_index = 1; argument_index < argc - 1;
       ++argument_index) {
    if (argument_index > 1
        && text_length < sizeof(text_buffer) - 1U) {
      text_buffer[text_length] = ' ';
      text_length++;
    }

    const size_t argument_length =
      strlen(argv[argument_index]);
    const size_t available_space =
      sizeof(text_buffer) - text_length - 2U;
    const size_t bytes_to_copy =
      (argument_length < available_space) ? argument_length
                                          : available_space;
    if (bytes_to_copy > 0U) {
      memcpy(text_buffer + text_length,
             argv[argument_index],
             bytes_to_copy);
      text_length += bytes_to_copy;
    }
  }

  text_buffer[text_length] = '\0';

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    file_path_argument,
    absolute_path,
    sizeof(absolute_path));

  const fs_status_t ensure_status =
    cli_utils_ensure_parent_directories(
      state->fs_state.fs_context.vfs_context,
      absolute_path);
  if (ensure_status != FS_STATUS_OK) {
    fprintf(
      stderr,
      "echo: cannot create parent directories for '%s' "
      "(status: %d)\n",
      absolute_path,
      ensure_status);
    return;
  }

  int32_t file_descriptor = -1;
  fs_status_t status = fs_vfs_open(
    state->fs_state.fs_context.vfs_context,
    absolute_path,
    FS_OPEN_CREATE | FS_OPEN_WRITE_ONLY | FS_OPEN_TRUNCATE,
    &file_descriptor);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "echo: cannot open '%s' (status: %d)\n",
            absolute_path,
            status);
    return;
  }

  size_t bytes_written = 0U;
  status =
    fs_vfs_write(state->fs_state.fs_context.vfs_context,
                 file_descriptor,
                 text_buffer,
                 text_length,
                 &bytes_written);
  (void)fs_vfs_close(state->fs_state.fs_context.vfs_context,
                     file_descriptor);

  if (status != FS_STATUS_OK) {
    fprintf(
      stderr, "echo: write error (status: %d)\n", status);
    return;
  }

  printf("Wrote %zu bytes to '%s'\n",
         bytes_written,
         absolute_path);
}