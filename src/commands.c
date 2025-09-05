/*
 * src/commands.c
 *
 * CLI command implementations (add, edit, delete, list, import, export, stats, help, version).
 * - Uses cn_* APIs (notes_io, db, display, utils, search)
 * - Portable getopt_long reset handling for GNU/BSD systems
 * - Memory-safe: bounds-checked copies, checked allocations, careful cleanup
 * - Fast: avoids unnecessary allocations in list/print loop; uses stack buffers where reasonable
 *
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>

#include "cheatnote.h"
#include "commands.h"
#include "notes_io.h"
#include "db.h"
#include "display.h"
#include "utils.h"
#include "search.h"

/* Helper: reset getopt between calls in a portable way */
static void reset_getopt_state(void)
{
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    /* BSD family requires optreset and optind reset */
    extern int optreset;
    optreset = 1;
    optind = 1;
#else
    /* GNU and others: setting optind to 0 allows reusing getopt_long */
    optind = 0;
#endif
}

/* ---------- add ---------- */
int cn_cmd_add(int argc, char *argv[])
{
    reset_getopt_state();

    char *title = NULL;
    char *content = NULL;
    char *tags = NULL;
    int opt;

    struct option longopts[] = {
        {"title", required_argument, NULL, 't'},
        {"content", required_argument, NULL, 'c'},
        {"tags", required_argument, NULL, 'g'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(argc, argv, "t:c:g:h", longopts, NULL)) != -1)
    {
        switch (opt)
        {
        case 't':
            title = optarg;
            break;
        case 'c':
            content = optarg;
            break;
        case 'g':
            tags = optarg;
            break;
        case 'h':
            printf("Usage: cheatnote add [OPTIONS] [TITLE] [CONTENT] [TAGS]\n"
                   "Options:\n"
                   "  -t, --title TITLE      Note title\n"
                   "  -c, --content CONTENT  Note content\n"
                   "  -g, --tags TAGS        Comma-separated tags\n"
                   "  -h, --help             Show this help\n\n"
                   "Positional usage:\n"
                   "  cheatnote add \"My Title\" \"My Content\" \"tag1,tag2\"\n");
            return 0;
        default:
            cn_error_exit("Invalid option for add command");
        }
    }

    /* positional fallback */
    int pos = optind;
    if (!title && pos < argc)
        title = argv[pos++];
    if (!content && pos < argc)
        content = argv[pos++];
    if (!tags && pos < argc)
        tags = argv[pos++];

    if (!title || !content || title[0] == '\0' || content[0] == '\0')
    {
        cn_error_exit("Title and content are required for add command");
    }

    /* length checks */
    if (strlen(title) >= MAX_TITLE_LEN)
        cn_error_exit("Title too long");
    if (strlen(content) >= MAX_CONTENT_LEN)
        cn_error_exit("Content too long");
    if (tags && strlen(tags) >= MAX_TAGS_LEN)
        cn_error_exit("Tags too long");

    unsigned int id = cn_note_add(title, content, tags);
    if (id == 0)
    {
        cn_error_exit("Failed to add note");
    }

    /* Save db and report */
    cn_db_save();
    printf("Note added successfully with ID: %s%u%s\n",
           use_colors ? COLOR_GREEN : "", id, use_colors ? COLOR_RESET : "");
    return 0;
}

