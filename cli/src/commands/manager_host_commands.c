#include "commands.h"
#include "fs/storage/disk.h"
#include "fs/storage/superblock.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define CLI_COPY_BUFFER_SIZE (64U * 1024U)

void cli_command_list(cli_main_state_t* state,
                      const int argc,
                      char** argv) {
  (void)argc;
  (void)argv;
  printf("Disk images in %s:\n",
         state->manager_state.current_host_path);

#ifdef _WIN32
  WIN32_FIND_DATAA find_data;
  char search_pattern[CLI_MAX_PATH_LENGTH];
  snprintf(search_pattern,
           sizeof(search_pattern),
           "%s\\*.*",
           state->manager_state.current_host_path);
  const HANDLE find_handle =
    FindFirstFileA(search_pattern, &find_data);

  if (find_handle == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "list: cannot read directory\n");
    return;
  }

  uint32_t file_count = 0;
  do {
    if ((find_data.dwFileAttributes
         & FILE_ATTRIBUTE_DIRECTORY)
        != 0) {
      continue;
    }

    const char* dot = strrchr(find_data.cFileName, '.');

    if (dot != nullptr && _stricmp(dot, ".sfs") == 0) {
      const uint64_t size_mb =
        ((uint64_t)find_data.nFileSizeHigh << 32
         | find_data.nFileSizeLow)
        / (1024U * 1024U);
      printf("  %-40s  (%llu MB)\n",
             find_data.cFileName,
             (unsigned long long)size_mb);
      file_count++;
    }
  } while (FindNextFileA(find_handle, &find_data) != 0);

  FindClose(find_handle);

  if (file_count == 0) {
    printf("  (no .sfs files found)\n");
  }
#else
  DIR* directory_stream =
    opendir(state->manager_state.current_host_path);

  if (directory_stream == nullptr) {
    fprintf(stderr, "list: cannot read directory\n");
    return;
  }

  uint32_t file_count = 0;
  struct dirent* entry = nullptr;

  while ((entry = readdir(directory_stream)) != nullptr) {
    if (entry->d_type != DT_REG
        && entry->d_type != DT_UNKNOWN) {
      continue;
    }

    const char* dot = strrchr(entry->d_name, '.');

    if (dot != nullptr && strcasecmp(dot, ".sfs") == 0) {
      char full_path[CLI_MAX_PATH_LENGTH];
      snprintf(full_path,
               sizeof(full_path),
               "%s/%s",
               state->manager_state.current_host_path,
               entry->d_name);
      struct stat file_stat;

      if (stat(full_path, &file_stat) == 0) {
        const uint64_t size_mb =
          (uint64_t)file_stat.st_size / (1024U * 1024U);
        printf("  %-40s  (%llu MB)\n",
               entry->d_name,
               (unsigned long long)size_mb);
      } else {
        printf("  %s\n", entry->d_name);
      }

      file_count++;
    }
  }

  closedir(directory_stream);

  if (file_count == 0) {
    printf("  (no .sfs files found)\n");
  }
#endif
}

void cli_command_manager_cd(cli_main_state_t* state,
                            const int argc,
                            char** argv) {
  if (argc < 2) {
    const char* home_dir = getenv("HOME");

    if (home_dir == nullptr) {
      home_dir = getenv("USERPROFILE");
    }

    if (home_dir == nullptr) {
      home_dir = "/";
    }
#ifdef _WIN32
    if (SetCurrentDirectoryA(home_dir) == 0) {
      return;
    }
#else
    if (chdir(home_dir) != 0) {
      return;
    }
#endif
    cli_utils_get_current_host_directory(
      state->manager_state.current_host_path,
      sizeof(state->manager_state.current_host_path));
    return;
  }

  const char* target_path = argv[1];

#ifdef _WIN32
  if (SetCurrentDirectoryA(target_path) == 0) {
    fprintf(
      stderr, "cd: cannot change to '%s'\n", target_path);
    return;
  }
#else
  if (chdir(target_path) != 0) {
    fprintf(
      stderr, "cd: cannot change to '%s'\n", target_path);
    return;
  }
#endif
  cli_utils_get_current_host_directory(
    state->manager_state.current_host_path,
    sizeof(state->manager_state.current_host_path));
}

