#!/usr/bin/env bash
set -euo pipefail

# Paths
BIN="./bin/cheatnote"
TESTDIR="./tests"
DB="$TESTDIR/cheatnote_test.db"
EXPORT="$TESTDIR/export.csv"
IMPORT="$TESTDIR/import.csv"
mkdir -p "$TESTDIR"
rm -f "$DB" "$EXPORT" "$IMPORT"

# Helper
run() {
    echo -e "\n$ $*"
    "$@"
}

echo "== CheatNote Super Comprehensive Test Suite =="

# 1. Add notes (various ways, edge cases)
run $BIN add "First Note" "First content" "tag1,tag2"
run $BIN add -t "Second Note" -c "Second content" -g "tag2,tag3"
run $BIN add "Third Note" "Third content" ""
run $BIN add "NoTag Note" "No tag content"
run $BIN add "Multiline" $'Line1\nLine2\nLine3' "multi"
run $BIN add "SpecialChars" "!@#$%^&*()_+" "specialchars"
run $BIN add "Unicode" "Привет мир" "unicode,тест"
run $BIN add "LongTag" "Content" "$(printf 'tag%.0s,' {1..50})"

# 2. List/search notes (all, compact, by tag, by search, regex, case-insensitive, exact, word-boundary, multiline, no-ids, advanced)
run $BIN list
run $BIN list -c
run $BIN list -g "tag2"
run $BIN list -g "unicode"
run $BIN list -s "first"
run $BIN list -s "FIRST" -i
run $BIN list -s "^First" -r
run $BIN list -s "Note" -e
run $BIN list -s "Line1" -w
run $BIN list -s "Line2" -m
run $BIN list -n
run $BIN list -s "content" -g "tag2,tag3" -i -c
run $BIN list -s "^Line" -r -m
run $BIN list -s "[A-Z][a-z]+ Note" -r
run $BIN list -s "!@#" -g "specialchars"
run $BIN list -s "мир" -g "unicode" -i

# 3. Edit notes (title, content, tags, clear tags, edge cases)
run $BIN edit 1 "First Note Edited" "Updated content" "edited"
run $BIN edit 2 "" "Second content updated"
run $BIN edit 3 "" "" ""
run $BIN edit 4 "" "" ""
run $BIN edit 5 "Multiline Edited" $'New1\nNew2' ""
run $BIN edit 6 "SpecialChars!" "!@#$$%^" "special,chars"
run $BIN edit 7 "Unicode Edited" "Здравствуйте" "юникод"

# 4. List after edits (all, compact, advanced)
run $BIN list
run $BIN list -c
run $BIN list -s "Edited" -i
run $BIN list -s "New1" -w
run $BIN list -s "юникод" -g "юникод"

# 5. Delete notes (first, last, middle, non-existent, edge cases)
run $BIN delete 1
run $BIN delete 8
run $BIN delete 100 || echo "Expected: delete non-existent note failed"
run $BIN delete 0 || echo "Expected: delete invalid note failed"

# 6. Export and import (corrupt file, empty file, merge mode)
run $BIN export "$EXPORT"
cat "$EXPORT"
cp "$EXPORT" "$IMPORT"
run $BIN import "$IMPORT"
run $BIN import -m "$IMPORT"
echo "" > "$IMPORT"
run $BIN import "$IMPORT" || echo "Expected: import empty file error"
echo 'corrupt,data,not,valid' > "$IMPORT"
run $BIN import "$IMPORT" || echo "Expected: import corrupt file error"

# 7. Stats, help, version, after modifications
run $BIN stats
run $BIN help
run $BIN version

# 8. Error handling (missing args, invalid ID, too long, too many tags, invalid CSV, etc.)
echo -e "\n# Error: add with missing title"
! $BIN add "" "content" && echo "Expected error"
echo -e "\n# Error: edit with invalid ID"
! $BIN edit 9999 "bad" "bad" && echo "Expected error"
echo -e "\n# Error: delete with invalid ID"
! $BIN delete 0 && echo "Expected error"
echo -e "\n# Error: add with too long title"
longtitle=$(head -c 300 < /dev/zero | tr '\0' 'A')
! $BIN add "$longtitle" "content" && echo "Expected error"
echo -e "\n# Error: add with too many tags"
longtags=$(head -c 300 < /dev/zero | tr '\0' 't')
! $BIN add "T" "C" "$longtags" && echo "Expected error"
echo -e "\n# Error: import with invalid CSV"
echo 'ID,Title,Content,Tags,Created,Modified\n1,"bad' > "$IMPORT"
! $BIN import "$IMPORT" && echo "Expected error"

# 9. Color output check (if terminal supports)
echo -e "\n# Color output check"
CHEATNOTE_DB="$DB" $BIN list --no-color

# 10. Concurrency: run multiple commands in parallel (background)
echo -e "\n# Concurrency test"
for i in {1..5}; do $BIN add "Parallel $i" "Content $i" "p$i" & done
wait
run $BIN list -g "p1,p2,p3,p4,p5"

# 11. Clean up
rm -f "$DB" "$EXPORT" "$IMPORT"
echo -e "\nAll tests completed successfully."