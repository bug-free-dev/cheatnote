#ifndef CN_DB_H
#define CN_DB_H

/*
 * db.h
 * Database lifecycle and persistence for CheatNote.
 */

/* Lifecycle */
void cn_db_init(void);
void cn_db_load(void);
void cn_db_save(void);
void cn_db_cleanup(void);

/* Path helpers */
const char *cn_get_db_path(void);
void cn_set_db_path(const char *path);

#endif /* CN_DB_H */
