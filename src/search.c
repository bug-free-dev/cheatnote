/*
 * src/search.c
 *
 * Search & matching utilities for CheatNote.
 *
 * Provides:
 *   - cn_note_match_tags(const char *note_tags, const char *search_tags)
 *   - cn_note_match_content(const cn_note *note, const cn_search_opts *opts)
 *
 * Behavior:
 *   - Tag match: case-insensitive, comma-separated; empty search_tags => match all.
 *   - Content match: supports regex mode (POSIX regex) and substring mode.
 *     - In regex mode, supports case-insensitive and multiline flags.
 *     - In substring mode, supports case-insensitive and exact-match options.
 *
 * Safety:
 *   - All allocations are checked and freed.
 *   - No writes beyond allocated buffers.
 *   - Uses explicit casts to (unsigned char) for ctype functions to avoid UB.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <limits.h>

#include "cheatnote.h"
#include "search.h"
#include "utils.h" /* for cn_safe_strncpy, cn_strip_whitespace if needed */

/* ------------ Tag matching -------------- */
/* Case-insensitive comma-separated match.
 * Returns 1 if search_tags is NULL/empty (match-all), 0 otherwise.
 */
int cn_note_match_tags(const char *note_tags, const char *search_tags)
{
    if (!search_tags || !*search_tags)
        return 1; /* no tag filter => match */

    if (!note_tags || !*note_tags)
        return 0; /* note has no tags but search requires some */

    size_t note_len = strlen(note_tags);
    size_t search_len = strlen(search_tags);

    /* Basic sanity checks */
    if (note_len >= MAX_TAGS_LEN || search_len >= MAX_TAGS_LEN)
        return 0;

    char *note_copy = malloc(note_len + 1);
    char *search_copy = malloc(search_len + 1);
    if (!note_copy || !search_copy)
    {
        free(note_copy);
        free(search_copy);
        return 0;
    }

    strcpy(note_copy, note_tags);
    strcpy(search_copy, search_tags);

    /* Lowercase both for case-insensitive match */
    for (char *p = note_copy; *p; ++p)
        *p = (char)tolower((unsigned char)*p);
    for (char *p = search_copy; *p; ++p)
        *p = (char)tolower((unsigned char)*p);

    /* Tokenize search_copy by commas and check each token is a substring of note_copy */
    int match = 1;
    char *saveptr = NULL;
    char *token = strtok_r(search_copy, ",", &saveptr);
    while (token)
    {
        /* Trim whitespace around token */
        cn_strip_whitespace(token);
        if (*token != '\0')
        {
            if (!strstr(note_copy, token))
            {
                match = 0;
                break;
            }
        }
        token = strtok_r(NULL, ",", &saveptr);
    }

    free(note_copy);
    free(search_copy);
    return match;
}

/* ------------ Content/title/tags matching -------------- */
/* Returns 1 if note matches the search options, 0 otherwise.
 * opts->pattern may be NULL or empty => match-all.
 */
int cn_note_match_content(const cn_note *note, const cn_search_opts *opts)
{
    if (!note || !opts)
        return 0;

    if (!opts->pattern || !*opts->pattern)
        return 1; /* nothing to search => matches */

    /* REGEX MODE */
    if (opts->regex_mode)
    {
        regex_t regex;
        int flags = REG_EXTENDED | REG_NOSUB;
#ifdef REG_NEWLINE
        if (opts->multiline_mode)
            flags |= REG_NEWLINE;
#endif
#ifdef REG_ICASE
        if (opts->case_insensitive)
            flags |= REG_ICASE;
#endif

        /* Build pattern with optional word boundary wrapping */
        size_t patlen = strlen(opts->pattern);
        size_t need = patlen + 16; /* room for \b .. \b and NUL */
        if (need > MAX_SEARCH_LEN * 4)
        {
            /* pattern too large */
            return 0;
        }

        char *pattern_buf = malloc(need);
        if (!pattern_buf)
            return 0;

        if (opts->word_boundary)
        {
            /* wrap with word boundary tokens (\b), which need escaping in C string */
            int rc = snprintf(pattern_buf, need, "\\b%s\\b", opts->pattern);
            if (rc < 0 || (size_t)rc >= need)
            {
                free(pattern_buf);
                return 0;
            }
        }
        else
        {
            /* copy pattern safely */
            if (patlen + 1 > need)
            {
                free(pattern_buf);
                return 0;
            }
            strcpy(pattern_buf, opts->pattern);
        }

        /* compile */
        if (regcomp(&regex, pattern_buf, flags) != 0)
        {
            free(pattern_buf);
            return 0;
        }

        int matched = 0;
        /* try title */
        if (regexec(&regex, note->title, 0, NULL, 0) == 0)
            matched = 1;
        /* try content */
        else if (regexec(&regex, note->content, 0, NULL, 0) == 0)
            matched = 1;
        /* try tags */
        else if (regexec(&regex, note->tags, 0, NULL, 0) == 0)
            matched = 1;

        regfree(&regex);
        free(pattern_buf);
        return matched ? 1 : 0;
    }

    /* NON-REGEX MODE (substring/exact) */
    const char *search = opts->pattern;
    char *title_copy = NULL;
    char *content_copy = NULL;
    char *tags_copy = NULL;
    char *search_copy = NULL;

    if (opts->case_insensitive)
    {
        /* allocate lowercased copies */
        size_t title_len = strlen(note->title);
        size_t content_len = strlen(note->content);
        size_t tags_len = strlen(note->tags);
        size_t search_len = strlen(search);

        /* Bounds sanity */
        if (title_len > MAX_CONTENT_LEN || content_len > MAX_CONTENT_LEN || tags_len > MAX_TAGS_LEN || search_len > MAX_SEARCH_LEN)
        {
            /* refuse very large inputs to avoid surprises */
            return 0;
        }

        title_copy = malloc(title_len + 1);
        content_copy = malloc(content_len + 1);
        tags_copy = malloc(tags_len + 1);
        search_copy = malloc(search_len + 1);
        if (!title_copy || !content_copy || !tags_copy || !search_copy)
        {
            free(title_copy);
            free(content_copy);
            free(tags_copy);
            free(search_copy);
            return 0;
        }

        strcpy(title_copy, note->title);
        strcpy(content_copy, note->content);
        strcpy(tags_copy, note->tags);
        strcpy(search_copy, search);

        for (char *p = title_copy; *p; ++p)
            *p = (char)tolower((unsigned char)*p);
        for (char *p = content_copy; *p; ++p)
            *p = (char)tolower((unsigned char)*p);
        for (char *p = tags_copy; *p; ++p)
            *p = (char)tolower((unsigned char)*p);
        for (char *p = search_copy; *p; ++p)
            *p = (char)tolower((unsigned char)*p);

        /* redirect pointers to lowercase buffers */
        search = search_copy;
    }

    const char *title = title_copy ? title_copy : note->title;
    const char *content = content_copy ? content_copy : note->content;
    const char *tags = tags_copy ? tags_copy : note->tags;

    int matched = 0;
    if (opts->exact_match)
    {
        if (strcmp(title, search) == 0 || strcmp(content, search) == 0 || strcmp(tags, search) == 0)
            matched = 1;
    }
    else
    {
        if (strstr(title, search) != NULL || strstr(content, search) != NULL || strstr(tags, search) != NULL)
            matched = 1;
    }

    free(title_copy);
    free(content_copy);
    free(tags_copy);
    free(search_copy);

    return matched ? 1 : 0;
}
