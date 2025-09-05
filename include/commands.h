#ifndef CN_COMMANDS_H
#define CN_COMMANDS_H

/*
 * commands.h
 * CLI command handlers for CheatNote.
 */

/* Command handlers */
int cn_cmd_add(int argc, char *argv[]);
int cn_cmd_edit(int argc, char *argv[]);
int cn_cmd_delete(int argc, char *argv[]);
int cn_cmd_list(int argc, char *argv[]);
int cn_cmd_export(int argc, char *argv[]);
int cn_cmd_import(int argc, char *argv[]);
int cn_cmd_stats(int argc, char *argv[]);
int cn_cmd_help(int argc, char *argv[]);
int cn_cmd_version(int argc, char *argv[]);

/* Dispatcher */
int cn_commands_dispatch(int argc, char *argv[]);

#endif /* CN_COMMANDS_H */
