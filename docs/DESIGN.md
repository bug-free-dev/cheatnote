# CheatNote Design & Architecture

## Overview

CheatNote is a blazing fast and portable command-line snippet/note manager written in C.

---

## Table of Contents

1. Key Design Goals
2. Architecture Overview
3. Data Model
4. File Formats
5. Component Breakdown
6. Error Handling
7. Portability
8. Testing
9. Extending CheatNote
10. Security Considerations
11. Limitations
12. Examples
13. References

---

## 1. Key Design Goals

- **Speed:** All operations are in-memory for fast access; disk writes are atomic.
- **Safety:** All allocations and I/O are checked; no unchecked buffer writes.
- **Portability:** Works on Linux, macOS, and Windows (with ANSI support).
- **Extensibility:** Modular codebase, easy to add new commands or features.
- **Testability:** Comprehensive test suite and smoke tests.

---

## 2. Architecture Overview

CheatNote is structured as a modular C project, with each major feature isolated in its own source and header files. The main entry point (`main.c`) handles global state and command dispatch. All user-facing commands are implemented in `commands.c`, which calls into lower-level modules for database, search, and display logic.

---

## 3. Data Model

### Note Structure (`cn_note`)
- `id` (unsigned int): Unique, auto-incremented
- `title` (char[MAX_TITLE_LEN])
- `content` (char[MAX_CONTENT_LEN])
- `tags` (char[MAX_TAGS_LEN])
- `created_at`, `modified_at` (time_t)

### Database (`cn_note_db`)
- `notes` (dynamic array of `cn_note`)
- `count`, `capacity`, `next_id`

---

## 4. File Formats

### Binary Database
- Header: [count][next_id]
- Array of `cn_note` structs
- Atomic save: write to temp file, then rename

### CSV Import/Export
- Standard CSV with quoted/escaped fields
- Fields: ID,Title,Content,Tags,Created,Modified

---

## 5. Component Breakdown

- `main.c`        Entry point, global state, command dispatch
- `commands.c`    CLI command handlers (add, edit, delete, list, etc.)
- `notes_io.c`    CRUD operations for notes
- `db.c`          Database load/save, path management
- `search.c`      Search and matching (regex, tags, etc.)
- `display.c`     Output formatting, color, info/error messages
- `utils.c`       String helpers, CSV parsing, terminal detection

---

## 6. Error Handling

- All errors print a clear message and exit (or return error code for non-fatal cases)
- All allocations and file operations are checked
- Defensive programming: bounds checks, null checks, integer overflow checks

---

## 7. Portability

- Uses only standard C and POSIX APIs (with Windows support for ANSI/paths)
- All platform-specific code is isolated and guarded by `#ifdef`

---

## 8. Testing

- `make test` runs a full smoke test (add, list, edit, export, import, delete)
- `tests/test.sh` runs a comprehensive suite (all features, edge cases, error handling)
- `make valgrind` checks for memory leaks/errors

---

## 9. Extending CheatNote

- Add new commands in `commands.c` and declare in `commands.h`
- Add new fields to `cn_note` (bump DB version if binary format changes)
- Add new search/filter options in `search.c`

---

## 10. Security Considerations

- All user input is bounds-checked
- No shell eval or unsafe system() calls
- No dynamic memory leaks (valgrind clean)

---

## 11. Limitations

- Not multi-user or concurrent (single process at a time)
- No encryption (plaintext DB)
- No network sync (local only)

---

## 12. Examples

### Add a Note
```sh
cheatnote add "Git status" "git status -s" "git,status"
```

### List Notes (with search)
```sh
cheatnote list "git" -r -i
```

### Edit a Note
```sh
cheatnote edit 1 "New Title" "New Content"
```

### Delete a Note
```sh
cheatnote delete 1
```

### Export Notes
```sh
cheatnote export notes.csv
```

### Import Notes
```sh
cheatnote import notes.csv
```

---

## 13. References

- See `README.md` for usage and build instructions
- See `src/` and `include/` for code

---