void cli_command_host_pwd(cli_main_state_t* state,
                          const int argc,
                          char** argv) {
  (void)argc;
  (void)argv;
  printf("%s\n", state->manager_state.current_host_path);
}

void cli_command_export_disk(cli_main_state_t* state,
                             const int argc,
                             char** argv) {
  (void)state;

  if (argc < 3) {
    fprintf(stderr,
            "Usage: export_disk <source_disk_path> "
            "<destination_path>\n");
    fprintf(stderr,
            "Example: export_disk my_disk.sfs "
            "/backup/my_disk_backup.sfs\n");
    return;
  }

  const char* source_path = argv[1];
  const char* destination_path = argv[2];

  FILE* source_file = fopen(source_path, "rb");

  if (source_file == nullptr) {
    fprintf(stderr,
            "export_disk: cannot open source file '%s'\n",
            source_path);
    return;
  }

  FILE* destination_file = fopen(destination_path, "wb");

  if (destination_file == nullptr) {
    fprintf(
      stderr,
      "export_disk: cannot create destination file '%s'\n",
      destination_path);
    (void)fclose(source_file);
    return;
  }

  uint8_t* copy_buffer = malloc(CLI_COPY_BUFFER_SIZE);

  if (copy_buffer == nullptr) {
    fprintf(stderr,
            "export_disk: memory allocation failed\n");
    (void)fclose(source_file);
    (void)fclose(destination_file);
    return;
  }

  size_t bytes_read = 0;
  size_t total_copied = 0;
  bool copy_failed = false;

  while (
    (bytes_read = fread(
       copy_buffer, 1, CLI_COPY_BUFFER_SIZE, source_file))
    > 0) {
    const size_t bytes_written =
      fwrite(copy_buffer, 1, bytes_read, destination_file);

    if (bytes_written != bytes_read) {
      fprintf(stderr,
              "export_disk: write error to destination\n");
      copy_failed = true;
      break;
    }

    total_copied += bytes_written;
  }

  if (ferror(source_file) != 0) {
    fprintf(stderr,
            "export_disk: read error from source\n");
    copy_failed = true;
  }

  free(copy_buffer);
  (void)fclose(source_file);
  (void)fclose(destination_file);

  if (copy_failed) {
    (void)remove(destination_path);
    return;
  }

  printf(
    "Exported disk image: %zu bytes from '%s' to '%s'\n",
    total_copied,
    source_path,
    destination_path);
}

