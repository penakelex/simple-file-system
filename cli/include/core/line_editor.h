#pragma once

#include "core/history.h"
#include <stddef.h>

#define CLI_LINE_EDITOR_MAX_INPUT 2048U

[[nodiscard]] bool
cli_line_editor_read(const char* prompt,
                     char* output_buffer,
                     const size_t buffer_size,
                     cli_history_t* history);

void cli_line_editor_reset_render_state();