/* ---------- edit ---------- */
int cn_cmd_edit(int argc, char *argv[])
{
    reset_getopt_state();

    unsigned int id = 0;
    char *title = NULL;
    char *content = NULL;
    char *tags = NULL;
    int opt;

    struct option longopts[] = {
        {"id", required_argument, NULL, 'i'},
        {"title", required_argument, NULL, 't'},
        {"content", required_argument, NULL, 'c'},
        {"tags", required_argument, NULL, 'g'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(argc, argv, "i:t:c:g:h", longopts, NULL)) != -1)
    {
        switch (opt)
        {
        case 'i':
        {
            char *endptr = NULL;
            long v = strtol(optarg, &endptr, 10);
            if (endptr == NULL || *endptr != '\0' || v <= 0 || v > UINT_MAX)
            {
                cn_error_exit("Invalid note ID");
            }
            id = (unsigned int)v;
            break;
        }
        case 't':
            title = optarg;
            break;
        case 'c':
            content = optarg;
            break;
        case 'g':
            tags = optarg;
            break;
        case 'h':
            printf("Usage: cheatnote edit [OPTIONS] [ID] [TITLE] [CONTENT] [TAGS]\n"
                   "Options:\n"
                   "  -i, --id ID            Note ID\n"
                   "  -t, --title TITLE      New title\n"
                   "  -c, --content CONTENT  New content\n"
                   "  -g, --tags TAGS        New tags\n"
                   "  -h, --help             Show this help\n\n"
                   "Positional usage:\n"
                   "  cheatnote edit 5 \"New Title\" \"New Content\"\n");
            return 0;
        default:
            cn_error_exit("Invalid option for edit command");
        }
    }

    /* positional fallback */
    int pos = optind;
    if (id == 0 && pos < argc)
    {
        char *endptr = NULL;
        long v = strtol(argv[pos++], &endptr, 10);
        if (endptr == NULL || *endptr != '\0' || v <= 0 || v > UINT_MAX)
        {
            cn_error_exit("Invalid note ID");
        }
        id = (unsigned int)v;
    }
    if (!title && pos < argc)
        title = argv[pos++];
    if (!content && pos < argc)
        content = argv[pos++];
    if (!tags && pos < argc)
        tags = argv[pos++];

    if (id == 0)
        cn_error_exit("Note ID is required for edit command");
    if (!title && !content && !tags)
        cn_error_exit("At least one field (title, content, or tags) must be provided for edit");

    /* length checks if provided */
    if (title && strlen(title) >= MAX_TITLE_LEN)
        cn_error_exit("Title too long");
    if (content && strlen(content) >= MAX_CONTENT_LEN)
        cn_error_exit("Content too long");
    if (tags && strlen(tags) >= MAX_TAGS_LEN)
        cn_error_exit("Tags too long");

    if (cn_note_edit(id, title, content, tags))
    {
        cn_db_save();
        cn_success_msg("Note updated successfully");
        return 0;
    }

    cn_error_exit("Note not found");
    return 1; /* unreachable because cn_error_exit exits, but keep for clarity */
}

/* ---------- delete ---------- */
int cn_cmd_delete(int argc, char *argv[])
{
    reset_getopt_state();

    unsigned int id = 0;
    int opt;

    struct option longopts[] = {
        {"id", required_argument, NULL, 'i'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(argc, argv, "i:h", longopts, NULL)) != -1)
    {
        switch (opt)
        {
        case 'i':
        {
            char *endptr = NULL;
            long v = strtol(optarg, &endptr, 10);
            if (endptr == NULL || *endptr != '\0' || v <= 0 || v > UINT_MAX)
            {
                cn_error_exit("Invalid note ID");
            }
            id = (unsigned int)v;
            break;
        }
        case 'h':
            printf("Usage: cheatnote delete [OPTIONS] [ID]\n"
                   "Options:\n"
                   "  -i, --id ID  Note ID to delete\n"
                   "  -h, --help   Show this help\n\n"
                   "Positional usage:\n"
                   "  cheatnote delete 5\n");
            return 0;
        default:
            cn_error_exit("Invalid option for delete command");
        }
    }

    /* positional fallback */
    if (id == 0 && optind < argc)
    {
        char *endptr = NULL;
        long v = strtol(argv[optind], &endptr, 10);
        if (endptr == NULL || *endptr != '\0' || v <= 0 || v > UINT_MAX)
        {
            cn_error_exit("Invalid note ID");
        }
        id = (unsigned int)v;
    }

    if (id == 0)
        cn_error_exit("Note ID is required for delete command");

    if (cn_note_delete(id))
    {
        cn_db_save();
        cn_success_msg("Note deleted successfully");
        return 0;
    }

    cn_error_exit("Note not found");
    return 1;
}

