/*
 * src/utils.c
 *
 * Small utility helpers used across the CheatNote codebase.
 *
 * - cn_safe_strncpy : bounded, NUL-terminated copy
 * - cn_strip_whitespace : in-place trim (leading + trailing)
 * - cn_parse_csv_field : parse one CSV field from a line (supports quoted fields and "" escapes)
 * - cn_is_terminal_output : wrapper around isatty for stdout
 *
 * Keep these functions small and portable. They intentionally avoid dynamic allocation.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "cheatnote.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h> /* isatty */
#include <limits.h>
#include <errno.h>

/* Safe bounded copy: always NUL-terminate dest (even if dest_size == 1).
 * Silently no-ops on invalid inputs.
 */
void cn_safe_strncpy(char *dest, const char *src, size_t dest_size)
{
    if (!dest || !src || dest_size == 0)
        return;

    /* copy up to dest_size - 1 bytes then NUL terminate */
    size_t i;
    for (i = 0; i + 1 < dest_size && src[i] != '\0'; ++i)
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/* Trim leading and trailing whitespace in-place.
 * Uses unsigned char cast before isspace to avoid UB on negative char values.
 */
void cn_strip_whitespace(char *str)
{
    if (!str || !*str)
        return;

    /* Trim trailing whitespace */
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1]))
    {
        str[--len] = '\0';
    }

    /* Trim leading whitespace by memmove */
    size_t start = 0;
    while (str[start] && isspace((unsigned char)str[start]))
        ++start;

    if (start > 0)
    {
        size_t remaining = strlen(str + start);
        memmove(str, str + start, remaining + 1); /* include terminating NUL */
    }
}

/*
 * Parse a single CSV field from *pos (which is updated to point after the parsed field).
 *
 * Rules:
 *  - Skips leading whitespace before a field.
 *  - If field starts with a double-quote, parse a quoted field:
 *      - Two consecutive double-quotes ("") within a quoted field mean a literal double-quote.
 *      - The quoted field ends at the next unescaped double-quote.
 *  - If not quoted, the field ends at the next comma or end-of-line.
 *  - After parsing, *pos is advanced to the character after the comma (or to the end).
 *
 * Writes at most max_len-1 bytes into output and ensures NUL termination.
 *
 * Returns 1 on success, 0 on error (invalid inputs or no progress).
 */
int cn_parse_csv_field(char **pos, char *output, size_t max_len)
{
    if (!pos || !*pos || !output || max_len == 0)
        return 0;

    char *p = *pos;
    char *out = output;
    size_t written = 0;
    int in_quotes = 0;

    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p))
        ++p;

    /* Quoted field? */
    if (*p == '"')
    {
        in_quotes = 1;
        ++p; /* skip opening quote */
    }

    while (*p != '\0')
    {
        if (in_quotes)
        {
            if (*p == '"')
            {
                if (*(p + 1) == '"')
                {
                    /* escaped quote -> write one quote and advance two chars */
                    if (written + 1 < max_len)
                    {
                        *out++ = '"';
                        ++written;
                    }
                    p += 2;
                    continue;
                }
                else
                {
                    /* closing quote */
                    ++p; /* skip closing quote */
                    in_quotes = 0;
                    break;
                }
            }
            else
            {
                /* regular character inside quotes */
                if (written + 1 < max_len)
                {
                    *out++ = *p;
                    ++written;
                }
                ++p;
            }
        }
        else
        {
            /* Not inside quotes: comma or end marks field end */
            if (*p == ',')
                break;
            if (written + 1 < max_len)
            {
                *out++ = *p;
                ++written;
            }
            ++p;
        }
    }

    /* Null-terminate output (always) */
    *out = '\0';

    /* Advance p to after the comma (if present) */
    while (*p && *p != ',')
        ++p;
    if (*p == ',')
        ++p;

    /* Skip any whitespace after the comma for next field */
    while (*p && isspace((unsigned char)*p))
        ++p;

    /* Update caller pointer */
    *pos = p;

    return 1;
}

/* Return 1 if stdout is a terminal (TTY), 0 otherwise.
 * This just wraps isatty(STDOUT_FILENO) but keeps the API name consistent.
 */
int cn_is_terminal_output(void)
{
#if defined(_WIN32) || defined(_WIN64)
    /* On Windows, isatty is available via <io.h> as _isatty; but include unistd.h above covers POSIX.
       If Windows-specific behavior is required (e.g., enabling virtual terminal sequences), handle in display.c. */
    return isatty(fileno(stdout));
#else
    return isatty(STDOUT_FILENO);
#endif
}
