#include "core/shell.h"
#include "utils.h"
#include <stdio.h>

int main() {
  cli_main_state_t main_state = {0};
  main_state.current_level = CLI_LEVEL_MANAGER;
  main_state.is_running = true;

  cli_history_initialize(
    &main_state.manager_state.command_history);
  cli_history_initialize(
    &main_state.fs_state.command_history);

  cli_utils_get_current_host_directory(
    main_state.manager_state.current_host_path,
    sizeof(main_state.manager_state.current_host_path));

  (void)fprintf(stdout,
                "Simple File System Manager\n"
                "Type 'help' for available commands, "
                "'exit' to quit.\n\n");

  cli_shell_run(&main_state);

  (void)fprintf(stdout, "Goodbye!\n");
  return 0;
}