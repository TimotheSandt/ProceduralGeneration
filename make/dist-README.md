# C++ Game Engine — Release Package

This directory is a self-contained engine release. It contains everything needed
to build a game project against this engine without having to compile the engine itself.

## Contents

```
engine-<platform>/
  includes/          — Public C++ headers (Graphics, UI, Noise, Profiler, …)
  lib/
    debug/           — Static archives built with debug flags
    release/         — Static archives built with optimisations
  make/
    engine.mk        — Include this in your project Makefile
  tools/
    cui[.exe]        — CUI precompiler (converts .cui DSL files to C++)
  .gitignore         — Ignores everything here (all files are generated)
  README.md          — This file
```

## Prerequisites

| Tool | Minimum version | Notes |
|------|----------------|-------|
| GNU Make | 4.x | `make --version` |
| g++ or clang++ | C++23 support | set `CXX` to override |
| vcpkg | any | only needed for `make install` |

No Python, no Rust toolchain required. The CUI precompiler ships as a native binary.

## Quick start

### 1 — Point your Makefile at the engine

```makefile
ENGINE_DIR   := path/to/engine-<platform>   # this directory
GAME_SRC_DIR := src
BUILD_TYPE   ?= release                      # debug | dev | release
include $(ENGINE_DIR)/make/engine.mk
```

### 2 — Build

```sh
make release    # optimised build  →  bin/release/<name>
make debug      # debug build      →  bin/debug/main
make run        # build + run
```

## Complete Makefile example

Save this as `Makefile` at the root of your game project:

```makefile
SHELL := /bin/sh
-include config.mk          # optional: PROJECT_NAME, VERSION, …

CXX ?= g++
CC  ?= gcc

ENGINE_DIR   := path/to/engine-<platform>
GAME_SRC_DIR := src
BUILD_TYPE   ?= release

ifneq ($(findstring debug,$(MAKECMDGOALS)),)
    BUILD_TYPE := debug
else ifneq ($(findstring dev,$(MAKECMDGOALS)),)
    BUILD_TYPE := dev
endif

include $(ENGINE_DIR)/make/engine.mk

SRC_DIR := src
OBJ_DIR := obj/$(BUILD_TYPE)
BIN_DIR := bin/$(BUILD_TYPE)

PROJECT_NAME ?= mygame
TARGET := $(BIN_DIR)/$(PROJECT_NAME)$(ENGINE_EXE_EXT)

SRCS_CPP := $(shell find $(SRC_DIR) -name "*.cpp")
OBJS_CPP := $(SRCS_CPP:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/src/%.o)
GEN_OBJS := $(GEN_CPP_SOURCES:$(ENGINE_GEN_DIR)/%.gen.cpp=$(OBJ_DIR)/gen/%.o)
ALL_OBJS := $(OBJS_CPP) $(GEN_OBJS)

.PHONY: all debug dev release run clean fclean

all debug dev release: $(TARGET)

run: $(TARGET)
	@./$(TARGET)

$(TARGET): $(ALL_OBJS) $(ENGINE_LIBS) | $(BIN_DIR)
	$(CXX) $(ENGINE_CXXFLAGS) $(ALL_OBJS) $(ENGINE_LINK_LIBS) $(ENGINE_LDFLAGS) -o $@
	@echo "Built: $@"

$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(ENGINE_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -c $< -o $@

$(OBJ_DIR)/gen/%.o: $(ENGINE_GEN_DIR)/%.gen.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(ENGINE_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -c $< -o $@

# Generated sources must exist before any object is compiled
$(ALL_OBJS): | $(GEN_CPP_SOURCES) $(GEN_REDIRECT_SOURCES)

$(BIN_DIR):
	@mkdir -p "$@"

clean:
	@rm -rf obj/ Generated/ bin/

fclean: clean

-include $(wildcard $(ALL_OBJS:.o=.d))
```

## CUI precompiler

`.cui` files are a declarative DSL for describing UI views. The precompiler
generates `.gen.cpp` / `.gen.h` pairs consumed by the C++ build.

`engine.mk` handles CUI generation automatically — any `.cui` file found under
`GAME_SRC_DIR` is compiled before the first C++ object is built.

You never call `tools/cui` directly; Make handles it.

## Integrating with a team repository

The files in this directory are **compiled artifacts** — do not commit them to
version control. The bundled `.gitignore` already ignores everything here.

Recommended workflows:

- **GitHub Releases** (recommended): CI publishes each engine release as an
  artifact. Each developer downloads and unpacks it next to their game project,
  then sets `ENGINE_DIR` accordingly. Nothing engine-related lives inside the
  game repository.

- **Submodule**: reference the engine source repository as a submodule and run
  `make -f Libraries/Makefile dist` locally. Keep the `dist/` output gitignored.
