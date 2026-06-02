#include "commands.h"
#include <stdio.h>

static const char* const exit_aliases[] = {
  "quit", "q", nullptr};
static const char* const help_aliases[] = {
  "h", "?", nullptr};
static const char* const list_aliases[] = {"dir", nullptr};
static const char* const close_aliases[] = {"umount",
                                            nullptr};
static const char* const history_aliases[] = {"hist",
                                              nullptr};
static const char* const mv_aliases[] = {"rename", nullptr};
static const char* const find_aliases[] = {"search",
                                           nullptr};
static const char* const ln_aliases[] = {"symlink",
                                         nullptr};

const cli_command_entry_t cli_command_table[] = {
  {"help",
   help_aliases,
   "Show available commands",
   cli_command_help,
   CLI_LEVEL_BOTH},
  {"exit",
   exit_aliases,
   "Exit program (or unmount FS)",
   cli_command_exit,
   CLI_LEVEL_BOTH},
  {"history",
   history_aliases,
   "Show command history",
   cli_command_history,
   CLI_LEVEL_BOTH},
  {"create",
   nullptr,
   "Create and format new disk: create <path> <clusters>",
   cli_command_create,
   CLI_LEVEL_1},
  {"format",
   nullptr,
   "Re-format existing disk: format <path> <clusters>",
   cli_command_format,
   CLI_LEVEL_1},
  {"open",
   nullptr,
   "Mount and open disk: open <path>",
   cli_command_open,
   CLI_LEVEL_1},
  {"list",
   list_aliases,
   "List disk images in current directory",
   cli_command_list,
   CLI_LEVEL_1},
  {"cd",
   nullptr,
   "Change host directory: host_cd <path>",
   cli_command_manager_cd,
   CLI_LEVEL_1},
  {"host_pwd",
   nullptr,
   "Print host working directory",
   cli_command_host_pwd,
   CLI_LEVEL_1},
  {"delete",
   nullptr,
   "Delete disk image: delete <path>",
   cli_command_delete,
   CLI_LEVEL_1},
  {"export_disk",
   nullptr,
   "Export disk image: export_disk <source> <destination>",
   cli_command_export_disk,
   CLI_LEVEL_1},
  {"import_disk",
   nullptr,
   "Import disk image: import_disk <host_path> [name]",
   cli_command_import_disk,
   CLI_LEVEL_1},
  {"disk_info",
   nullptr,
   "Show disk information: disk_info <path>",
   cli_command_disk_info,
   CLI_LEVEL_1},
  {"cd",
   nullptr,
   "Change directory: cd <path>",
   cli_command_fs_cd,
   CLI_LEVEL_2},
  {"ls",
   nullptr,
   "List directory: ls [path]",
   cli_command_ls,
   CLI_LEVEL_2},
  {"pwd",
   nullptr,
   "Print working directory",
   cli_command_pwd,
   CLI_LEVEL_2},
  {"tree",
   nullptr,
   "Show directory tree: tree [path]",
   cli_command_tree,
   CLI_LEVEL_2},
  {"mkdir",
   nullptr,
   "Create directory: mkdir <path>",
   cli_command_mkdir,
   CLI_LEVEL_2},
  {"rmdir",
   nullptr,
   "Remove directory: rmdir <path>",
   cli_command_rmdir,
   CLI_LEVEL_2},
  {"touch",
   nullptr,
   "Create empty file: touch <path>",
   cli_command_touch,
   CLI_LEVEL_2},
  {"rm",
   nullptr,
   "Remove file: rm <path>",
   cli_command_rm,
   CLI_LEVEL_2},
  {"cat",
   nullptr,
   "Print file contents: cat <path>",
   cli_command_cat,
   CLI_LEVEL_2},
  {"info",
   nullptr,
   "Show file metadata: info <path>",
   cli_command_info,
   CLI_LEVEL_2},
  {"cp",
   nullptr,
   "Copy file or directory recursively: cp <src> <dst>",
   cli_command_cp,
   CLI_LEVEL_2},
  {"mv",
   mv_aliases,
   "Rename/move: mv <src> <dst>",
   cli_command_mv,
   CLI_LEVEL_2},
  {"import",
   nullptr,
   "Import host -> FS: import <host> <fs>",
   cli_command_import,
   CLI_LEVEL_2},
  {"export",
   nullptr,
   "Export FS -> host: export <fs> <host>",
   cli_command_export,
   CLI_LEVEL_2},
  {"open",
   nullptr,
   "Open with system app: open_file <path>",
   cli_command_open_file,
   CLI_LEVEL_2},
  {"compress",
   nullptr,
   "Compress file (RLE): compress <path>",
   cli_command_compress,
   CLI_LEVEL_2},
  {"decompress",
   nullptr,
   "Decompress .sz file: decompress <path.sz>",
   cli_command_decompress,
   CLI_LEVEL_2},
  {"df",
   nullptr,
   "Show file system usage statistics",
   cli_command_df,
   CLI_LEVEL_2},
  {"find",
   find_aliases,
   "Search for files by name: find <pattern> [start_path]",
   cli_command_find,
   CLI_LEVEL_2},
  {"echo",
   nullptr,
   "Write text to a file: echo <text...> <path>",
   cli_command_echo,
   CLI_LEVEL_2},
  {"ln",
   ln_aliases,
   "Create symbolic link: ln <target> <link_name>",
   cli_command_ln,
   CLI_LEVEL_2},
  {"close",
   close_aliases,
   "Unmount current FS",
   cli_command_close,
   CLI_LEVEL_2},
  {"defrag",
   nullptr,
   "Defragment file system",
   cli_command_defrag,
   CLI_LEVEL_2},
  {"bitmap_dump",
   nullptr,
   "Show cluster bitmap visual map",
   cli_command_bitmap_dump,
   CLI_LEVEL_2},
};

