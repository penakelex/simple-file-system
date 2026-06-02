#include "core/terminal.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#endif

static char cli_paste_buffer[CLI_PASTE_BUFFER_SIZE];

#ifdef _WIN32

void cli_terminal_enable_raw_mode() {
  const HANDLE output_handle =
    GetStdHandle(STD_OUTPUT_HANDLE);

  if (output_handle == INVALID_HANDLE_VALUE) {
    return;
  }

  DWORD console_mode = 0;

  if (GetConsoleMode(output_handle, &console_mode) != 0) {
    console_mode |= 0x0004U;
    (void)SetConsoleMode(output_handle, console_mode);
  }

  (void)fprintf(stdout, "\x1b[?2004h");
  (void)fflush(stdout);
}

void cli_terminal_disable_raw_mode() {
  (void)fprintf(stdout, "\x1b[?2004l");
  (void)fflush(stdout);
}

static int cli_terminal_read_raw_byte() {
  return _getch();
}

int cli_terminal_get_columns() {
  CONSOLE_SCREEN_BUFFER_INFO console_info;

  const HANDLE output_handle =
    GetStdHandle(STD_OUTPUT_HANDLE);

  if (GetConsoleScreenBufferInfo(output_handle,
                                 &console_info)
      != 0) {
    return console_info.srWindow.Right
           - console_info.srWindow.Left + 1;
  }

  return 80;
}

#else

static struct termios cli_original_termios;
static bool cli_termios_saved = false;

void cli_terminal_enable_raw_mode() {
  if (!cli_termios_saved) {
    (void)tcgetattr(STDIN_FILENO, &cli_original_termios);
    cli_termios_saved = true;
  }

  struct termios raw_mode = cli_original_termios;
  raw_mode.c_lflag &= ~(ECHO | ICANON | ISIG);
  raw_mode.c_cc[VMIN] = 1;
  raw_mode.c_cc[VTIME] = 0;
  (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode);
  (void)fprintf(stdout, "\x1b[?2004h");
  (void)fflush(stdout);
}

void cli_terminal_disable_raw_mode() {
  (void)fprintf(stdout, "\x1b[?2004l");
  (void)fflush(stdout);

  if (cli_termios_saved) {
    (void)tcsetattr(
      STDIN_FILENO, TCSAFLUSH, &cli_original_termios);
  }
}

static int cli_terminal_read_raw_byte() {
  unsigned char byte = 0;
  const ssize_t read_result = read(STDIN_FILENO, &byte, 1);

  if (read_result <= 0) {
    return -1;
  }

  return (int)byte;
}

int cli_terminal_get_columns() {
  struct winsize terminal_size;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal_size) == 0
      && terminal_size.ws_col > 0) {
    return terminal_size.ws_col;
  }

  return 80;
}

#endif

static void cli_terminal_read_paste_data() {
  size_t paste_length = 0;
  cli_paste_buffer[0] = '\0';

  while (paste_length < CLI_PASTE_BUFFER_SIZE - 1U) {
    const int byte = cli_terminal_read_raw_byte();

    if (byte < 0) {
      break;
    }

    if (byte == 0x1B) {
      const int bracket = cli_terminal_read_raw_byte();

      if (bracket != '[') {
        if (paste_length < CLI_PASTE_BUFFER_SIZE - 2U) {
          cli_paste_buffer[paste_length++] = (char)byte;
          cli_paste_buffer[paste_length++] = (char)bracket;
        }

        continue;
      }

      const int two = cli_terminal_read_raw_byte();
      const int zero_or_one = cli_terminal_read_raw_byte();
      const int tilde = cli_terminal_read_raw_byte();

      if (two == '2' && zero_or_one == '1'
          && tilde == '~') {
        break;
      }

      if (paste_length + 5U < CLI_PASTE_BUFFER_SIZE) {
        cli_paste_buffer[paste_length++] = (char)byte;
        cli_paste_buffer[paste_length++] = (char)bracket;
        cli_paste_buffer[paste_length++] = (char)two;
        cli_paste_buffer[paste_length++] =
          (char)zero_or_one;
        cli_paste_buffer[paste_length++] = (char)tilde;
      }

      continue;
    }

    if (byte >= 32 && byte < 127) {
      cli_paste_buffer[paste_length++] = (char)byte;
    } else if (byte == '\r' || byte == '\n') {
      cli_paste_buffer[paste_length++] = '\n';
    }
  }

  cli_paste_buffer[paste_length] = '\0';
}

