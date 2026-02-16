# NES Emulator (SDL3) - Makefile
# Usage:
#   make              -> release build
#   make debug        -> debug build
#   make run ROM=path/to/game.nes
#   make clean

SHELL := /usr/bin/env bash

APP_NAME := nes_emu
BUILD_DIR := build
SRC_DIR := src
INC_DIR := include

CC := cc

# --- SDL3 via pkg-config (recommended) ---
PKG_CONFIG ?= pkg-config
SDL_PKGS := sdl3

SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(SDL_PKGS) 2>/dev/null)
SDL_LIBS   := $(shell $(PKG_CONFIG) --libs   $(SDL_PKGS) 2>/dev/null)

# If pkg-config can't find SDL3, you can override like:
#   make SDL_CFLAGS="-I/usr/local/include" SDL_LIBS="-L/usr/local/lib -lSDL3"

# --- Common flags ---
CSTD := -std=c11
WARN := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter
INCS := -I$(INC_DIR)

CFLAGS_COMMON := $(CSTD) $(WARN) $(INCS) $(SDL_CFLAGS)
LDFLAGS_COMMON :=
LDLIBS_COMMON := $(SDL_LIBS) -lm

# --- Build type flags ---
CFLAGS_RELEASE := -O2 -DNDEBUG
CFLAGS_DEBUG   := -O0 -g3 -DDEBUG

# Default build type
CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_RELEASE)
LDFLAGS := $(LDFLAGS_COMMON)
LDLIBS := $(LDLIBS_COMMON)

# --- Source discovery ---
SRCS := $(shell find $(SRC_DIR) -type f -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/obj/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

BIN := $(BUILD_DIR)/$(APP_NAME)

.PHONY: all debug release clean run print-vars

all: release

release: CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_RELEASE)
release: $(BIN)

debug: CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_DEBUG)
debug: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

# Compile .c -> .o with dependency file
$(BUILD_DIR)/obj/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

run: $(BIN)
	@if [[ -z "$${ROM:-}" ]]; then \
		echo "Usage: make run ROM=path/to/game.nes"; \
		exit 1; \
	fi
	./$(BIN) "$${ROM}"

clean:
	rm -rf $(BUILD_DIR)

print-vars:
	@echo "CC=$(CC)"
	@echo "SDL_CFLAGS=$(SDL_CFLAGS)"
	@echo "SDL_LIBS=$(SDL_LIBS)"
	@echo "SRCS=$(words $(SRCS)) files"
