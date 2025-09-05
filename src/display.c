/*
 * src/display.c
 *
 * Rendering and user-facing messages for CheatNote.
 *
 * - Memory safe: no heap allocations while printing notes
 * - Portable: tries to enable ANSI on Windows; uses localtime_r when available
 * - Fast: prints by slicing existing strings (no copies)
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "cheatnote.h"
#include "display.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif

/* Extern globals (defined once in main.c) */
// db is declared as extern cn_note_db db; in cheatnote.h
extern int use_colors;
extern char db_path[PATH_MAX];

/* Internal: try to enable ANSI processing on Windows consoles when colors requested.
 */
#ifdef _WIN32
static void try_enable_ansi_on_windows(void)
{
#if defined(_WIN32) || defined(_WIN64)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        use_colors = 0;
        return;
    }
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode))
    {
        /* Not a console or can't query mode */
        use_colors = 0;
        return;
    }
    /* ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004 */
    const DWORD vt_flag = 0x0004;
    if ((mode & vt_flag) == 0)
    {
        if (!SetConsoleMode(hOut, mode | vt_flag))
        {
            /* Failed — disable colors */
            use_colors = 0;
        }
    }
#endif
}
#endif

/* Public: enable/disable colors (also tries to enable ANSI on Windows) */
void cn_set_use_colors(int enabled)
{
    use_colors = enabled ? 1 : 0;
    if (use_colors)
    {
        /* Only keep colors if stdout is a terminal */
        if (!cn_is_terminal_output())
        {
            use_colors = 0;
            return;
        }
#if defined(_WIN32) || defined(_WIN64)
        try_enable_ansi_on_windows();
#endif
    }
}

/* Safety: prints error then exits. Colors optional.
 * This function intentionally does not return.
 */
void cn_error_exit(const char *msg)
{
    if (!msg)
        msg = "Unknown error";
    if (use_colors)
        fprintf(stderr, "%sError:%s %s\n", COLOR_RED, COLOR_RESET, msg);
    else
        fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}

void cn_success_msg(const char *msg)
{
    if (!msg)
        return;
    if (use_colors)
        printf("%s✓%s %s\n", COLOR_GREEN, COLOR_RESET, msg);
    else
        printf("✓ %s\n", msg);
}

void cn_info_msg(const char *msg)
{
    if (!msg)
        return;
    if (use_colors)
        printf("%sInfo:%s %s\n", COLOR_BLUE, COLOR_RESET, msg);
    else
        printf("Info: %s\n", msg);
}

/* Internal helpers for rendering without allocations */

/* Print header line with box-drawing glyphs, title and optional tags/ID. */
void cn_print_note_header(const cn_note *note, int show_id)
{
    if (!note)
        return;

    if (use_colors)
    {
        fputs(COLOR_BLUE, stdout);
        fputs("╭─ ", stdout);
        if (show_id)
        {
            printf("%s[%u]%s ", COLOR_YELLOW, note->id, COLOR_BLUE);
        }
        printf("%s%s%s", COLOR_BOLD, note->title, COLOR_RESET);
        if (note->tags[0])
            printf(" %s(%s)%s", COLOR_MAGENTA, note->tags, COLOR_RESET);
        fputs("\n", stdout);
    }
    else
    {
        fputs("╭─ ", stdout);
        if (show_id)
            printf("[%u] ", note->id);
        printf("%s", note->title);
        if (note->tags[0])
            printf(" (%s)", note->tags);
        fputs("\n", stdout);
    }
}

/* Print content lines without modifying the original buffer.
 * Splits on '\n' and prints each line with box glyphs.
 */
