# Makefile for tobin_c -- a plain-make alternative to CMakeLists.txt.
# Same compiler flags, same libraries, same output path (build/tobin_c) as
# the CMake build, so existing run/deploy instructions work either way.
#
# Requires: gcc/clang, pkg-config, and the MariaDB C client dev package
# (libmariadb-dev on Debian/Ubuntu, mariadb-connector-c-devel on Fedora).
# Linux only -- see README.md.
#
#   make            # debug build (default)
#   make release    # optimized build
#   make clean

SHELL    := /bin/bash

CC       ?= cc
STD      := -std=gnu11
WARN     := -Wall -Wextra
MARIADB_CFLAGS := $(shell pkg-config --cflags libmariadb)
MARIADB_LIBS   := $(shell pkg-config --libs libmariadb)

BOLD_GREEN  := \033[1;32m
BOLD_RED    := \033[1;31m
BOLD_CYAN   := \033[1;36m
BOLD_WHITE := \033[1;37m
COLOR_OFF  := \033[0m

CFLAGS   ?= -g -O0
ALL_CFLAGS = $(CFLAGS) $(STD) $(WARN) -Iinclude $(MARIADB_CFLAGS)
LDLIBS   := $(MARIADB_LIBS) -lcrypt -lm

BIN      := build/tobin_c
SRCS     := $(shell find src -name '*.c')
OBJS     := $(patsubst src/%.c,build/obj/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)

.PHONY: all release clean

all: $(BIN)

release: CFLAGS := -O2
release: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(dir $@)
	@echo -e "$(BOLD_GREEN)  Linking:$(BOLD_WHITE) $@$(COLOR_OFF)"
	@$(CC) $(OBJS) -o $@ $(LDLIBS)

build/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	@echo -e "$(BOLD_RED)  Compiling:$(BOLD_CYAN) $<$(COLOR_OFF)"
	@$(CC) $(ALL_CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	rm -rf build
