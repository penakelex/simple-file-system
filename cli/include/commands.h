#pragma once

#include "core/shell.h"
#include <stdint.h>

#define CLI_LEVEL_1 (1U << 0)
#define CLI_LEVEL_2 (1U << 1)
#define CLI_LEVEL_BOTH (CLI_LEVEL_1 | CLI_LEVEL_2)

typedef void (*cli_command_handler_t)(
  cli_main_state_t* state, const int argc, char** argv);

typedef struct cli_command_entry {
  const char* name;
  const char* const* aliases;
  const char* description;
  cli_command_handler_t handler;
  uint32_t level_mask;
} cli_command_entry_t;

extern const cli_command_entry_t cli_command_table[];
extern const size_t cli_command_table_size;

void cli_command_help(cli_main_state_t* state,
                      const int argc,
                      char** argv);
void cli_command_exit(cli_main_state_t* state,
                      const int argc,
                      char** argv);
void cli_command_history(cli_main_state_t* state,
                         const int argc,
                         char** argv);

void cli_command_create(cli_main_state_t* state,
                        const int argc,
                        char** argv);
void cli_command_format(cli_main_state_t* state,
                        const int argc,
                        char** argv);
void cli_command_open(cli_main_state_t* state,
                      const int argc,
                      char** argv);
void cli_command_close(cli_main_state_t* state,
                       const int argc,
                       char** argv);
void cli_command_list(cli_main_state_t* state,
                      const int argc,
                      char** argv);
void cli_command_manager_cd(cli_main_state_t* state,
                            const int argc,
                            char** argv);
void cli_command_host_pwd(cli_main_state_t* state,
                          const int argc,
                          char** argv);
void cli_command_delete(cli_main_state_t* state,
                        const int argc,
                        char** argv);
void cli_command_export_disk(cli_main_state_t* state,
                             const int argc,
                             char** argv);
void cli_command_import_disk(cli_main_state_t* state,
                             const int argc,
                             char** argv);
void cli_command_disk_info(cli_main_state_t* state,
                           const int argc,
                           char** argv);

void cli_command_fs_cd(cli_main_state_t* state,
                       const int argc,
                       char** argv);
void cli_command_ls(cli_main_state_t* state,
                    const int argc,
                    char** argv);
void cli_command_pwd(cli_main_state_t* state,
                     const int argc,
                     char** argv);
void cli_command_tree(cli_main_state_t* state,
                      const int argc,
                      char** argv);

void cli_command_mkdir(cli_main_state_t* state,
                       const int argc,
                       char** argv);
void cli_command_rmdir(cli_main_state_t* state,
                       const int argc,
                       char** argv);

void cli_command_touch(cli_main_state_t* state,
                       const int argc,
                       char** argv);
void cli_command_rm(cli_main_state_t* state,
                    const int argc,
                    char** argv);
void cli_command_cat(cli_main_state_t* state,
                     const int argc,
                     char** argv);
void cli_command_info(cli_main_state_t* state,
                      const int argc,
                      char** argv);
void cli_command_cp(cli_main_state_t* state,
                    const int argc,
                    char** argv);
void cli_command_mv(cli_main_state_t* state,
                    const int argc,
                    char** argv);
void cli_command_import(cli_main_state_t* state,
                        const int argc,
                        char** argv);
void cli_command_export(cli_main_state_t* state,
                        const int argc,
                        char** argv);
void cli_command_open_file(cli_main_state_t* state,
                           const int argc,
                           char** argv);

void cli_command_defrag(cli_main_state_t* state,
                        const int argc,
                        char** argv);
void cli_command_bitmap_dump(cli_main_state_t* state,
                             const int argc,
                             char** argv);
void cli_command_compress(cli_main_state_t* state,
                          const int argc,
                          char** argv);
void cli_command_decompress(cli_main_state_t* state,
                            const int argc,
                            char** argv);
void cli_command_df(cli_main_state_t* state,
                    const int argc,
                    char** argv);
void cli_command_find(cli_main_state_t* state,
                      const int argc,
                      char** argv);
void cli_command_echo(cli_main_state_t* state,
                      const int argc,
                      char** argv);
void cli_command_ln(cli_main_state_t* state,
                    const int argc,
                    char** argv);