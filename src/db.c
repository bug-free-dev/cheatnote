/*
 * src/db.c
 *
 * Database lifecycle and path management for CheatNote.
 *
 * Responsibilities:
 *  - Provide cn_db_init/cn_db_load/cn_db_save/cn_db_cleanup
 *  - Provide cn_get_db_path / cn_set_db_path (portable, XDG-aware)
 *  - Ensure parent directories exist (recursive mkdir)
 *
 * Safety & portability:
 *  - Bounds-checked path building
 *  - Check return values for all allocations / IO
 *  - Use atomic rename for database updates
 *
 * Globals:
 *  - Uses extern `db`, `use_colors`, `db_path` declared in cheatnote.h and defined in main.c
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* for snprintf, mkdir, etc. */
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cheatnote.h"
#include "db.h"
#include "utils.h"
#include "display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#define mkdir_p(path, mode) _mkdir(path)
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

/* Externals (defined in main.c) - declared in cheatnote.h as extern */
extern int use_colors;
extern char db_path[PATH_MAX];

/* Internal helpers forward */
static int make_parent_dirs(const char *path);
static const char *build_default_db_path(char *outbuf, size_t bufsz);
static int path_dirname(const char *path, char *outdir, size_t outdir_sz);

/* ------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------*/

/* Ensure DB structure is allocated (empty) */
void cn_db_init(void)
{
    if (db.notes)
        return;

    db.capacity = INITIAL_CAPACITY;
    db.notes = calloc(db.capacity, sizeof(cn_note));
    if (!db.notes)
    {
        cn_error_exit("Failed to allocate memory for database");
    }
    db.count = 0;
    db.next_id = 1;
}

/*
 * Build and return canonical DB path.
 * Caches result in global db_path (must be defined in main.c).
 */
const char *cn_get_db_path(void)
{
    if (db_path[0])
        return db_path;

    /* 1) Environment override */
    const char *env = getenv("CHEATNOTE_DB");
    if (env && env[0] && strlen(env) < PATH_MAX)
    {
        /* use provided path */
        cn_safe_strncpy(db_path, env, sizeof(db_path));
        return db_path;
    }

    /* 2) Build default path */
    if (!build_default_db_path(db_path, sizeof(db_path)))
    {
        /* fallback to local file */
        cn_safe_strncpy(db_path, "cheatnote.db", sizeof(db_path));
    }

    return db_path;
}

/* Allow user to override path programmatically */
void cn_set_db_path(const char *path)
{
    if (!path || path[0] == '\0')
    {
        /* reset */
        db_path[0] = '\0';
        return;
    }
    if (strlen(path) >= sizeof(db_path))
    {
        cn_error_exit("Database path too long");
    }
    cn_safe_strncpy(db_path, path, sizeof(db_path));
}

/*
 * Load database from disk (binary format). If file missing or corrupt,
 * initializes an empty DB.
 */
void cn_db_load(void)
{
    const char *path = cn_get_db_path();
    if (!path || path[0] == '\0')
    {
        cn_info_msg("No database path available; starting with in-memory DB");
        cn_db_init();
        return;
    }

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        /* Not an error: start fresh */
        cn_db_init();
        return;
    }

    size_t file_count = 0;
    unsigned int file_next_id = 0;

    if (fread(&file_count, sizeof(size_t), 1, f) != 1 ||
        fread(&file_next_id, sizeof(unsigned int), 1, f) != 1)
    {
        fclose(f);
        cn_info_msg("Database header corrupted, starting fresh");
        cn_db_init();
        return;
    }

    if (file_count > MAX_NOTES || file_next_id == 0)
    {
        fclose(f);
        cn_info_msg("Database parameters invalid, starting fresh");
        cn_db_init();
        return;
    }

    if (file_count == 0)
    {
        fclose(f);
        cn_db_init();
        db.next_id = file_next_id; /* preserve next_id */
        return;
    }

    /* Compute capacity (grow a bit to reduce reallocs) */
    size_t cap = (file_count < INITIAL_CAPACITY) ? INITIAL_CAPACITY : file_count;
    if (cap < file_count * GROWTH_FACTOR && file_count * GROWTH_FACTOR <= MAX_NOTES)
        cap = file_count * GROWTH_FACTOR;

    /* allocate space */
    cn_note *notes = calloc(cap, sizeof(cn_note));
    if (!notes)
    {
        fclose(f);
        cn_error_exit("Failed to allocate memory for database");
    }

    size_t actually_read = fread(notes, sizeof(cn_note), file_count, f);
    if (actually_read != file_count)
    {
        free(notes);
        fclose(f);
        cn_info_msg("Database records corrupted, starting fresh");
        cn_db_init();
        return;
    }

    /* loaded OK: replace current db */
    db.notes = notes;
    db.count = file_count;
    db.capacity = cap;
    db.next_id = file_next_id;

    fclose(f);

    /* Basic sanitization */
    for (size_t i = 0; i < db.count; ++i)
    {
        db.notes[i].title[MAX_TITLE_LEN - 1] = '\0';
        db.notes[i].content[MAX_CONTENT_LEN - 1] = '\0';
        db.notes[i].tags[MAX_TAGS_LEN - 1] = '\0';
        if (db.notes[i].id == 0 || db.notes[i].created_at < 0 || db.notes[i].modified_at < 0)
        {
            cn_info_msg("Found possibly corrupted record(s) in DB; continuing with preserved data");
        }
    }
}

/*
 * Save database to disk atomically (write to temp + rename).
 * On any write error the function will call cn_error_exit.
 */
