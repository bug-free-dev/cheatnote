#ifndef CN_UTILS_H
#define CN_UTILS_H

/*
 * utils.h
 * String utilities, CSV parsing, and terminal detection.
 */

#include <stddef.h>

/* String helpers */
void cn_safe_strncpy(char *dest, const char *src, size_t dest_size);
void cn_strip_whitespace(char *str);

/* CSV parsing */
int cn_parse_csv_field(char **pos, char *output, size_t max_len);

/* Environment / terminal */
int cn_is_terminal_output(void);

#endif /* CN_UTILS_H */
