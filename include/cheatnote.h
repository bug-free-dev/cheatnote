#ifndef CHEATNOTE_H
#define CHEATNOTE_H

/*
 * cheatnote.h
 *
 * Core macros, typedefs and shared data structures for CheatNote.
 * This header intentionally contains *no* function prototypes — only
 * compile-time constants, color codes, and structs as requested.
 *
 * NOTE: Global variables are declared as `extern` here. Define them
 *       exactly once in a single .c file:
 *
 **/

#include <time.h>
#include <stddef.h>
#include <limits.h>
#include <stdint.h>

/* Version and limits */

#define VERSION "3"
#define MAX_TITLE_LEN 256
#define MAX_CONTENT_LEN 8192
#define MAX_TAGS_LEN 512
#define MAX_TAG_COUNT 32
#define MAX_SEARCH_LEN 256
#define INITIAL_CAPACITY 64
#define GROWTH_FACTOR 2
#define PATH_MAX 4096
#define MAX_NOTES 1000000
#define MAX_LINE_LENGTH (MAX_TITLE_LEN + MAX_CONTENT_LEN + MAX_TAGS_LEN + 256)

/* ANSI color codes (terminal) */
#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_DIM "\033[2m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"

#define COLOR_GRAY "\033[90m"

/* === Data structures === */

typedef struct cn_note
{
    unsigned int id;
    char title[MAX_TITLE_LEN];
    char content[MAX_CONTENT_LEN];
    char tags[MAX_TAGS_LEN];
    time_t created_at;
    time_t modified_at;
} cn_note;

typedef struct cn_note_db
{
    cn_note *notes;
    size_t count;
    size_t capacity;
    unsigned int next_id;
} cn_note_db;

typedef struct cn_search_opts
{
    const char *pattern;
    const char *tags;
    int case_insensitive;
    int regex_mode;
    int exact_match;
    int word_boundary;
    int multiline_mode;
} cn_search_opts;

/*
 * Globals (declared here, define in exactly one C file)
 */
extern cn_note_db db;
extern int use_colors;
extern char db_path[PATH_MAX];

#ifdef __cplusplus
extern "C"
{
#endif

    /* (no functions here) */

#ifdef __cplusplus
}
#endif

#endif /* CHEATNOTE_H */
