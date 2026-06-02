#pragma once

#include <stddef.h>

#define CLI_HISTORY_MAX_ENTRIES 128U
#define CLI_HISTORY_MAX_LENGTH 512U

typedef struct cli_history {
  char entries[CLI_HISTORY_MAX_ENTRIES]
              [CLI_HISTORY_MAX_LENGTH];
  size_t entry_count;
  size_t browse_index;
} cli_history_t;

void cli_history_initialize(cli_history_t* history);
void cli_history_add_entry(cli_history_t* history,
                           const char* command_line);
[[nodiscard]] const char*
cli_history_browse_up(cli_history_t* history);
[[nodiscard]] const char*
cli_history_browse_down(cli_history_t* history);
void cli_history_reset_browse(cli_history_t* history);
[[nodiscard]] size_t
cli_history_get_entry_count(const cli_history_t* history);
[[nodiscard]] const char*
cli_history_get_entry(const cli_history_t* history,
                      const size_t entry_index);