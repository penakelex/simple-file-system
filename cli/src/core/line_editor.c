#include "core/line_editor.h"
#include "core/terminal.h"
#include <stdio.h>
#include <string.h>

typedef struct line_editor_render_state {
  size_t previous_cursor_row;
  size_t previous_end_row;
} line_editor_render_state_t;

static line_editor_render_state_t render_state = {0};

void cli_line_editor_reset_render_state() {
  render_state.previous_cursor_row = 0U;
  render_state.previous_end_row = 0U;
}

static void
cli_line_editor_clear_previous(const size_t prev_cursor_row,
                               const size_t prev_end_row) {
  if (prev_cursor_row > 0U) {
    (void)fprintf(stdout, "\x1b[%zuA", prev_cursor_row);
  }

  (void)fprintf(stdout, "\x1b[1G");

  for (size_t row = 0U; row <= prev_end_row; ++row) {
    (void)fprintf(stdout, "\x1b[2K");
    if (row < prev_end_row) {
      (void)fprintf(stdout, "\x1b[1B");
    }
  }

  if (prev_end_row > 0U) {
    (void)fprintf(stdout, "\x1b[%zuA", prev_end_row);
  }

  (void)fprintf(stdout, "\x1b[1G");
}

static void
cli_line_editor_refresh(const char* prompt,
                        const char* line_buffer,
                        const size_t cursor_position,
                        const size_t line_length) {
  const int terminal_columns = cli_terminal_get_columns();

  if (terminal_columns <= 0) {
    return;
  }

  const size_t cols = (size_t)terminal_columns;
  const size_t prompt_length = strlen(prompt);
  const size_t current_total = prompt_length + line_length;

  cli_line_editor_clear_previous(
    render_state.previous_cursor_row,
    render_state.previous_end_row);

  (void)fprintf(stdout, "%s%s", prompt, line_buffer);

  const size_t cursor_absolute =
    prompt_length + cursor_position;

  size_t target_row = 0U;
  size_t target_column = 1U;

  if (cursor_absolute > 0U) {
    target_row = (cursor_absolute - 1U) / cols;

    if (cursor_absolute % cols == 0U) {
      target_column = cols;
    } else {
      target_column = (cursor_absolute % cols) + 1U;
    }
  }

  const size_t end_row = (current_total > 0U)
                           ? ((current_total - 1U) / cols)
                           : 0U;

  if (target_row > end_row) {
    (void)fprintf(
      stdout, "\x1b[%zuB", target_row - end_row);
  } else if (end_row > target_row) {
    (void)fprintf(
      stdout, "\x1b[%zuA", end_row - target_row);
  }

  (void)fprintf(stdout, "\x1b[%zuG", target_column);

  (void)fflush(stdout);

  render_state.previous_cursor_row = target_row;
  render_state.previous_end_row = end_row;
}