/* ---------- list ---------- */
int cn_cmd_list(int argc, char *argv[])
{
    reset_getopt_state();

    cn_search_opts opts = {0};
    int compact = 0;
    int show_ids = 1;
    int opt;

    struct option longopts[] = {
        {"search", required_argument, NULL, 's'},
        {"tags", required_argument, NULL, 'g'},
        {"regex", no_argument, NULL, 'r'},
        {"case-insensitive", no_argument, NULL, 'i'},
        {"exact", no_argument, NULL, 'e'},
        {"word-boundary", no_argument, NULL, 'w'},
        {"multiline", no_argument, NULL, 'm'},
        {"compact", no_argument, NULL, 'c'},
        {"no-ids", no_argument, NULL, 'n'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(argc, argv, "s:g:riewmcnh", longopts, NULL)) != -1)
    {
        switch (opt)
        {
        case 's':
            opts.pattern = optarg;
            break;
        case 'g':
            opts.tags = optarg;
            break;
        case 'r':
            opts.regex_mode = 1;
            break;
        case 'i':
            opts.case_insensitive = 1;
            break;
        case 'e':
            opts.exact_match = 1;
            break;
        case 'w':
            opts.word_boundary = 1;
            break;
        case 'm':
            opts.multiline_mode = 1;
            break;
        case 'c':
            compact = 1;
            break;
        case 'n':
            show_ids = 0;
            break;
        case 'h':
            printf("Usage: cheatnote list [OPTIONS] [SEARCH_PATTERN]\n"
                   "Options:\n"
                   "  -s, --search PATTERN       Search in title, content, and tags\n"
                   "  -g, --tags TAGS            Filter by tags\n"
                   "  -r, --regex                Use regex for search\n"
                   "  -i, --case-insensitive     Case-insensitive search\n"
                   "  -e, --exact                Exact match search\n"
                   "  -w, --word-boundary        Match whole words only (regex mode)\n"
                   "  -m, --multiline            Multiline regex mode\n"
                   "  -c, --compact              Compact output format\n"
                   "  -n, --no-ids               Hide note IDs\n"
                   "  -h, --help                 Show this help\n\n"
                   "Positional usage:\n"
                   "  cheatnote list \"git status\"\n");
            return 0;
        default:
            cn_error_exit("Invalid option for list command");
        }
    }

    /* positional pattern fallback */
    if (!opts.pattern && optind < argc)
        opts.pattern = argv[optind];

    int found = 0;

    for (size_t i = 0; i < db.count; ++i)
    {
        const cn_note *note = &db.notes[i];
        if (cn_note_match_content(note, &opts) && cn_note_match_tags(note->tags, opts.tags))
        {
            if (compact)
                cn_print_note_compact(note, show_ids);
            else
                cn_print_note_full(note, show_ids);
            ++found;
        }
    }

    if (found == 0)
    {
        cn_info_msg("No notes found matching the criteria");
    }
    else
    {
        printf("%sFound %d note%s%s\n",
               use_colors ? COLOR_GREEN : "", found, found == 1 ? "" : "s",
               use_colors ? COLOR_RESET : "");
    }
    return 0;
}

