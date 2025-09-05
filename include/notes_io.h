#ifndef CN_NOTES_IO_H
#define CN_NOTES_IO_H

/*
 * notes_io.h
 * CRUD operations for notes: add, edit, delete.
 */

/* CRUD */
unsigned int cn_note_add(const char *title, const char *content, const char *tags);
int cn_note_edit(unsigned int id, const char *title, const char *content, const char *tags);
int cn_note_delete(unsigned int id);

#endif /* CN_NOTES_IO_H */
