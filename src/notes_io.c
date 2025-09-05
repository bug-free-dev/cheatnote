#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>

#include "cheatnote.h"
#include "notes_io.h"
#include "db.h"
#include "utils.h"
#include "display.h"

/* extern globals (defined once in main.c) */
// db is declared as extern cn_note_db db; in cheatnote.h
extern int use_colors;
extern char db_path[PATH_MAX];

/* Internal helper: attempt to ensure there's room for at least one more note.
 * Returns 1 on success, 0 on failure.
 */
static int ensure_capacity_for_one(void)
{
    /* initialize db if needed */
    if (!db.notes)
    {
        /* cn_db_init will allocate INITIAL_CAPACITY */
        cn_db_init();
        return 1;
    }

    if (db.count < db.capacity)
        return 1;

    /* Prevent integer overflow when computing new capacity:
     * new_capacity = db.capacity * GROWTH_FACTOR
     */
    if (db.capacity > SIZE_MAX / GROWTH_FACTOR / sizeof(cn_note))
    {
        return 0;
    }

    size_t new_capacity = db.capacity * GROWTH_FACTOR;
    if (new_capacity > MAX_NOTES)
        new_capacity = MAX_NOTES;

    if (new_capacity <= db.capacity)
        return 0; /* cannot grow further */

    cn_note *new_notes = realloc(db.notes, new_capacity * sizeof(cn_note));
    if (!new_notes)
        return 0;

    /* zero initialize the newly allocated portion */
    size_t old_bytes = db.capacity * sizeof(cn_note);
    size_t new_bytes = new_capacity * sizeof(cn_note);
    if (new_bytes > old_bytes)
    {
        memset((char *)new_notes + old_bytes, 0, new_bytes - old_bytes);
    }

    db.notes = new_notes;
    db.capacity = new_capacity;
    return 1;
}

/*
 * Add a new note.
 * Returns new note ID on success, 0 on invalid input or failure.
 * Caller typically calls cn_db_save() after a successful add.
 */
unsigned int cn_note_add(const char *title, const char *content, const char *tags)
{
    if (!title || !content)
        return 0;

    if (title[0] == '\0' || content[0] == '\0')
        return 0;

    if (strlen(title) >= MAX_TITLE_LEN || strlen(content) >= MAX_CONTENT_LEN)
        return 0;

    if (tags && strlen(tags) >= MAX_TAGS_LEN)
        return 0;

    if (db.count >= MAX_NOTES)
    {
        /* fatal: user has reached maximum supported notes */
        cn_error_exit("Maximum number of notes reached");
    }

    if (!ensure_capacity_for_one())
    {
        cn_error_exit("Failed to resize database for new note");
    }

    cn_note *note = &db.notes[db.count];

    /* assign ID and protect against wrap to 0 */
    note->id = db.next_id++;
    if (db.next_id == 0) /* wrapped */
        db.next_id = 1;

    /* Safe copies into fixed-size arrays */
    cn_safe_strncpy(note->title, title, sizeof(note->title));
    cn_safe_strncpy(note->content, content, sizeof(note->content));
    if (tags && tags[0] != '\0')
        cn_safe_strncpy(note->tags, tags, sizeof(note->tags));
    else
        note->tags[0] = '\0';

    /* Trim whitespace in-place */
    cn_strip_whitespace(note->title);
    cn_strip_whitespace(note->content);
    cn_strip_whitespace(note->tags);

    time_t now = time(NULL);
    note->created_at = now;
    note->modified_at = now;

    db.count++;
    return note->id;
}

/*
 * Edit an existing note by ID.
 * Only non-NULL and non-empty title/content will replace fields.
 * tags may be NULL (no change) or empty string (clears tags).
 * Returns 1 on success, 0 if the note was not found or invalid parameters.
 * Caller should call cn_db_save() after a successful edit.
 */
int cn_note_edit(unsigned int id, const char *title, const char *content, const char *tags)
{
    if (id == 0)
        return 0;

    for (size_t i = 0; i < db.count; ++i)
    {
        if (db.notes[i].id == id)
        {
            cn_note *note = &db.notes[i];

            if (title && title[0] != '\0')
            {
                if (strlen(title) >= MAX_TITLE_LEN)
                    return 0;
                cn_safe_strncpy(note->title, title, sizeof(note->title));
                cn_strip_whitespace(note->title);
            }

            if (content && content[0] != '\0')
            {
                if (strlen(content) >= MAX_CONTENT_LEN)
                    return 0;
                cn_safe_strncpy(note->content, content, sizeof(note->content));
                cn_strip_whitespace(note->content);
            }

            if (tags)
            {
                /* tags provided; empty string clears tags */
                if (strlen(tags) >= MAX_TAGS_LEN)
                    return 0;
                cn_safe_strncpy(note->tags, tags, sizeof(note->tags));
                cn_strip_whitespace(note->tags);
            }

            note->modified_at = time(NULL);
            return 1;
        }
    }

    return 0; /* not found */
}

/*
 * Delete a note by ID.
 * Uses "move last element into this slot" trick for O(1) deletion order-not-preserving.
 * Returns 1 on success, 0 if note not found.
 * Caller should call cn_db_save() after a successful delete.
 */
int cn_note_delete(unsigned int id)
{
    if (id == 0)
        return 0;

    for (size_t i = 0; i < db.count; ++i)
    {
        if (db.notes[i].id == id)
        {
            /* replace this slot with the last note (if not already last) */
            if (i < db.count - 1)
            {
                db.notes[i] = db.notes[db.count - 1];
            }
            /* clear last slot */
            memset(&db.notes[db.count - 1], 0, sizeof(cn_note));
            db.count--;
            return 1;
        }
    }
    return 0;
}