/* ---------- export ---------- */
int cn_cmd_export(int argc, char *argv[])
{
    reset_getopt_state();

    char *filename = NULL;
    int opt;

    struct option longopts[] = {
        {"output", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(argc, argv, "o:h", longopts, NULL)) != -1)
    {
        switch (opt)
        {
        case 'o':
            filename = optarg;
            break;
        case 'h':
            printf("Usage: cheatnote export [OPTIONS] [FILENAME]\n"
                   "Options:\n"
                   "  -o, --output FILENAME  Output filename\n"
                   "  -h, --help             Show this help\n\n"
                   "Positional usage:\n"
                   "  cheatnote export my_notes.csv\n");
            return 0;
        default:
            cn_error_exit("Invalid option for export command");
        }
    }

    if (!filename && optind < argc)
        filename = argv[optind];
    if (!filename)
    {
        filename = "cheatnotes_export.csv";
        cn_info_msg("No filename specified, using default: cheatnotes_export.csv");
    }

    FILE *f = fopen(filename, "w");
    if (!f)
    {
        cn_error_exit("Failed to open export file for writing");
    }

    if (fprintf(f, "ID,Title,Content,Tags,Created,Modified\n") < 0)
    {
        fclose(f);
        cn_error_exit("Failed to write export header");
    }

    for (size_t i = 0; i < db.count; ++i)
    {
        const cn_note *note = &db.notes[i];

        if (fprintf(f, "%u,\"", note->id) < 0)
        {
            fclose(f);
            cn_error_exit("Write error");
        }

        /* escape and write title */
        for (const char *p = note->title; *p; ++p)
        {
            if (*p == '"')
            {
                if (fputs("\"\"", f) == EOF)
                {
                    fclose(f);
                    cn_error_exit("Write error");
                }
            }
            else if (fputc((unsigned char)*p, f) == EOF)
            {
                fclose(f);
                cn_error_exit("Write error");
            }
        }

        if (fputs("\",\"", f) == EOF)
        {
            fclose(f);
            cn_error_exit("Write error");
        }

        /* escape and write content */
        for (const char *p = note->content; *p; ++p)
        {
            if (*p == '"')
            {
                if (fputs("\"\"", f) == EOF)
                {
                    fclose(f);
                    cn_error_exit("Write error");
                }
            }
            else if (fputc((unsigned char)*p, f) == EOF)
            {
                fclose(f);
                cn_error_exit("Write error");
            }
        }

        if (fputs("\",\"", f) == EOF)
        {
            fclose(f);
            cn_error_exit("Write error");
        }

        /* escape and write tags */
        for (const char *p = note->tags; *p; ++p)
        {
            if (*p == '"')
            {
                if (fputs("\"\"", f) == EOF)
                {
                    fclose(f);
                    cn_error_exit("Write error");
                }
            }
            else if (fputc((unsigned char)*p, f) == EOF)
            {
                fclose(f);
                cn_error_exit("Write error");
            }
        }

        if (fprintf(f, "\",%ld,%ld\n", (long)note->created_at, (long)note->modified_at) < 0)
        {
            fclose(f);
            cn_error_exit("Write error");
        }
    }

    if (fclose(f) != 0)
        cn_error_exit("Failed to close export file");

    printf("Exported %zu notes to %s%s%s in CSV format\n",
           db.count, use_colors ? COLOR_CYAN : "", filename, use_colors ? COLOR_RESET : "");
    return 0;
}

