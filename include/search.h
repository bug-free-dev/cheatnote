#ifndef CN_SEARCH_H
#define CN_SEARCH_H

/*
 * search.h
 * Search and matching functions for notes.
 */

#include "cheatnote.h"
/* Tag & content matching */
int cn_note_match_tags(const char *note_tags, const char *search_tags);
int cn_note_match_content(const cn_note *note, const cn_search_opts *opts);

#endif /* CN_SEARCH_H */
