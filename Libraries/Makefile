# Libraries/Makefile — Builds the engine subsystem archives.
#
# ── Standalone usage (from project root) ─────────────────────────────────────
#   make -f Libraries/Makefile debug
#   make -f Libraries/Makefile release
#   make -f Libraries/Makefile BUILD_TYPE=debug lib/debug/libui.a   # single archive
#   make -f Libraries/Makefile dist                                  # package for distribution
#
# ── Variables passed by the root game Makefile ───────────────────────────────
#   BUILD_TYPE, CXX, CC, AR, CXXFLAGS, CFLAGS, DEPFLAGS, INCLUDES,
#   VCPKG_INSTALLED_ROOT, VCPKG_TRIPLET
#
# ── Output ────────────────────────────────────────────────────────────────────
#   lib/<build>/libgraphics.a    lib/<build>/libui.a
#   lib/<build>/libnoise.a       lib/<build>/libprofiler.a
#   lib/<build>/libutilities.a   lib/<build>/libthirdparty.a
#
# ── Distribution (make dist) ─────────────────────────────────────────────────
#   dist/engine-<platform>/
#     includes/   — public headers
#     lib/        — debug + release archives
#     make/       — engine.mk
#     tools/      — CUI precompiler

SHELL := /bin/sh

# ── Overridable by the root Makefile ─────────────────────────────────────────
BUILD_TYPE ?= release
CXX        ?= g++
CC         ?= gcc
AR         ?= ar
CARGO      ?= cargo

# ── Platform detection ────────────────────────────────────────────────────────
UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
else ifneq ($(filter Linux,$(UNAME_S)),)
    DETECTED_OS := Linux
else ifneq ($(filter Darwin,$(UNAME_S)),)
    DETECTED_OS := Darwin
else ifneq ($(filter MSYS% MINGW% CYGWIN%,$(UNAME_S)),)
    DETECTED_OS := Windows
else
    DETECTED_OS := Unknown
endif

ifeq ($(DETECTED_OS),Windows)
    VCPKG_TRIPLET    ?= x64-mingw-static
    ARCH_FLAG        ?= -m64
    PLATFORM_DEFINES ?= -DNOMINMAX -DWIN32_LEAN_AND_MEAN
    ENGINE_PLATFORM  := windows-x64

else ifeq ($(DETECTED_OS),Darwin)
    _ARCH := $(shell uname -m)
    ifeq ($(_ARCH),arm64)
        VCPKG_TRIPLET   ?= arm64-osx
        ARCH_FLAG       ?=
        ENGINE_PLATFORM := macos-arm64
    else
        VCPKG_TRIPLET   ?= x64-osx
        ARCH_FLAG       ?= -m64
        ENGINE_PLATFORM := macos-x64
    endif
    PLATFORM_DEFINES ?=

else
    _ARCH := $(shell uname -m 2>/dev/null)
    ifeq ($(_ARCH),aarch64)
        VCPKG_TRIPLET   ?= arm64-linux
        ARCH_FLAG       ?=
        ENGINE_PLATFORM := linux-arm64
    else
        VCPKG_TRIPLET   ?= x64-linux
        ARCH_FLAG       ?= -m64
        ENGINE_PLATFORM := linux-x64
    endif
    PLATFORM_DEFINES ?=
endif

VCPKG_INSTALLED_ROOT ?= ./vcpkg_installed
VCPKG_INSTALLED_DIR  := $(VCPKG_INSTALLED_ROOT)/$(VCPKG_TRIPLET)

# ── Compiler flags (root Makefile overrides all) ──────────────────────────────
DEPFLAGS ?= -MMD -MP

ifeq ($(BUILD_TYPE),debug)
    CFLAGS   ?= -Wall -Wextra $(ARCH_FLAG) -O1 -g -DDEBUG $(PLATFORM_DEFINES) -DGLM_ENABLE_EXPERIMENTAL
    CXXFLAGS ?= -std=c++23 $(CFLAGS)
else ifeq ($(BUILD_TYPE),dev)
    CFLAGS   ?= $(ARCH_FLAG) -g3 -O0 -DDEBUG $(PLATFORM_DEFINES) -DGLM_ENABLE_EXPERIMENTAL
    CXXFLAGS ?= -std=c++23 $(CFLAGS)
else
    CFLAGS   ?= -Wall -Wextra $(ARCH_FLAG) -O3 -DNDEBUG $(PLATFORM_DEFINES) -DGLM_ENABLE_EXPERIMENTAL
    CXXFLAGS ?= -std=c++23 $(CFLAGS)
endif