/* ---------- import ---------- */
int cn_cmd_import(int argc, char *argv[])
{
    reset_getopt_state();

    char *filename = NULL;
    int merge_mode = 0;
    int opt;

    struct option longopts[] = {
        {"input", required_argument, NULL, 'i'},
        {"merge", no_argument, NULL, 'm'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(argc, argv, "i:mh", longopts, NULL)) != -1)
    {
        switch (opt)
        {
        case 'i':
            filename = optarg;
            break;
        case 'm':
            merge_mode = 1;
            break;
        case 'h':
            printf("Usage: cheatnote import [OPTIONS] [FILENAME]\n"
                   "Options:\n"
                   "  -i, --input FILENAME  Input filename\n"
                   "  -m, --merge           Merge with existing notes (default: replace)\n"
                   "  -h, --help            Show this help\n\n"
                   "Positional usage:\n"
                   "  cheatnote import my_notes.csv\n");
            return 0;
        default:
            cn_error_exit("Invalid option for import command");
        }
    }

    if (!filename && optind < argc)
        filename = argv[optind];
    if (!filename)
        cn_error_exit("Input filename is required for import command");

    FILE *f = fopen(filename, "r");
    if (!f)
        cn_error_exit("Failed to open import file for reading");

    if (!merge_mode)
    {
        cn_db_cleanup();
        cn_db_init();
    }

    size_t imported = 0, line_num = 0, errors = 0;
    char *linebuf = malloc(MAX_LINE_LENGTH);
    if (!linebuf)
    {
        fclose(f);
        cn_error_exit("Failed to allocate memory for line buffer");
    }

    while (fgets(linebuf, MAX_LINE_LENGTH, f))
    {
        ++line_num;

        size_t len = strlen(linebuf);
        if (len == MAX_LINE_LENGTH - 1 && linebuf[len - 1] != '\n' && !feof(f))
        {
            /* skip remainder of overlong line */
            int ch;
            while ((ch = fgetc(f)) != '\n' && ch != EOF)
            {
            }
            fprintf(stderr, "%sWarning:%s Skipping overlong line %zu\n", use_colors ? COLOR_YELLOW : "", use_colors ? COLOR_RESET : "", line_num);
            ++errors;
            continue;
        }

        /* skip header */
        if (line_num == 1 && (strstr(linebuf, "ID,Title,Content") || strstr(linebuf, "id,title,content")))
            continue;

        if (len > 0 && linebuf[len - 1] == '\n')
            linebuf[len - 1] = '\0';
        if (linebuf[0] == '\0')
            continue;

        char *pos = linebuf;
        char id_str[32] = {0};
        char title[MAX_TITLE_LEN] = {0};
        char content[MAX_CONTENT_LEN] = {0};
        char tags[MAX_TAGS_LEN] = {0};

        if (!cn_parse_csv_field(&pos, id_str, sizeof(id_str)) ||
            !cn_parse_csv_field(&pos, title, sizeof(title)) ||
            !cn_parse_csv_field(&pos, content, sizeof(content)))
        {
            fprintf(stderr, "%sWarning:%s Skipping malformed line %zu\n", use_colors ? COLOR_YELLOW : "", use_colors ? COLOR_RESET : "", line_num);
            ++errors;
            continue;
        }
        /* optional tags */
        cn_parse_csv_field(&pos, tags, sizeof(tags));

        if (title[0] == '\0' || content[0] == '\0')
        {
            fprintf(stderr, "%sWarning:%s Skipping line %zu - missing title or content\n", use_colors ? COLOR_YELLOW : "", use_colors ? COLOR_RESET : "", line_num);
            ++errors;
            continue;
        }

        unsigned int nid = cn_note_add(title, content, tags[0] ? tags : NULL);
        if (nid == 0)
        {
            fprintf(stderr, "%sWarning:%s Failed to import line %zu\n", use_colors ? COLOR_YELLOW : "", use_colors ? COLOR_RESET : "", line_num);
            ++errors;
            continue;
        }
        ++imported;
    }

    free(linebuf);
    if (fclose(f) != 0)
        fprintf(stderr, "Warning: Error closing import file\n");

    cn_db_save();
    printf("Successfully imported %s%zu%s notes from %s%s%s",
           use_colors ? COLOR_GREEN : "", imported, use_colors ? COLOR_RESET : "",
           use_colors ? COLOR_CYAN : "", filename, use_colors ? COLOR_RESET : "");
    if (errors > 0)
    {
        printf(" (%s%zu%s errors)", use_colors ? COLOR_YELLOW : "", errors, use_colors ? COLOR_RESET : "");
    }
    printf("\n");
    return 0;
}

/* ---------- stats ---------- */
int cn_cmd_stats(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    if (db.count == 0)
    {
        cn_info_msg("No notes in database");
        return 0;
    }

    size_t total_chars = 0;
    size_t total_lines = 0;
    time_t oldest = db.notes[0].created_at;
    time_t newest = db.notes[0].created_at;

    for (size_t i = 0; i < db.count; ++i)
    {
        const cn_note *note = &db.notes[i];
        total_chars += strlen(note->content);

        /* count lines */
        for (const char *p = note->content; *p; ++p)
            if (*p == '\n')
                ++total_lines;
        if (note->content[0])
            ++total_lines; /* last line */

        if (note->created_at < oldest)
            oldest = note->created_at;
        if (note->created_at > newest)
            newest = note->created_at;
    }

    char oldest_str[32] = "Invalid date", newest_str[32] = "Invalid date";
    struct tm tm_info;

    if (localtime_r(&oldest, &tm_info))
    {
        strftime(oldest_str, sizeof(oldest_str), "%Y-%m-%d %H:%M", &tm_info);
    }
    if (localtime_r(&newest, &tm_info))
    {
        strftime(newest_str, sizeof(newest_str), "%Y-%m-%d %H:%M", &tm_info);
    }

    printf("%sCheatNote Statistics%s\n", use_colors ? COLOR_BOLD COLOR_CYAN : "", use_colors ? COLOR_RESET : "");
    printf("%s━━━━━━━━━━━━━━━━━━━━━━━━%s\n", use_colors ? COLOR_BLUE : "", use_colors ? COLOR_RESET : "");
    printf("Total Notes:     %s%zu%s\n", use_colors ? COLOR_GREEN : "", db.count, use_colors ? COLOR_RESET : "");
    printf("Total Characters: %s%zu%s\n", use_colors ? COLOR_YELLOW : "", total_chars, use_colors ? COLOR_RESET : "");
    printf("Total Lines:     %s%zu%s\n", use_colors ? COLOR_YELLOW : "", total_lines, use_colors ? COLOR_RESET : "");
    printf("Avg Chars/Note:  %s%.1f%s\n", use_colors ? COLOR_MAGENTA : "",
           db.count > 0 ? (double)total_chars / db.count : 0.0, use_colors ? COLOR_RESET : "");
    printf("Oldest Note:     %s%s%s\n", use_colors ? COLOR_DIM : "", oldest_str, use_colors ? COLOR_RESET : "");
    printf("Newest Note:     %s%s%s\n", use_colors ? COLOR_DIM : "", newest_str, use_colors ? COLOR_RESET : "");
    printf("Database Size:   %s%.2f KB%s\n", use_colors ? COLOR_CYAN : "",
           (double)(sizeof(cn_note) * db.count) / 1024.0, use_colors ? COLOR_RESET : "");

    return 0;
}