const size_t cli_command_table_size =
  sizeof(cli_command_table) / sizeof(cli_command_table[0]);

void cli_command_help(cli_main_state_t* state,
                      const int argc,
                      char** argv) {
  (void)argc;
  (void)argv;
  const uint32_t current_level_mask =
    (state->current_level == CLI_LEVEL_MANAGER)
      ? CLI_LEVEL_1
      : CLI_LEVEL_2;

  printf("Available commands:\n");

  for (size_t index = 0; index < cli_command_table_size;
       ++index) {
    const cli_command_entry_t* entry =
      &cli_command_table[index];

    if ((entry->level_mask & current_level_mask) == 0) {
      continue;
    }

    printf("  %-14s - %s", entry->name, entry->description);

    if (entry->aliases != nullptr) {
      printf(" (aliases:");

      for (size_t alias_index = 0;
           entry->aliases[alias_index] != nullptr;
           ++alias_index) {
        printf(" %s", entry->aliases[alias_index]);
      }

      printf(")");
    }

    printf("\n");
  }
}

void cli_command_exit(cli_main_state_t* state,
                      const int argc,
                      char** argv) {
  (void)argc;
  (void)argv;

  if (state->current_level == CLI_LEVEL_FS) {
    cli_command_close(state, 0, nullptr);
    return;
  }

  state->is_running = false;
}

void cli_command_history(cli_main_state_t* state,
                         const int argc,
                         char** argv) {
  (void)argc;
  (void)argv;
  const cli_history_t* active_history =
    (state->current_level == CLI_LEVEL_MANAGER)
      ? &state->manager_state.command_history
      : &state->fs_state.command_history;

  const size_t count =
    cli_history_get_entry_count(active_history);

  if (count == 0) {
    printf("History is empty.\n");
    return;
  }

  for (size_t index = 0; index < count; ++index) {
    const char* entry =
      cli_history_get_entry(active_history, index);
    printf("  %3zu  %s\n",
           index + 1U,
           entry != nullptr ? entry : "");
  }
}

void cli_command_close(cli_main_state_t* state,
                       const int argc,
                       char** argv) {
  (void)argc;
  (void)argv;

  if (state->current_level != CLI_LEVEL_FS) {
    fprintf(stderr, "close: no file system is mounted\n");
    return;
  }

  if (state->fs_state.is_mounted) {
    printf("Unmounting disk '%s'...\n",
           state->fs_state.disk_name);
    const fs_status_t unmount_status =
      cli_unmount_disk(&state->fs_state.fs_context);

    if (unmount_status != FS_STATUS_OK) {
      fprintf(stderr,
              "Warning: unmount returned status %d\n",
              unmount_status);
    }

    state->fs_state.is_mounted = false;
  }

  state->fs_state.current_fs_path[0] = '/';
  state->fs_state.current_fs_path[1] = '\0';
  state->fs_state.disk_name[0] = '\0';
  state->current_level = CLI_LEVEL_MANAGER;
  printf("Returned to manager level.\n");
}