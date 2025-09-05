/*
 * src/main.c
 *
 * Entry point for CheatNote CLI.
 * - Defines global storage for the DB and runtime options (single-definition).
 * - Handles global flags (currently: --no-color).
 * - Initializes DB, registers cleanup, and dispatches commands.
 *
 * Globals defined here (declared extern in include/cheatnote.h):
 *   cn_note_db db;
 *   int use_colors;
 *   char db_path[PATH_MAX];
 *
 * Keep this file tiny: parsing of subcommand-specific options is delegated
 * to the command implementations.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cheatnote.h"
#include "commands.h"
#include "db.h"
#include "display.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* === Global definitions (one true place) === */
cn_note_db db = {0};
int use_colors = 1;
char db_path[PATH_MAX] = "";

/* Remove a single argv entry at index idx, shifting the rest left and
 * decrementing *argc. This preserves argv[0] and the program name.
 */
static void argv_remove(char **argv, int *argc, int idx)
{
    for (int j = idx; j < (*argc) - 1; ++j)
    {
        argv[j] = argv[j + 1];
    }
    argv[(*argc) - 1] = NULL;
    (*argc)--;
}

/* Process global flags that should not be visible to subcommands.
 * Currently supports:
 *   --no-color    : disable colored output
 *
 * This intentionally strips recognized global flags from argv so that
 * subcommand parsers see only the subcommand and its args.
 */
static void process_global_flags(int *argc, char *argv[])
{
    for (int i = 1; i < *argc; ++i)
    {
        if (strcmp(argv[i], "--no-color") == 0)
        {
            /* disable colors and remove the flag from argv */
            cn_set_use_colors(0);
            argv_remove(argv, argc, i);
            i--; /* re-check current index after shift */
        }
    }

    /* If colors are still enabled, only keep them if stdout is a terminal */
    if (use_colors && !cn_is_terminal_output())
    {
        cn_set_use_colors(0);
    }
}

int main(int argc, char *argv[])
{
    /* Keep main minimal: handle only global flags here */
    process_global_flags(&argc, argv);

    /* If user asked for help/version without subcommand (e.g., `cheatnote help`),
     * the dispatcher will handle it. Load DB for commands that need it.
     *
     * We load DB here so commands can assume it's available.
     */
    cn_db_load();

    /* Ensure DB memory is freed on normal exit */
    if (atexit(cn_db_cleanup) != 0)
    {
        /* If registration fails, continue but warn */
        fprintf(stderr, "Warning: failed to register cleanup handler\n");
    }

    /* Dispatch commands (returns exit code) */
    int rc = cn_commands_dispatch(argc, argv);

    /* Return the dispatch result as program exit code */
    return rc;
}