void cn_print_note_content(const cn_note *note)
{
    if (!note)
        return;

    if (use_colors)
        printf("%s├─ Content:%s\n", COLOR_BLUE, COLOR_RESET);
    else
        printf("├─ Content:\n");

    const char *p = note->content;
    if (!p || !*p)
        return;

    const char *line = p;
    const char *nl = NULL;
    while (line && *line)
    {
        nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);

        if (use_colors)
            printf("%s│%s  %.*s\n", COLOR_BLUE, COLOR_RESET, (int)len, line);
        else
            printf("│  %.*s\n", (int)len, line);

        if (!nl)
            break;
        line = nl + 1;
    }
}

/* Print created/modified timestamps. Uses localtime_r when available. */
void cn_print_note_timestamps(const cn_note *note)
{
    if (!note)
        return;

    char created_str[64] = "Invalid date";
    char modified_str[64] = "Invalid date";
    struct tm tm_info;

#if defined(__unix__) || defined(__APPLE__) || defined(_POSIX_VERSION)
    if (localtime_r(&note->created_at, &tm_info) != NULL)
    {
        strftime(created_str, sizeof(created_str), "%Y-%m-%d %H:%M", &tm_info);
    }
    if (localtime_r(&note->modified_at, &tm_info) != NULL)
    {
        strftime(modified_str, sizeof(modified_str), "%Y-%m-%d %H:%M", &tm_info);
    }
#else
    /* Fallback using localtime (not thread-safe but OK for CLI) */
    struct tm *t;
    t = localtime(&note->created_at);
    if (t)
        strftime(created_str, sizeof(created_str), "%Y-%m-%d %H:%M", t);
    t = localtime(&note->modified_at);
    if (t)
        strftime(modified_str, sizeof(modified_str), "%Y-%m-%d %H:%M", t);
#endif

    if (use_colors)
    {
        printf("%s├─ Timeline:%s\n", COLOR_BLUE, COLOR_RESET);
        printf("%s│%s  %sCreated:%s %s\n", COLOR_BLUE, COLOR_RESET, COLOR_DIM, COLOR_RESET, created_str);
        printf("%s│%s  %sModified:%s %s\n", COLOR_BLUE, COLOR_RESET, COLOR_DIM, COLOR_RESET, modified_str);
    }
    else
    {
        printf("├─ Timeline:\n");
        printf("│  Created: %s\n", created_str);
        printf("│  Modified: %s\n", modified_str);
    }
}

void cn_print_note_footer(void)
{
    if (use_colors)
        printf("%s╰─%s\n", COLOR_BLUE, COLOR_RESET);
    else
        printf("╰─\n");
}

/* Full note rendering */
void cn_print_note_full(const cn_note *note, int show_id)
{
    if (!note)
        return;
    cn_print_note_header(note, show_id);
    cn_print_note_content(note);
    cn_print_note_timestamps(note);
    cn_print_note_footer();
    fputc('\n', stdout);
}

/* Compact rendering: title + first line of content */
void cn_print_note_compact(const cn_note *note, int show_id)
{
    if (!note)
        return;

    if (use_colors)
    {
        if (show_id)
            printf("%s[%u]%s ", COLOR_YELLOW, note->id, COLOR_RESET);
        printf("%s%s%s", COLOR_BOLD, note->title, COLOR_RESET);
        if (note->tags[0])
            printf(" %s(%s)%s", COLOR_MAGENTA, note->tags, COLOR_RESET);
        fputc('\n', stdout);

        if (note->content[0])
        {
            const char *nl = strchr(note->content, '\n');
            size_t len = nl ? (size_t)(nl - note->content) : strlen(note->content);
            printf("  %s%.*s%s\n", COLOR_DIM, (int)len, note->content, COLOR_RESET);
        }
    }
    else
    {
        if (show_id)
            printf("[%u] ", note->id);
        printf("%s", note->title);
        if (note->tags[0])
            printf(" (%s)", note->tags);
        fputc('\n', stdout);

        if (note->content[0])
        {
            const char *nl = strchr(note->content, '\n');
            size_t len = nl ? (size_t)(nl - note->content) : strlen(note->content);
            printf("  %.*s\n", (int)len, note->content);
        }
    }

    fputc('\n', stdout);
}
