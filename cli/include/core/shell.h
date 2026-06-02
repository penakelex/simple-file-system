#pragma once

#include "context.h"
#include "core/history.h"

#define CLI_MAX_INPUT_LENGTH 2048U
#define CLI_MAX_ARGC 32U
#define CLI_MAX_PATH_LENGTH 1024U
#define CLI_MAX_DISK_NAME_LENGTH 256U

typedef enum {
  CLI_LEVEL_MANAGER = 1,
  CLI_LEVEL_FS = 2
} cli_level_t;

typedef struct cli_manager_state {
  char current_host_path[CLI_MAX_PATH_LENGTH];
  cli_history_t command_history;
} cli_manager_state_t;

typedef struct cli_fs_state {
  char disk_name[CLI_MAX_DISK_NAME_LENGTH];
  char current_fs_path[CLI_MAX_PATH_LENGTH];
  cli_context_t fs_context;
  bool is_mounted;
  cli_history_t command_history;
} cli_fs_state_t;

typedef struct cli_main_state {
  cli_level_t current_level;
  cli_manager_state_t manager_state;
  cli_fs_state_t fs_state;
  bool is_running;
} cli_main_state_t;

void cli_shell_run(cli_main_state_t* state);
void cli_shell_print_manager_prompt(
  const cli_manager_state_t* manager_state);
void cli_shell_print_fs_prompt(
  const cli_fs_state_t* fs_state);