bool cli_line_editor_read(const char* prompt,
                          char* output_buffer,
                          const size_t buffer_size,
                          cli_history_t* history) {
  if (prompt == nullptr || output_buffer == nullptr
      || buffer_size == 0U) {
    return false;
  }

  output_buffer[0] = '\0';
  size_t cursor_position = 0U;
  size_t line_length = 0U;
  char saved_line[CLI_LINE_EDITOR_MAX_INPUT] = {0};
  bool is_navigating_history = false;

  cli_line_editor_refresh(
    prompt, output_buffer, cursor_position, line_length);
  while (true) {
    const int key = cli_terminal_read_key();

    switch (key) {
    case CLI_KEY_ENTER:
      output_buffer[line_length] = '\0';
      cli_line_editor_clear_previous(
        render_state.previous_cursor_row,
        render_state.previous_end_row);
      (void)fprintf(
        stdout, "%s%s\n", prompt, output_buffer);
      (void)fflush(stdout);
      render_state.previous_cursor_row = 0U;
      render_state.previous_end_row = 0U;
      cli_history_reset_browse(history);
      return true;
    case CLI_KEY_CTRL_C:
      cli_line_editor_clear_previous(
        render_state.previous_cursor_row,
        render_state.previous_end_row);
      (void)fprintf(stdout, "^C\n");
      (void)fflush(stdout);
      output_buffer[0] = '\0';
      render_state.previous_cursor_row = 0U;
      render_state.previous_end_row = 0U;
      cli_history_reset_browse(history);
      return false;
    case CLI_KEY_CTRL_D:
      if (line_length == 0U) {
        cli_line_editor_clear_previous(
          render_state.previous_cursor_row,
          render_state.previous_end_row);
        (void)fprintf(stdout, "exit\n");
        (void)fflush(stdout);
        (void)strcpy(output_buffer, "exit");
        render_state.previous_cursor_row = 0U;
        render_state.previous_end_row = 0U;
        return true;
      }

      if (cursor_position < line_length) {
        (void)memmove(output_buffer + cursor_position,
                      output_buffer + cursor_position + 1U,
                      line_length - cursor_position);
        line_length--;
        cli_line_editor_refresh(prompt,
                                output_buffer,
                                cursor_position,
                                line_length);
      }

      break;
    case CLI_KEY_BACKSPACE:
      if (cursor_position > 0U) {
        (void)memmove(output_buffer + cursor_position - 1U,
                      output_buffer + cursor_position,
                      line_length - cursor_position + 1U);
        cursor_position--;
        line_length--;
        cli_line_editor_refresh(prompt,
                                output_buffer,
                                cursor_position,
                                line_length);
      }

      break;
    case CLI_KEY_DELETE:
      if (cursor_position < line_length) {
        (void)memmove(output_buffer + cursor_position,
                      output_buffer + cursor_position + 1U,
                      line_length - cursor_position);
        line_length--;
        cli_line_editor_refresh(prompt,
                                output_buffer,
                                cursor_position,
                                line_length);
      }

      break;
    case CLI_KEY_LEFT:
      if (cursor_position > 0U) {
        cursor_position--;
        cli_line_editor_refresh(prompt,
                                output_buffer,
                                cursor_position,
                                line_length);
      }

      break;
    case CLI_KEY_RIGHT:
      if (cursor_position < line_length) {
        cursor_position++;
        cli_line_editor_refresh(prompt,
                                output_buffer,
                                cursor_position,
                                line_length);
      }

      break;
    case CLI_KEY_HOME:
      if (cursor_position != 0U) {
        cursor_position = 0U;
        cli_line_editor_refresh(prompt,
                                output_buffer,
                                cursor_position,
                                line_length);
      }

      break;
    case CLI_KEY_END:
      if (cursor_position != line_length) {
        cursor_position = line_length;
        cli_line_editor_refresh(prompt,
                                output_buffer,
                                cursor_position,
                                line_length);
      }

      break;
    case CLI_KEY_PASTE: {
      const char* paste_data =
        cli_terminal_get_paste_data();

      if (paste_data != nullptr && paste_data[0] != '\0') {
        const size_t insert_length = strlen(paste_data);
        const size_t available_space =
          buffer_size - 1U - line_length;
        const size_t bytes_to_insert =
          (insert_length < available_space)
            ? insert_length
            : available_space;

        if (bytes_to_insert > 0U) {
          (void)memmove(output_buffer + cursor_position
                          + bytes_to_insert,
                        output_buffer + cursor_position,
                        line_length - cursor_position + 1U);
          (void)memcpy(output_buffer + cursor_position,
                       paste_data,
                       bytes_to_insert);
          cursor_position += bytes_to_insert;
          line_length += bytes_to_insert;
          output_buffer[line_length] = '\0';
          cli_line_editor_refresh(prompt,
                                  output_buffer,
                                  cursor_position,
                                  line_length);
        }
      }

      break;
    }
    case CLI_KEY_UP: {
      if (!is_navigating_history) {
        (void)strncpy(saved_line,
                      output_buffer,
                      sizeof(saved_line) - 1U);
        saved_line[sizeof(saved_line) - 1U] = '\0';
        is_navigating_history = true;
      }

      const char* history_entry =
        cli_history_browse_up(history);

      if (history_entry != nullptr) {
        (void)strncpy(
          output_buffer, history_entry, buffer_size - 1U);
        output_buffer[buffer_size - 1U] = '\0';
        line_length = strlen(output_buffer);
        cursor_position = line_length;
        cli_line_editor_refresh(prompt,
                                output_buffer,
                                cursor_position,
                                line_length);
      }

      break;
    }
    case CLI_KEY_DOWN: {
      if (is_navigating_history) {
        const char* history_entry =
          cli_history_browse_down(history);

        if (history_entry != nullptr
            && history_entry[0] != '\0') {
          (void)strncpy(
            output_buffer, history_entry, buffer_size - 1U);
          output_buffer[buffer_size - 1U] = '\0';
        } else {
          (void)strncpy(
            output_buffer, saved_line, buffer_size - 1U);
          output_buffer[buffer_size - 1U] = '\0';
          is_navigating_history = false;
        }

        line_length = strlen(output_buffer);
        cursor_position = line_length;
        cli_line_editor_refresh(prompt,
                                output_buffer,
                                cursor_position,
                                line_length);
      }

      break;
    }
    case CLI_KEY_ESCAPE:
    case CLI_KEY_TAB:
    case CLI_KEY_NONE:
      break;
    default:
      if (key >= 32 && key < 127
          && line_length < buffer_size - 1U) {
        (void)memmove(output_buffer + cursor_position + 1U,
                      output_buffer + cursor_position,
                      line_length - cursor_position + 1U);
        output_buffer[cursor_position] = (char)key;
        cursor_position++;
        line_length++;
        cli_line_editor_refresh(prompt,
                                output_buffer,
                                cursor_position,
                                line_length);
      }

      break;
    }
  }
}