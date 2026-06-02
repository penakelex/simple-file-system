#include "commands.h"
#include "fs/config.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on

#else
#include <sys/wait.h>
#include <unistd.h>
#endif

void cli_command_import(cli_main_state_t* state,
                        const int argc,
                        char** argv) {
  if (argc < 3) {
    fprintf(stderr,
            "Usage: import <host_path> <fs_path>\n");
    return;
  }

  const char* host_path = argv[1];
  char absolute_fs_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[2],
    absolute_fs_path,
    sizeof(absolute_fs_path));

  const fs_status_t ensure_status =
    cli_utils_ensure_parent_directories(
      state->fs_state.fs_context.vfs_context,
      absolute_fs_path);

  if (ensure_status != FS_STATUS_OK) {
    fprintf(stderr,
            "import: cannot create parent directories for "
            "'%s' (status: %d)\n",
            absolute_fs_path,
            ensure_status);
    return;
  }

  FILE* host_file = fopen(host_path, "rb");

  if (host_file == nullptr) {
    fprintf(stderr,
            "import: cannot open host file '%s'\n",
            host_path);
    return;
  }

  int32_t file_descriptor = -1;
  fs_status_t status = fs_vfs_open(
    state->fs_state.fs_context.vfs_context,
    absolute_fs_path,
    FS_OPEN_CREATE | FS_OPEN_WRITE_ONLY | FS_OPEN_TRUNCATE,
    &file_descriptor);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "import: cannot create fs file (status: %d)\n",
            status);
    (void)fclose(host_file);
    return;
  }

  uint8_t buffer[FS_CLUSTER_SIZE];
  size_t bytes_read = 0;
  size_t total_written = 0;

  while ((bytes_read =
            fread(buffer, 1, sizeof(buffer), host_file))
         > 0) {
    size_t bytes_written = 0;
    status =
      fs_vfs_write(state->fs_state.fs_context.vfs_context,
                   file_descriptor,
                   buffer,
                   bytes_read,
                   &bytes_written);

    if (status != FS_STATUS_OK
        || bytes_written != bytes_read) {
      if (status == FS_STATUS_ERROR_NO_SPACE) {
        fprintf(stderr,
                "import: no space left on device\n");
      } else {
        fprintf(stderr,
                "import: write error (status: %d)\n",
                status);
      }
      break;
    }

    total_written += bytes_written;
  }

  (void)fs_vfs_close(state->fs_state.fs_context.vfs_context,
                     file_descriptor);
  (void)fclose(host_file);
  printf("Imported %zu bytes from '%s' to '%s'\n",
         total_written,
         host_path,
         absolute_fs_path);
}

void cli_command_export(cli_main_state_t* state,
                        const int argc,
                        char** argv) {
  if (argc < 3) {
    fprintf(stderr,
            "Usage: export <fs_path> <host_path>\n");
    return;
  }

  char absolute_fs_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_fs_path,
    sizeof(absolute_fs_path));
  const char* host_path = argv[2];

  int32_t file_descriptor = -1;
  fs_status_t status =
    fs_vfs_open(state->fs_state.fs_context.vfs_context,
                absolute_fs_path,
                FS_OPEN_READ_ONLY,
                &file_descriptor);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "export: cannot open fs file (status: %d)\n",
            status);
    return;
  }

  FILE* host_file = fopen(host_path, "wb");

  if (host_file == nullptr) {
    fprintf(stderr,
            "export: cannot create host file '%s'\n",
            host_path);
    (void)fs_vfs_close(
      state->fs_state.fs_context.vfs_context,
      file_descriptor);
    return;
  }

  uint8_t buffer[FS_CLUSTER_SIZE];
  size_t bytes_read = 0;
  size_t total_exported = 0;

  while (true) {
    status =
      fs_vfs_read(state->fs_state.fs_context.vfs_context,
                  file_descriptor,
                  buffer,
                  sizeof(buffer),
                  &bytes_read);
    if (status != FS_STATUS_OK || bytes_read == 0) {
      break;
    }
    (void)fwrite(buffer, 1, bytes_read, host_file);
    total_exported += bytes_read;
  }

  (void)fs_vfs_close(state->fs_state.fs_context.vfs_context,
                     file_descriptor);
  (void)fclose(host_file);
  printf("Exported %zu bytes from '%s' to '%s'\n",
         total_exported,
         absolute_fs_path,
         host_path);
}

void cli_command_open_file(cli_main_state_t* state,
                           const int argc,
                           char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: open <path>\n");
    return;
  }

  char absolute_path[FS_MAX_PATH_LENGTH];
  cli_utils_build_absolute_path(
    state->fs_state.current_fs_path,
    argv[1],
    absolute_path,
    sizeof(absolute_path));
  char extension[64] = {0};
  const char* last_dot = strrchr(absolute_path, '.');

  if (last_dot != nullptr
      && last_dot > strrchr(absolute_path, '/')) {
    strncpy(extension, last_dot, sizeof(extension) - 1U);
  }

  char temp_host_path[512];
#ifdef _WIN32
  const char* temp_dir = getenv("TEMP");

  if (temp_dir == nullptr) {
    temp_dir = ".";
  }

  snprintf(temp_host_path,
           sizeof(temp_host_path),
           "%s\\fs_temp_%u%s",
           temp_dir,
           (uint32_t)time(nullptr),
           extension);
#else
  snprintf(temp_host_path,
           sizeof(temp_host_path),
           "/tmp/fs_temp_%u%s",
           (uint32_t)time(nullptr),
           extension);
#endif
  int32_t file_descriptor = -1;
  fs_status_t status =
    fs_vfs_open(state->fs_state.fs_context.vfs_context,
                absolute_path,
                FS_OPEN_READ_ONLY,
                &file_descriptor);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "open: cannot open '%s' (status: %d)\n",
            absolute_path,
            status);
    return;
  }

  FILE* host_file = fopen(temp_host_path, "wb");

  if (host_file == nullptr) {
    (void)fs_vfs_close(
      state->fs_state.fs_context.vfs_context,
      file_descriptor);
    fprintf(stderr, "open: cannot create temp file\n");
    return;
  }

  uint8_t buffer[FS_CLUSTER_SIZE];
  size_t bytes_read = 0;

  while (true) {
    status =
      fs_vfs_read(state->fs_state.fs_context.vfs_context,
                  file_descriptor,
                  buffer,
                  sizeof(buffer),
                  &bytes_read);

    if (status != FS_STATUS_OK || bytes_read == 0) {
      break;
    }

    (void)fwrite(buffer, 1, bytes_read, host_file);
  }

  (void)fs_vfs_close(state->fs_state.fs_context.vfs_context,
                     file_descriptor);
  (void)fclose(host_file);
  printf("Exported to temp: %s\n", temp_host_path);
#ifdef _WIN32
  const intptr_t result =
    (intptr_t)ShellExecuteA(nullptr,
                            "open",
                            temp_host_path,
                            nullptr,
                            nullptr,
                            SW_SHOW);

  if (result <= 32) {
    fprintf(stderr,
            "open: ShellExecute failed (error: %ld)\n",
            (long)result);
  }
#else
  const pid_t child_pid = fork();
  if (child_pid < 0) {
    fprintf(stderr, "open: fork failed\n");
    return;
  }
  if (child_pid == 0) {
    (void)close(STDIN_FILENO);
#ifdef __APPLE__
    (void)execlp(
      "open", "open", temp_host_path, (char*)nullptr);
#else
    (void)execlp("xdg-open",
                 "xdg-open",
                 temp_host_path,
                 (char*)nullptr);
#endif
    _exit(127);
  }
#endif
}