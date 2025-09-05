#ifndef CN_DISPLAY_H
#define CN_DISPLAY_H
#include "cheatnote.h"
/*
 * display.h
 * Rendering and user-facing messages for CheatNote.
 */

/* Color control */
void cn_set_use_colors(int enabled);

/* Note rendering */
void cn_print_note_full(const cn_note *note, int show_id);
void cn_print_note_compact(const cn_note *note, int show_id);
void cn_print_note_header(const cn_note *note, int show_id);
void cn_print_note_content(const cn_note *note);
void cn_print_note_timestamps(const cn_note *note);
void cn_print_note_footer(void);

/* Status messages */
void cn_info_msg(const char *msg);
void cn_success_msg(const char *msg);
void cn_error_exit(const char *msg);

#endif /* CN_DISPLAY_H */