void cn_db_save(void)
{
    const char *path = cn_get_db_path();
    if (!path || path[0] == '\0')
    {
        cn_error_exit("No database path available");
    }

    /* Ensure parent directory exists */
    char parent[PATH_MAX];
    if (path_dirname(path, parent, sizeof(parent)) && parent[0])
    {
        if (!make_parent_dirs(parent))
        {
            cn_error_exit("Failed to create database directory");
        }
    }

    /* Build temporary filename: "<path>.tmp" */
    char tmp[PATH_MAX];
    int ret = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (ret < 0 || (size_t)ret >= sizeof(tmp))
    {
        cn_error_exit("Temporary path too long");
    }

    FILE *f = fopen(tmp, "wb");
    if (!f)
    {
        cn_error_exit("Failed to open temporary database file for writing");
    }

    /* Write header */
    if (fwrite(&db.count, sizeof(size_t), 1, f) != 1 ||
        fwrite(&db.next_id, sizeof(unsigned int), 1, f) != 1)
    {
        fclose(f);
        (void)remove(tmp);
        cn_error_exit("Failed to write database header");
    }

    /* Write notes */
    if (db.count > 0)
    {
        if (fwrite(db.notes, sizeof(cn_note), db.count, f) != db.count)
        {
            fclose(f);
            (void)remove(tmp);
            cn_error_exit("Failed to write database records");
        }
    }

    if (fclose(f) != 0)
    {
        (void)remove(tmp);
        cn_error_exit("Failed to close temporary database file");
    }

    /* Atomic rename to final path */
    if (rename(tmp, path) != 0)
    {
        (void)remove(tmp);
        cn_error_exit("Failed to update database file");
    }
}

/* Free DB memory */
void cn_db_cleanup(void)
{
    if (db.notes)
    {
        free(db.notes);
        db.notes = NULL;
    }
    db.count = 0;
    db.capacity = 0;
    db.next_id = 1;
}

/* ------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------*/

/*
 * Create directories recursively like "mkdir -p".
 * Returns 1 on success (or already exists), 0 on error.
 */
static int make_parent_dirs(const char *path)
{
    if (!path || *path == '\0')
        return 1;

    size_t len = strlen(path);
    if (len == 0 || len >= PATH_MAX)
        return 0;

    char tmp[PATH_MAX];
    cn_safe_strncpy(tmp, path, sizeof(tmp));

    /* Remove trailing separators */
    while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\'))
    {
        tmp[len - 1] = '\0';
        --len;
    }

    /* Iterate and create each prefix */
    for (size_t i = 1; i <= len; ++i)
    {
        if (tmp[i] == '/' || tmp[i] == '\\' || tmp[i] == '\0')
        {
            char save = tmp[i];
            tmp[i] = '\0';

            struct stat st;
            if (stat(tmp, &st) != 0)
            {
                /* doesn't exist: create */
#if defined(_WIN32) || defined(_WIN64)
                if (_mkdir(tmp) != 0)
                {
                    if (errno != EEXIST)
                        return 0;
                }
#else
                if (mkdir(tmp, 0700) != 0)
                {
                    if (errno != EEXIST)
                        return 0;
                }
#endif
            }
            else
            {
                /* exists: ensure it's a dir */
                if (!S_ISDIR(st.st_mode))
                    return 0;
            }

            tmp[i] = save;
        }
    }

    return 1;
}

/*
 * Build a default DB path into outbuf (size bufsz).
 * Returns 1 on success, 0 on failure.
 */

static const char *build_default_db_path(char *outbuf, size_t bufsz)
{
    if (!outbuf || bufsz == 0)
        return NULL;

#if defined(_WIN32) || defined(_WIN64)
    const char *base = appdata_home();
    if (base && base[0])
    {
        int r = snprintf(outbuf, bufsz, "%s%ccheatnote%ccheatnote.db", base, PATH_SEP, PATH_SEP);
        return (r > 0 && (size_t)r < bufsz) ? outbuf : NULL;
    }
    /* fallback to current directory */
    if (snprintf(outbuf, bufsz, "cheatnote.db") >= (int)bufsz)
        return NULL;
    return outbuf;
#else
    const char *env_xdg = getenv("XDG_DATA_HOME");
    if (env_xdg && env_xdg[0])
    {
        int r = snprintf(outbuf, bufsz, "%s%ccheatnote%ccheatnote.db", env_xdg, PATH_SEP, PATH_SEP);
        return (r > 0 && (size_t)r < bufsz) ? outbuf : NULL;
    }

    const char *home = getenv("HOME");
    if (home && home[0])
    {
        int r = snprintf(outbuf, bufsz, "%s%c.local%cshare%ccheatnote%ccheatnote.db",
                         home, PATH_SEP, PATH_SEP, PATH_SEP, PATH_SEP);
        return (r > 0 && (size_t)r < bufsz) ? outbuf : NULL;
    }

    /* fallback */
    if (snprintf(outbuf, bufsz, "cheatnote.db") >= (int)bufsz)
        return NULL;
    return outbuf;
#endif
}

/*
 * Path dirname helper: copies directory portion of path into outdir.
 * Returns 1 on success, 0 on failure.
 */
static int path_dirname(const char *path, char *outdir, size_t outdir_sz)
{
    if (!path || !outdir || outdir_sz == 0)
        return 0;
    const char *p = path + strlen(path);
    /* move left until separator or begin */
    while (p > path && *p != '/' && *p != '\\')
        --p;
    if (p == path)
    {
        /* no directory component */
        outdir[0] = '\0';
        return 1;
    }
    size_t len = (size_t)(p - path);
    if (len >= outdir_sz)
        return 0;
    memcpy(outdir, path, len);
    outdir[len] = '\0';
    return 1;
}