/* ---------- help & version & dispatch ---------- */
int cn_cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("%s%sCheatNote v%s - Blazing fast Snippet Manager%s\n",
           use_colors ? COLOR_BOLD : "", use_colors ? COLOR_CYAN : "",
           VERSION, use_colors ? COLOR_RESET : "");
    printf("\nUsage: cheatnote COMMAND [OPTIONS] [ARGS...]\n\n");
    printf("Commands:\n");
    printf("  add      Add a new note\n");
    printf("  edit     Edit an existing note\n");
    printf("  delete   Delete a note\n");
    printf("  list     List and search notes\n");
    printf("  export   Export notes to file\n");
    printf("  import   Import notes from file\n");
    printf("  stats    Show database statistics\n");
    printf("  help     Show this help message\n");
    printf("  version  Show version information\n\n");
    printf("Global Options:\n");
    printf("  --no-color   Disable colored output\n");
    printf("  CHEATNOTE_DB   Override database path\n\n");
    printf("Examples:\n");
    printf("  cheatnote add \"Git status\" \"git status -s\" \"git,status\"\n");
    printf("  cheatnote list \"git\" -r -i\n");
    return 0;
}

int cn_cmd_version(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("CheatNote v%s\n", VERSION);
    printf("Blazing fast snippet and note manager\n");
    printf("Built with: Enhanced regex, colored output, import/export\n");
    printf("Database: Binary format optimized for speed\n");
    return 0;
}

int cn_commands_dispatch(int argc, char *argv[])
{
    if (argc < 2)
    {
        cn_cmd_help(0, NULL);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0)
    {
        return cn_cmd_help(argc - 1, argv + 1);
    }
    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "-v") == 0 || strcmp(cmd, "--version") == 0)
    {
        return cn_cmd_version(argc - 1, argv + 1);
    }

    /* For other commands we expect DB already loaded by main() */
    if (strcmp(cmd, "add") == 0)
        return cn_cmd_add(argc - 1, argv + 1);
    if (strcmp(cmd, "edit") == 0)
        return cn_cmd_edit(argc - 1, argv + 1);
    if (strcmp(cmd, "delete") == 0)
        return cn_cmd_delete(argc - 1, argv + 1);
    if (strcmp(cmd, "list") == 0)
        return cn_cmd_list(argc - 1, argv + 1);
    if (strcmp(cmd, "export") == 0)
        return cn_cmd_export(argc - 1, argv + 1);
    if (strcmp(cmd, "import") == 0)
        return cn_cmd_import(argc - 1, argv + 1);
    if (strcmp(cmd, "stats") == 0)
        return cn_cmd_stats(argc - 1, argv + 1);

    fprintf(stderr, "Unknown command: %s\n", cmd);
    fprintf(stderr, "Use 'cheatnote help' for usage information\n");
    return 2;
}