void cli_command_import_disk(cli_main_state_t* state,
                             const int argc,
                             char** argv) {
  (void)state;

  if (argc < 2) {
    fprintf(stderr,
            "Usage: import_disk <host_path> "
            "[destination_name]\n");
    fprintf(stderr,
            "  host_path       - path to disk image on "
            "host system\n");
    fprintf(stderr,
            "  destination_name - optional name for "
            "imported disk (default: same as source)\n");
    fprintf(stderr,
            "Example: import_disk /backup/my_disk.sfs "
            "local_copy.sfs\n");
    return;
  }

  const char* host_path = argv[1];

  char destination_name[CLI_MAX_PATH_LENGTH];

  if (argc >= 3) {
    strncpy(destination_name,
            argv[2],
            sizeof(destination_name) - 1U);
    destination_name[sizeof(destination_name) - 1U] = '\0';
  } else {
    cli_utils_extract_disk_name_from_path(
      host_path,
      destination_name,
      sizeof(destination_name));
  }

  char destination_path[CLI_MAX_PATH_LENGTH];
#ifdef _WIN32
  snprintf(destination_path,
           sizeof(destination_path),
           "%s\\%s",
           state->manager_state.current_host_path,
           destination_name);
#else
  snprintf(destination_path,
           sizeof(destination_path),
           "%s/%s",
           state->manager_state.current_host_path,
           destination_name);
#endif

  if (cli_utils_file_exists(destination_path)) {
    fprintf(
      stderr,
      "import_disk: destination file '%s' already exists\n",
      destination_path);
    fprintf(stderr,
            "Delete it first or specify a different "
            "destination name\n");
    return;
  }

  FILE* source_file = fopen(host_path, "rb");

  if (source_file == nullptr) {
    fprintf(stderr,
            "import_disk: cannot open host file '%s'\n",
            host_path);
    return;
  }

  FILE* destination_file = fopen(destination_path, "wb");

  if (destination_file == nullptr) {
    fprintf(
      stderr,
      "import_disk: cannot create destination file '%s'\n",
      destination_path);
    (void)fclose(source_file);
    return;
  }

  uint8_t* copy_buffer = malloc(CLI_COPY_BUFFER_SIZE);

  if (copy_buffer == nullptr) {
    fprintf(stderr,
            "import_disk: memory allocation failed\n");
    (void)fclose(source_file);
    (void)fclose(destination_file);
    return;
  }

  size_t bytes_read = 0;
  size_t total_copied = 0;
  bool copy_failed = false;

  while (
    (bytes_read = fread(
       copy_buffer, 1, CLI_COPY_BUFFER_SIZE, source_file))
    > 0) {
    const size_t bytes_written =
      fwrite(copy_buffer, 1, bytes_read, destination_file);

    if (bytes_written != bytes_read) {
      fprintf(stderr,
              "import_disk: write error to destination\n");
      copy_failed = true;
      break;
    }

    total_copied += bytes_written;
  }

  if (ferror(source_file) != 0) {
    fprintf(stderr,
            "import_disk: read error from host file\n");
    copy_failed = true;
  }

  free(copy_buffer);
  (void)fclose(source_file);
  (void)fclose(destination_file);

  if (copy_failed) {
    (void)remove(destination_path);
    return;
  }

  printf(
    "Imported disk image: %zu bytes from '%s' to '%s'\n",
    total_copied,
    host_path,
    destination_path);
  printf("You can now use 'open %s' to mount it.\n",
         destination_name);
}

void cli_command_disk_info(cli_main_state_t* state,
                           const int argc,
                           char** argv) {
  (void)state;

  if (argc < 2) {
    fprintf(stderr, "Usage: disk_info <path>\n");
    return;
  }

  const char* disk_path = argv[1];

  fs_disk_t* temp_disk = nullptr;
  fs_status_t status =
    fs_disk_create_or_open(&temp_disk, disk_path, 1U, true);

  if (status != FS_STATUS_OK) {
    fprintf(stderr,
            "disk_info: cannot open '%s' (status: %d)\n",
            disk_path,
            status);
    return;
  }

  fs_superblock_t superblock = {0};
  status =
    fs_superblock_read_from_disk(temp_disk, &superblock);
  const uint32_t total_clusters =
    fs_disk_get_total_cluster_count(temp_disk);
  (void)fs_disk_close(temp_disk);

  if (status == FS_STATUS_OK
      && superblock.total_clusters > 0) {
    status =
      fs_disk_create_or_open(&temp_disk,
                             disk_path,
                             superblock.total_clusters,
                             true);

    if (status == FS_STATUS_OK) {
      (void)fs_disk_close(temp_disk);
    }
  }

  printf("Disk info for '%s':\n", disk_path);

  if (status != FS_STATUS_OK) {
    printf("  (not a valid file system or corrupted)\n");
    return;
  }

  const uint64_t total_bytes =
    (uint64_t)superblock.total_clusters * FS_CLUSTER_SIZE;
  printf("  Magic:             0x%08X (%s)\n",
         superblock.magic,
         superblock.magic == FS_MAGIC_NUMBER ? "valid"
                                             : "INVALID");
  printf("  Total clusters:    %u\n",
         superblock.total_clusters);
  printf("  Cluster size:      %u bytes\n",
         FS_CLUSTER_SIZE);
  printf("  Total size:        %.2f MB\n",
         (double)total_bytes / (1024.0 * 1024.0));
  printf("  Root inode:        %u\n",
         superblock.root_inode_id);
  printf("  Inode table start: cluster %u (%u clusters)\n",
         superblock.inode_table_start_cluster,
         superblock.inode_table_size_clusters);
  printf("  Data start:        cluster %u\n",
         superblock.data_start_cluster);
  (void)total_clusters;
}