# ── Paths ─────────────────────────────────────────────────────────────────────
LIBRARIES_SRC_DIR := Libraries/src
INCLUDES_BASE     := Libraries/includes
OBJ_DIR           := obj
LIB_DIR           := lib

OBJ_DIR_TYPE := $(OBJ_DIR)/$(BUILD_TYPE)

INCLUDES_SUBDIRS := $(notdir $(wildcard $(INCLUDES_BASE)/*))
INCLUDES ?= \
    -I$(INCLUDES_BASE) \
    $(foreach d,$(INCLUDES_SUBDIRS),-I$(INCLUDES_BASE)/$(d)) \
    -I$(VCPKG_INSTALLED_DIR)/include

# ── Subsystem archives ────────────────────────────────────────────────────────
LIB_GRAPHICS   := $(LIB_DIR)/$(BUILD_TYPE)/libgraphics.a
LIB_UI         := $(LIB_DIR)/$(BUILD_TYPE)/libui.a
LIB_NOISE      := $(LIB_DIR)/$(BUILD_TYPE)/libnoise.a
LIB_PROFILER   := $(LIB_DIR)/$(BUILD_TYPE)/libprofiler.a
LIB_UTILITIES  := $(LIB_DIR)/$(BUILD_TYPE)/libutilities.a
LIB_THIRDPARTY := $(LIB_DIR)/$(BUILD_TYPE)/libthirdparty.a

ENGINE_LIBS := $(LIB_GRAPHICS) $(LIB_UI) $(LIB_NOISE) $(LIB_PROFILER) $(LIB_UTILITIES) $(LIB_THIRDPARTY)

# ── Per-subsystem sources ─────────────────────────────────────────────────────
Graphics_SRCS   := $(shell find $(LIBRARIES_SRC_DIR)/Graphics   -type f \( -name "*.cpp" -o -name "*.c" \) 2>/dev/null)
UI_SRCS         := $(shell find $(LIBRARIES_SRC_DIR)/UI         -type f \( -name "*.cpp" -o -name "*.c" \) 2>/dev/null)
Noise_SRCS      := $(shell find $(LIBRARIES_SRC_DIR)/Noise      -type f \( -name "*.cpp" -o -name "*.c" \) 2>/dev/null)
Profiler_SRCS   := $(shell find $(LIBRARIES_SRC_DIR)/Profiler   -type f \( -name "*.cpp" -o -name "*.c" \) 2>/dev/null)
Utilities_SRCS  := $(shell find $(LIBRARIES_SRC_DIR)/Utilities  -type f \( -name "*.cpp" -o -name "*.c" \) 2>/dev/null)
ThirdParty_SRCS := $(shell find $(LIBRARIES_SRC_DIR)/ThirdParty -type f \( -name "*.cpp" -o -name "*.c" \) 2>/dev/null)

obj_from_src = \
    $(patsubst $(LIBRARIES_SRC_DIR)/%.cpp,$(OBJ_DIR_TYPE)/Libraries/%.o,$(filter %.cpp,$(1))) \
    $(patsubst $(LIBRARIES_SRC_DIR)/%.c,$(OBJ_DIR_TYPE)/Libraries/%.o,$(filter %.c,$(1)))

Graphics_OBJS   := $(call obj_from_src,$(Graphics_SRCS))
UI_OBJS         := $(call obj_from_src,$(UI_SRCS))
Noise_OBJS      := $(call obj_from_src,$(Noise_SRCS))
Profiler_OBJS   := $(call obj_from_src,$(Profiler_SRCS))
Utilities_OBJS  := $(call obj_from_src,$(Utilities_SRCS))
ThirdParty_OBJS := $(call obj_from_src,$(ThirdParty_SRCS))

ALL_ENGINE_OBJS := $(Graphics_OBJS) $(UI_OBJS) $(Noise_OBJS) $(Profiler_OBJS) $(Utilities_OBJS) $(ThirdParty_OBJS)

# ── CUI precompiler (Rust) — variables only, rules come after main targets ────
CUI_RS_DIR      := tools/cui-rs
CUI_RS_MANIFEST := $(CUI_RS_DIR)/Cargo.toml
CUI_RS_SRCS     := $(shell find $(CUI_RS_DIR)/src -type f -name "*.rs" 2>/dev/null)

ifeq ($(DETECTED_OS),Windows)
    CUI_EXE := $(CUI_RS_DIR)/target/release/cui.exe
else
    CUI_EXE := $(CUI_RS_DIR)/target/release/cui
endif

_CUI_INSTALLED := tools/$(notdir $(CUI_EXE))

# ── Main targets ──────────────────────────────────────────────────────────────
.PHONY: all debug release dev dist clean fclean

all debug release dev: $(ENGINE_LIBS) $(_CUI_INSTALLED)
	@echo "Engine [$(BUILD_TYPE)] — all archives up to date"

# ── Archive rules ─────────────────────────────────────────────────────────────
$(LIB_GRAPHICS): $(Graphics_OBJS)
	@mkdir -p "$(dir $@)"
	$(AR) rcs $@ $^
	@echo "Archived [Graphics]    $(BUILD_TYPE): $@"

$(LIB_UI): $(UI_OBJS)
	@mkdir -p "$(dir $@)"
	$(AR) rcs $@ $^
	@echo "Archived [UI]          $(BUILD_TYPE): $@"

$(LIB_NOISE): $(Noise_OBJS)
	@mkdir -p "$(dir $@)"
	$(AR) rcs $@ $^
	@echo "Archived [Noise]       $(BUILD_TYPE): $@"

$(LIB_PROFILER): $(Profiler_OBJS)
	@mkdir -p "$(dir $@)"
	$(AR) rcs $@ $^
	@echo "Archived [Profiler]    $(BUILD_TYPE): $@"

$(LIB_UTILITIES): $(Utilities_OBJS)
	@mkdir -p "$(dir $@)"
	$(AR) rcs $@ $^
	@echo "Archived [Utilities]   $(BUILD_TYPE): $@"

$(LIB_THIRDPARTY): $(ThirdParty_OBJS)
	@mkdir -p "$(dir $@)"
	$(AR) rcs $@ $^
	@echo "Archived [ThirdParty]  $(BUILD_TYPE): $@"

# ── Compilation rules ─────────────────────────────────────────────────────────
$(OBJ_DIR_TYPE)/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@
	@echo "  CXX [$(BUILD_TYPE)] $<"

$(OBJ_DIR_TYPE)/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@
	@echo "  CC  [$(BUILD_TYPE)] $<"

# ── CUI build + install rules ─────────────────────────────────────────────────
$(CUI_EXE): $(CUI_RS_MANIFEST) $(CUI_RS_SRCS)
	$(CARGO) build --release --manifest-path $(CUI_RS_MANIFEST)

$(_CUI_INSTALLED): $(CUI_EXE)
	cp "$(CUI_EXE)" "$@"
	@echo "Installed CUI precompiler: $@"

.PHONY: cui-tool
cui-tool: $(_CUI_INSTALLED)

# ── Distribution package ──────────────────────────────────────────────────────
# Builds debug + release archives and packages everything a game project needs:
#   dist/engine-<platform>/includes/   public headers
#   dist/engine-<platform>/lib/        debug + release archives
#   dist/engine-<platform>/make/       engine.mk
#   dist/engine-<platform>/tools/      CUI precompiler binary

DIST_DIR := dist/engine-$(ENGINE_PLATFORM)

dist: $(_CUI_INSTALLED)
	@$(MAKE) -f Libraries/Makefile BUILD_TYPE=debug
	@$(MAKE) -f Libraries/Makefile BUILD_TYPE=release
	@rm -rf "$(DIST_DIR)"
	@mkdir -p "$(DIST_DIR)/lib/debug" "$(DIST_DIR)/lib/release" \
	          "$(DIST_DIR)/make" "$(DIST_DIR)/tools"
	@cp -r $(INCLUDES_BASE) "$(DIST_DIR)/includes"
	@cp lib/debug/*.a    "$(DIST_DIR)/lib/debug/"
	@cp lib/release/*.a  "$(DIST_DIR)/lib/release/"
	@cp make/engine.mk   "$(DIST_DIR)/make/"
	@cp "$(_CUI_INSTALLED)" "$(DIST_DIR)/tools/"
	@cp make/dist.gitignore "$(DIST_DIR)/.gitignore"
	@cp make/dist-README.md "$(DIST_DIR)/README.md"
	@echo ""
	@echo "Distribution ready: $(DIST_DIR)/"
	@echo "  $(DIST_DIR)/includes/      public headers"
	@echo "  $(DIST_DIR)/lib/debug/     debug archives"
	@echo "  $(DIST_DIR)/lib/release/   release archives"
	@echo "  $(DIST_DIR)/make/          engine.mk"
	@echo "  $(DIST_DIR)/tools/         CUI precompiler (Rust binary)"

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	@rm -f $(ALL_ENGINE_OBJS) $(ALL_ENGINE_OBJS:.o=.d)
	@echo "Engine objects cleaned [$(BUILD_TYPE)]"

fclean: clean
	@rm -f $(ENGINE_LIBS)
	@echo "Engine archives removed [$(BUILD_TYPE)]"

# ── Dependency tracking ───────────────────────────────────────────────────────
-include $(wildcard $(ALL_ENGINE_OBJS:.o=.d))
