#include "core/history.h"
#include <string.h>

void cli_history_initialize(cli_history_t* history) {
  if (history == nullptr) {
    return;
  }

  memset(history, 0, sizeof(cli_history_t));
}

void cli_history_add_entry(cli_history_t* history,
                           const char* command_line) {
  if (history == nullptr || command_line == nullptr) {
    return;
  }

  const size_t line_length = strlen(command_line);

  if (line_length == 0) {
    return;
  }

  if (history->entry_count > 0) {
    const size_t last_index = history->entry_count - 1U;

    if (strcmp(history->entries[last_index], command_line)
        == 0) {
      history->browse_index = history->entry_count;
      return;
    }
  }

  if (history->entry_count >= CLI_HISTORY_MAX_ENTRIES) {
    for (size_t shift_index = 0;
         shift_index < CLI_HISTORY_MAX_ENTRIES - 1U;
         ++shift_index) {
      memcpy(history->entries[shift_index],
             history->entries[shift_index + 1U],
             CLI_HISTORY_MAX_LENGTH);
    }

    history->entry_count = CLI_HISTORY_MAX_ENTRIES - 1U;
  }

  strncpy(history->entries[history->entry_count],
          command_line,
          CLI_HISTORY_MAX_LENGTH - 1U);
  history->entries[history->entry_count]
                  [CLI_HISTORY_MAX_LENGTH - 1U] = '\0';
  history->entry_count++;
  history->browse_index = history->entry_count;
}

const char* cli_history_browse_up(cli_history_t* history) {
  if (history == nullptr || history->entry_count == 0) {
    return nullptr;
  }

  if (history->browse_index == 0) {
    return history->entries[0];
  }

  history->browse_index--;
  return history->entries[history->browse_index];
}

const char*
cli_history_browse_down(cli_history_t* history) {
  if (history == nullptr || history->entry_count == 0) {
    return nullptr;
  }

  if (history->browse_index >= history->entry_count) {
    return "";
  }

  history->browse_index++;
  if (history->browse_index >= history->entry_count) {
    return "";
  }

  return history->entries[history->browse_index];
}

void cli_history_reset_browse(cli_history_t* history) {
  if (history == nullptr) {
    return;
  }

  history->browse_index = history->entry_count;
}

size_t
cli_history_get_entry_count(const cli_history_t* history) {
  if (history == nullptr) {
    return 0;
  }

  return history->entry_count;
}

const char*
cli_history_get_entry(const cli_history_t* history,
                      const size_t entry_index) {
  if (history == nullptr
      || entry_index >= history->entry_count) {
    return nullptr;
  }

  return history->entries[entry_index];
}