int cli_terminal_read_key() {
  const int first_byte = cli_terminal_read_raw_byte();

  if (first_byte < 0) {
    return CLI_KEY_CTRL_D;
  }

  if (first_byte == 0x0D || first_byte == 0x0A) {
    return CLI_KEY_ENTER;
  }
  if (first_byte == 0x7F || first_byte == 0x08) {
    return CLI_KEY_BACKSPACE;
  }
  if (first_byte == 0x03) {
    return CLI_KEY_CTRL_C;
  }
  if (first_byte == 0x04) {
    return CLI_KEY_CTRL_D;
  }
  if (first_byte == 0x09) {
    return CLI_KEY_TAB;
  }

  if (first_byte == 0x1B) {
    const int second_byte = cli_terminal_read_raw_byte();

    if (second_byte < 0) {
      return CLI_KEY_ESCAPE;
    }

    if (second_byte == '[') {
      const int third_byte = cli_terminal_read_raw_byte();

      if (third_byte < 0) {
        return CLI_KEY_ESCAPE;
      }

      if (third_byte == '2') {
        const int fourth_byte =
          cli_terminal_read_raw_byte();
        const int fifth_byte = cli_terminal_read_raw_byte();
        const int sixth_byte = cli_terminal_read_raw_byte();

        if (fourth_byte == '0' && fifth_byte == '0'
            && sixth_byte == '~') {
          cli_terminal_read_paste_data();
          return CLI_KEY_PASTE;
        }

        if (sixth_byte == '~') {
          switch (fourth_byte) {
          case '0':
            return CLI_KEY_NONE;
          default:
            return CLI_KEY_NONE;
          }
        }

        return CLI_KEY_NONE;
      }

      if (third_byte >= '0' && third_byte <= '9') {
        const int fourth_byte =
          cli_terminal_read_raw_byte();

        if (fourth_byte < 0) {
          return CLI_KEY_ESCAPE;
        }
        if (fourth_byte == '~') {
          switch (third_byte) {
          case '1':
            return CLI_KEY_HOME;
          case '3':
            return CLI_KEY_DELETE;
          case '4':
            return CLI_KEY_END;
          case '7':
            return CLI_KEY_HOME;
          case '8':
            return CLI_KEY_END;
          default:
            break;
          }
        }

        return CLI_KEY_NONE;
      }

      switch (third_byte) {
      case 'A':
        return CLI_KEY_UP;
      case 'B':
        return CLI_KEY_DOWN;
      case 'C':
        return CLI_KEY_RIGHT;
      case 'D':
        return CLI_KEY_LEFT;
      case 'H':
        return CLI_KEY_HOME;
      case 'F':
        return CLI_KEY_END;
      default:
        break;
      }

      return CLI_KEY_ESCAPE;
    }

    if (second_byte == 'O') {
      const int third_byte = cli_terminal_read_raw_byte();

      if (third_byte < 0) {
        return CLI_KEY_ESCAPE;
      }

      switch (third_byte) {
      case 'H':
        return CLI_KEY_HOME;
      case 'F':
        return CLI_KEY_END;
      default:
        break;
      }

      return CLI_KEY_ESCAPE;
    }

    return CLI_KEY_ESCAPE;
  }

#ifdef _WIN32
  if (first_byte == 0x00 || first_byte == 0xE0) {
    const int second_byte = cli_terminal_read_raw_byte();

    switch (second_byte) {
    case 0x48:
      return CLI_KEY_UP;
    case 0x50:
      return CLI_KEY_DOWN;
    case 0x4B:
      return CLI_KEY_LEFT;
    case 0x4D:
      return CLI_KEY_RIGHT;
    case 0x47:
      return CLI_KEY_HOME;
    case 0x4F:
      return CLI_KEY_END;
    case 0x53:
      return CLI_KEY_DELETE;
    default:
      return CLI_KEY_NONE;
    }
  }
#endif

  return first_byte;
}

const char* cli_terminal_get_paste_data() {
  return cli_paste_buffer;
}