# Makefile -- portable build & test system for cheatnote
# Supports: GCC, Clang, Linux, macOS, *BSD
# - Release: optimized but portable
# - Debug: symbols, no optimization
# - Test: smoke tests with temp DB
# - Valgrind: memory leak check (Linux/WSL only)

PROJECT = cheatnote
SRCDIR  = src
INCDIR  = include
BINDIR  = bin
OBJDIR  = build
TESTDIR = tests

CC ?= cc   # use system default compiler (gcc/clang)
INSTALL ?= install
PREFIX ?= /usr/local
BINDIR_INSTALL ?= $(PREFIX)/bin

# Flags: safe defaults for portability
COMMON_FLAGS   = -std=c11 -Wall -Wextra -Wpedantic
RELEASE_FLAGS  = -O3 -pipe
DEBUG_FLAGS    = -g -O0 -DDEBUG

ifeq ($(BUILD),debug)
CFLAGS = $(COMMON_FLAGS) $(DEBUG_FLAGS)
LDFLAGS =
else
CFLAGS = $(COMMON_FLAGS) $(RELEASE_FLAGS)
LDFLAGS =
endif

SRC  = $(wildcard $(SRCDIR)/*.c)
OBJ  = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC))
DEPS = $(OBJ:.o=.d)
BIN  = $(BINDIR)/$(PROJECT)

TEST_DB := $(CURDIR)/$(TESTDIR)/cheatnote_test.db

.PHONY: all release debug clean test valgrind install uninstall help

all: release

release: BUILD = release
release: $(BIN)

debug: BUILD = debug
debug: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Built $@ (mode=$(BUILD))"

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -MMD -MP -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR)

clean:
	-rm -rf $(OBJDIR) $(BINDIR) $(TESTDIR)/cheatnote_test.db $(TESTDIR)/export.csv
	@echo "Cleaned."

# Smoke test
test: all
	@mkdir -p $(TESTDIR)
	@echo "Running smoke test (DB: $(TEST_DB))"
	@rm -f $(TEST_DB)
	@CHEATNOTE_DB=$(TEST_DB) $(BIN) add "smoke" "hello world" "tag"
	@CHEATNOTE_DB=$(TEST_DB) $(BIN) list
	@CHEATNOTE_DB=$(TEST_DB) $(BIN) edit 1 "new title" "new content"
	@CHEATNOTE_DB=$(TEST_DB) $(BIN) export $(TESTDIR)/export.csv
	@CHEATNOTE_DB=$(TEST_DB) $(BIN) import -m $(TESTDIR)/export.csv
	@CHEATNOTE_DB=$(TEST_DB) $(BIN) delete 1
	@echo "Smoke test OK."

# Run under valgrind if present (Linux/WSL only)
valgrind: all
	@mkdir -p $(TESTDIR)
	@if command -v valgrind >/dev/null 2>&1; then \
	  echo "Valgrind run (DB: $(TEST_DB))"; \
	  rm -f $(TEST_DB); \
	  CHEATNOTE_DB=$(TEST_DB) $(BIN) add "vtest" "vcontent" "vtag" >/dev/null; \
	  CHEATNOTE_DB=$(TEST_DB) valgrind --leak-check=full --show-leak-kinds=all \
	    --error-exitcode=2 $(BIN) list; \
	else \
	  echo "Valgrind not installed (skipping)"; \
	fi

install: all
	@mkdir -p $(DESTDIR)$(BINDIR_INSTALL)
	$(INSTALL) -m 755 $(BIN) $(DESTDIR)$(BINDIR_INSTALL)/
	@echo "Installed to $(DESTDIR)$(BINDIR_INSTALL)/$(PROJECT)"

uninstall:
	-rm -f $(DESTDIR)$(BINDIR_INSTALL)/$(PROJECT)
	@echo "Uninstalled $(PROJECT)"

help:
	@echo "Targets:"
	@echo "  make [all]       Build release (default)"
	@echo "  make release     Build optimized"
	@echo "  make debug       Build with debug info"
	@echo "  make test        Run smoke tests"
	@echo "  make valgrind    Run valgrind test (Linux only)"
	@echo "  make install     Install to \$$PREFIX (default: /usr/local)"
	@echo "  make uninstall   Remove installed binary"
	@echo "  make clean       Remove build artifacts"

-include $(DEPS)
