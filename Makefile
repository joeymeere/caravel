#   make                 build the CLI (build/caravel)
#   make install         install the CLI + SDK headers (PREFIX, DESTDIR)
#   make uninstall
#
#   make test            host unit tests + SBF compile matrix
#   make test-host       header unit tests on the host (ASan + UBSan)
#   make test-compile    compile probe across SBPF versions, C standards, options
#   make test-examples   `caravel test` every example (SBF_VER=v0|v3)
#   make bench           compute-unit / binary-size regression check
#   make bench-update    refresh etc/baseline.json
#   make check           everything CI runs
#
#   make format          clang-format all C sources in place
#   make format-check    fail if anything is not formatted (CI)
#
#   make platform-tools  install platform-tools $(PLATFORM_TOOLS)
#   make clean
#
# args: CC, CFLAGS (appended), WERROR=1, SAN= (no sanitizers), SBF_VER,
#        PLATFORM_TOOLS, PREFIX, DESTDIR, BENCH_ARGS.

BUILD          := build
PREFIX         ?= $(HOME)/.local
DESTDIR        ?=
DATADIR        := $(PREFIX)/share/caravel
SBF_VER        ?= v0
PLATFORM_TOOLS ?= v1.57
EXAMPLES       := counter vault token-vault amm

# Output differs between clang-format releases; CI pins this version
# (`pip install clang-format==$(CLANG_FORMAT_VERSION)`).
CLANG_FORMAT         ?= clang-format
CLANG_FORMAT_VERSION := 22.1.8
FORMAT_SRCS = $(shell git ls-files -co --exclude-standard '*.c' '*.h' \
                  | grep -v node_modules | while read f; do \
                      [ -f "$$f" ] && echo "$$f"; done)

CLI      := $(BUILD)/caravel
CLI_SRCS := $(wildcard cli/*.c cli/cmd/*.c cli/util/*.c)
CLI_HDRS := $(wildcard cli/*.h cli/util/*.h cli/templates/*.h) \
            include/caravel/version.h
CLI_CFLAGS = -std=c11 -O2 -Wall -Wextra $(if $(WERROR),-Werror) \
             -Icli -Icli/util $(CFLAGS)

SAN         ?= -fsanitize=address,undefined -fno-sanitize-recover=all \
               -fno-omit-frame-pointer
TEST_COMMON := tests/host/common
TEST_BINS   := $(patsubst tests/host/%.c,$(BUILD)/tests/%,$(wildcard tests/host/*.c))
TEST_DEPS   := $(wildcard $(TEST_COMMON)/*) $(wildcard include/*.h include/caravel/*.h)
TEST_CFLAGS  = -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-unused-function \
               -Iinclude -I$(TEST_COMMON) -DNO_HEAP $(SAN) $(CFLAGS)

.PHONY: all cli install uninstall test test-host test-compile test-examples \
        bench bench-update check format format-check platform-tools clean

all: cli

cli: $(CLI)

$(CLI): $(CLI_SRCS) $(CLI_HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CLI_CFLAGS) -o $@ $(CLI_SRCS)

install: $(CLI)
	@mkdir -p $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(DATADIR)/include/caravel
	cp -f $(CLI) $(DESTDIR)$(PREFIX)/bin/caravel
	chmod 755 $(DESTDIR)$(PREFIX)/bin/caravel
	cp -f include/caravel.h include/bpf.ld include/sbpfv3.ld \
	      $(DESTDIR)$(DATADIR)/include/
	cp -f include/caravel/*.h $(DESTDIR)$(DATADIR)/include/caravel/
	@echo "Installed caravel to $(DESTDIR)$(PREFIX)/bin"
	@echo "Installed SDK headers to $(DESTDIR)$(DATADIR)/include"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/caravel
	rm -rf $(DESTDIR)$(DATADIR)
	@echo "Uninstalled caravel and SDK headers"

test: test-host test-compile

test-host: $(TEST_BINS)
	@fail=0; for t in $(abspath $(TEST_BINS)); do $$t || fail=1; done; \
	if [ $$fail -ne 0 ]; then echo "host tests FAILED"; exit 1; fi; \
	echo "host tests passed"

$(BUILD)/tests/%: tests/host/%.c $(TEST_DEPS)
	@mkdir -p $(BUILD)/tests
	$(CC) $(TEST_CFLAGS) -o $@ $< $(TEST_COMMON)/stubs.c

test-compile:
	@tests/compile/matrix.sh

test-examples: $(CLI)
	@fail=0; for ex in $(EXAMPLES); do \
	    echo "== $$ex ($(SBF_VER))"; \
	    (cd examples/$$ex && CARAVEL_INCLUDE=$(CURDIR)/include \
	        $(CURDIR)/$(CLI) test --sbf-ver=$(SBF_VER)) || fail=1; \
	done; exit $$fail

bench: $(CLI)
	python3 etc/scripts/bench.py --caravel $(CLI) $(BENCH_ARGS)

bench-update: $(CLI)
	python3 etc/scripts/bench.py --caravel $(CLI) --update

check:
	$(MAKE) format-check
	$(MAKE) cli WERROR=1
	$(MAKE) test
	$(MAKE) bench

format:
	@$(CLANG_FORMAT) -i $(FORMAT_SRCS)

format-check:
	@$(CLANG_FORMAT) --version | grep -q " $(CLANG_FORMAT_VERSION)" || \
	    echo "warning: CI uses clang-format $(CLANG_FORMAT_VERSION); output may differ" >&2
	@$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_SRCS) && echo "formatting ok"

platform-tools:
	etc/scripts/install-platform-tools.sh $(PLATFORM_TOOLS)

clean:
	rm -rf $(BUILD)

# `make -s print-VAR` echoes a variable (CI reads CLANG_FORMAT_VERSION this way)
print-%:
	@echo $($*)
