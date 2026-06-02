#include "core/shell.h"
#include "commands.h"
#include "core/line_editor.h"
#include "core/terminal.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

void cli_shell_print_manager_prompt(
  const cli_manager_state_t* manager_state) {
  if (manager_state == nullptr) {
    return;
  }

  (void)fprintf(stdout,
                "fs-manager %s> ",
                manager_state->current_host_path);
  (void)fflush(stdout);
}

void cli_shell_print_fs_prompt(
  const cli_fs_state_t* fs_state) {
  if (fs_state == nullptr) {
    return;
  }

  (void)fprintf(stdout,
                "fs %s:%s> ",
                fs_state->disk_name,
                fs_state->current_fs_path);
  (void)fflush(stdout);
}

static void
cli_shell_build_prompt(const cli_main_state_t* state,
                       char* prompt_buffer,
                       const size_t prompt_buffer_size) {
  if (state->current_level == CLI_LEVEL_MANAGER) {
    (void)snprintf(prompt_buffer,
                   prompt_buffer_size,
                   "fs-manager %s> ",
                   state->manager_state.current_host_path);
  } else {
    (void)snprintf(prompt_buffer,
                   prompt_buffer_size,
                   "fs %s:%s> ",
                   state->fs_state.disk_name,
                   state->fs_state.current_fs_path);
  }
}

static const cli_command_entry_t*
cli_shell_find_command(const char* command_name,
                       const uint32_t current_level_mask) {
  for (size_t command_index = 0;
       command_index < cli_command_table_size;
       ++command_index) {
    const cli_command_entry_t* entry =
      &cli_command_table[command_index];

    if ((entry->level_mask & current_level_mask) == 0) {
      continue;
    }

    if (strcmp(command_name, entry->name) == 0) {
      return entry;
    }

    if (entry->aliases != nullptr) {
      for (size_t alias_index = 0;
           entry->aliases[alias_index] != nullptr;
           ++alias_index) {
        if (strcmp(command_name,
                   entry->aliases[alias_index])
            == 0) {
          return entry;
        }
      }
    }
  }

  return nullptr;
}

static void cli_shell_dispatch(cli_main_state_t* state,
                               const int argc,
                               char** argv) {
  if (argc == 0 || argv[0] == nullptr) {
    return;
  }

  const uint32_t current_level_mask =
    (state->current_level == CLI_LEVEL_MANAGER)
      ? CLI_LEVEL_1
      : CLI_LEVEL_2;

  const cli_command_entry_t* matched_command =
    cli_shell_find_command(argv[0], current_level_mask);

  if (matched_command == nullptr) {
    (void)fprintf(stderr,
                  "Unknown command: '%s'. Type 'help' for "
                  "available commands.\n",
                  argv[0]);
    return;
  }

  matched_command->handler(state, argc, argv);
}

void cli_shell_run(cli_main_state_t* state) {
  if (state == nullptr) {
    return;
  }

  state->is_running = true;
  cli_terminal_enable_raw_mode();

  char input_buffer[CLI_MAX_INPUT_LENGTH] = {0};
  char* token_buffer[CLI_MAX_ARGC];
  char prompt_buffer[CLI_MAX_PATH_LENGTH + 256] = {0};

  while (state->is_running) {
    cli_shell_build_prompt(
      state, prompt_buffer, sizeof(prompt_buffer));

    cli_history_t* active_history =
      (state->current_level == CLI_LEVEL_MANAGER)
        ? &state->manager_state.command_history
        : &state->fs_state.command_history;

    cli_terminal_disable_raw_mode();
    cli_terminal_enable_raw_mode();

    const bool line_valid =
      cli_line_editor_read(prompt_buffer,
                           input_buffer,
                           sizeof(input_buffer),
                           active_history);

    if (!line_valid) {
      break;
    }

    const size_t input_length = strlen(input_buffer);
    if (input_length == 0U) {
      continue;
    }

    cli_history_add_entry(active_history, input_buffer);

    (void)memset(token_buffer, 0, sizeof(token_buffer));
    const size_t token_count = cli_utils_tokenize_line(
      input_buffer, token_buffer, CLI_MAX_ARGC);

    if (token_count > 0U) {
      const cli_level_t level_before_dispatch =
        state->current_level;
      cli_shell_dispatch(
        state, (int)token_count, token_buffer);

      if (state->current_level != level_before_dispatch) {
        cli_line_editor_reset_render_state();
      }
    }
  }

  cli_terminal_disable_raw_mode();
}