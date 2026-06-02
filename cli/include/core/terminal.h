#pragma once

#define CLI_KEY_NONE 0
#define CLI_KEY_ENTER 1
#define CLI_KEY_BACKSPACE 2
#define CLI_KEY_UP 3
#define CLI_KEY_DOWN 4
#define CLI_KEY_LEFT 5
#define CLI_KEY_RIGHT 6
#define CLI_KEY_HOME 7
#define CLI_KEY_END 8
#define CLI_KEY_DELETE 9
#define CLI_KEY_CTRL_C 10
#define CLI_KEY_CTRL_D 11
#define CLI_KEY_TAB 12
#define CLI_KEY_ESCAPE 13
#define CLI_KEY_PASTE 14

#define CLI_PASTE_BUFFER_SIZE 4096U

void cli_terminal_enable_raw_mode();
void cli_terminal_disable_raw_mode();
[[nodiscard]] int cli_terminal_read_key();
[[nodiscard]] int cli_terminal_get_columns();
[[nodiscard]] const char* cli_terminal_get_paste_data();