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
#     dist/         — engine package (embedded in engine[.exe] as a zip)
#       includes/   — public headers
#       lib/        — debug + release archives
#       make/       — engine.mk
#       tools/      — CUI precompiler
#
# ── engine tool (make engine-tool) ───────────────────────────────────────────
#   dist/engine-<platform>/engine[.exe]  — scaffolding tool (engine init/update)

SHELL := /bin/sh

# ── Overridable by the root Makefile ─────────────────────────────────────────
BUILD_TYPE ?= release
CXX        ?= g++
CC         ?= gcc
AR         ?= ar
CARGO      ?= cargo
VCPKG      ?= vcpkg

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

# ── Rust tools workspace — variables only, rules come after main targets ───────
TOOLS_WORKSPACE   := tools/Cargo.toml
TOOLS_RS_SRCS     := $(shell find tools/cui-rs/src tools/engine-init-rs/src -type f -name "*.rs" 2>/dev/null)

ifeq ($(DETECTED_OS),Windows)
    CUI_EXE         := tools/target/release/cui.exe
    ENGINE_INIT_EXE := tools/target/release/engine.exe
else
    CUI_EXE         := tools/target/release/cui
    ENGINE_INIT_EXE := tools/target/release/engine
endif

_CUI_INSTALLED := tools/$(notdir $(CUI_EXE))

# ── Distribution paths ─────────────────────────────────────────────────────────
# Layout:
#   dist/engine-<platform>/
#     engine[.exe]   ← scaffolding tool
#     dist/          ← engine package (headers, libs, tools) — embedded in tool
DIST_DIR      := dist/engine-$(ENGINE_PLATFORM)
DIST_PKG_DIR  := $(DIST_DIR)/dist
DIST_ZIP      := dist/engine-$(ENGINE_PLATFORM).zip

ifeq ($(DETECTED_OS),Windows)
    ENGINE_TOOL_EXE := $(DIST_DIR)/engine.exe
else
    ENGINE_TOOL_EXE := $(DIST_DIR)/engine
endif

# ── Main targets ──────────────────────────────────────────────────────────────
.PHONY: all debug release dev dist engine-tool install-demo clean fclean

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

# ── Rust workspace build rules ────────────────────────────────────────────────
# cui is built as part of the normal engine build (engine-tool is separate to
# avoid circular deps: engine-tool embeds the dist zip which requires dist).

$(CUI_EXE): $(TOOLS_WORKSPACE) $(TOOLS_RS_SRCS)
	$(CARGO) build --release --manifest-path $(TOOLS_WORKSPACE) --bin cui

$(_CUI_INSTALLED): $(CUI_EXE)
	cp "$(CUI_EXE)" "$@"
	@echo "Installed CUI precompiler: $@"

.PHONY: cui-tool
cui-tool: $(_CUI_INSTALLED)

# ── Distribution package ──────────────────────────────────────────────────────
# Builds debug+release archives, packages headers, engine.mk, and CUI binary
# into $(DIST_PKG_DIR)/. The engine tool binary goes at $(DIST_DIR)/engine[.exe].

dist: $(_CUI_INSTALLED)
	@$(MAKE) -f Libraries/Makefile BUILD_TYPE=debug
	@$(MAKE) -f Libraries/Makefile BUILD_TYPE=release
	@rm -rf "$(DIST_DIR)"
	@mkdir -p "$(DIST_PKG_DIR)/lib/debug" "$(DIST_PKG_DIR)/lib/release" \
	          "$(DIST_PKG_DIR)/make" "$(DIST_PKG_DIR)/tools"
	@cp -r $(INCLUDES_BASE)            "$(DIST_PKG_DIR)/includes"
	@cp lib/debug/*.a                  "$(DIST_PKG_DIR)/lib/debug/"
	@cp lib/release/*.a                "$(DIST_PKG_DIR)/lib/release/"
	@cp engine-sdk/engine.mk           "$(DIST_PKG_DIR)/make/"
	@cp "$(_CUI_INSTALLED)"            "$(DIST_PKG_DIR)/tools/"
	@cp Libraries/vcpkg.json           "$(DIST_PKG_DIR)/vcpkg.json"
	@cp engine-sdk/dist.gitignore      "$(DIST_PKG_DIR)/.gitignore"
	@cp engine-sdk/dist-README.md      "$(DIST_PKG_DIR)/README.md"
	@cp -r "$(VCPKG_INSTALLED_ROOT)/." "$(DIST_PKG_DIR)/vcpkg_installed/"
	@echo "Engine package ready: $(DIST_PKG_DIR)/"

# ── engine tool binary (embeds the dist pkg as a zip) ─────────────────────────
# The zip contains dist/ as its top-level so strip_top_dir() in Rust works correctly.
# 'zip' is available natively on Linux/macOS and via MSYS2/MinGW on Windows.
_DO_ZIP = cd "$(DIST_DIR)" && zip -r "$(abspath $(DIST_ZIP))" "dist/"

# Sentinel tracks freshness of the engine package without globbing every file.
_DIST_SENTINEL := $(DIST_PKG_DIR)/.gitignore

$(DIST_ZIP): $(_DIST_SENTINEL)
	$(_DO_ZIP)
	@echo "Zipped: $(DIST_ZIP)"

# engine tool embeds the zip at compile time via build.rs / include_bytes!
$(ENGINE_INIT_EXE): $(TOOLS_WORKSPACE) $(TOOLS_RS_SRCS) $(DIST_ZIP)
	ENGINE_PAYLOAD_ZIP="$(abspath $(DIST_ZIP))" \
	    $(CARGO) build --release --manifest-path $(TOOLS_WORKSPACE) --bin engine
	@echo "Built: $(ENGINE_INIT_EXE)"

$(ENGINE_TOOL_EXE): $(ENGINE_INIT_EXE)
	@mkdir -p "$(DIST_DIR)"
	cp "$(ENGINE_INIT_EXE)" "$@"
	@echo "Installed engine tool: $@"

.PHONY: engine-tool
engine-tool: dist $(ENGINE_TOOL_EXE)
	@echo "engine tool ready: $(ENGINE_TOOL_EXE)"

# ── Sync demo ─────────────────────────────────────────────────────────────────
# Copies the current BUILD_TYPE artifacts directly into demo/engine/.
# Faster than install-demo: no dist packaging step required.
# vcpkg_installed/ is linked (not copied) to avoid duplicating hundreds of MB.
.PHONY: sync-demo
sync-demo: $(ENGINE_LIBS) $(_CUI_INSTALLED)
	@mkdir -p demo/engine/lib/$(BUILD_TYPE) demo/engine/make demo/engine/tools
	@rm -rf demo/engine/includes && cp -r $(INCLUDES_BASE) demo/engine/includes
	@cp $(ENGINE_LIBS) demo/engine/lib/$(BUILD_TYPE)/
	@cp engine-sdk/engine.mk demo/engine/make/
	@cp "$(_CUI_INSTALLED)" demo/engine/tools/
	@if [ ! -e demo/engine/vcpkg_installed ]; then \
	    if [ "$(DETECTED_OS)" = "Windows" ]; then \
	        TARGET=$$(cd "$(VCPKG_INSTALLED_ROOT)" && pwd -W 2>/dev/null || echo "$(VCPKG_INSTALLED_ROOT)"); \
	        MSYS_NO_PATHCONV=1 cmd /c "mklink /J \"demo\\engine\\vcpkg_installed\" \"$$TARGET\"" 2>/dev/null \
	            || ln -sfn "../../$(VCPKG_INSTALLED_ROOT)" demo/engine/vcpkg_installed; \
	    else \
	        ln -sfn "../../$(VCPKG_INSTALLED_ROOT)" demo/engine/vcpkg_installed; \
	    fi; \
	fi
	@echo "Synced [$(BUILD_TYPE)] → demo/engine/"

# ── Install demo ──────────────────────────────────────────────────────────────
# Full dist-based install: packages headers, both build types, and tools.
install-demo: dist
	@rm -rf demo/engine
	@cp -r "$(DIST_PKG_DIR)" demo/engine
	@echo "Engine installed to demo/engine/"

# ── vcpkg dependency management ───────────────────────────────────────────────
# install:   no args → install all Libraries/vcpkg.json deps (incremental)
#            with args → add packages, install them, print detected linker flags
# search:    search the vcpkg catalog
#
# Usage:
#   make -f Libraries/Makefile install
#   make -f Libraries/Makefile install fmt
#   make -f Libraries/Makefile search json

ifeq ($(firstword $(MAKECMDGOALS)),install)
  _INSTALL_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  ifneq ($(_INSTALL_ARGS),)
    $(foreach _d,$(_INSTALL_ARGS),$(eval $(_d):;@true))
  endif
endif
ifeq ($(firstword $(MAKECMDGOALS)),search)
  _Q_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  ifneq ($(_Q_ARGS),)
    Q ?= $(_Q_ARGS)
    $(foreach _q,$(_Q_ARGS),$(eval $(_q):;@true))
  endif
endif

.PHONY: install install-deps search

install install-deps:
	@if [ -z "$(_INSTALL_ARGS)" ]; then \
	    $(VCPKG) install \
	        --triplet=$(VCPKG_TRIPLET) \
	        --x-manifest-root=Libraries \
	        --x-install-root=$(VCPKG_INSTALLED_ROOT) \
	        --no-print-usage; \
	else \
	    [ -f Libraries/vcpkg.json ] || printf '{\n  "name": "engine",\n  "version": "0.1.0",\n  "dependencies": []\n}\n' > Libraries/vcpkg.json; \
	    LIBDIR="$(VCPKG_INSTALLED_DIR)/lib"; \
	    for dep in $(_INSTALL_ARGS); do \
	        if command -v jq >/dev/null 2>&1; then \
	            if jq --arg d "$$dep" -e '.dependencies[] | select((type=="string" and .==$d) or (.name?==$d))' Libraries/vcpkg.json >/dev/null 2>&1; then \
	                printf "  [vcpkg.json] %s already listed\n" "$$dep"; \
	            else \
	                tmp=$$(mktemp); jq --arg d "$$dep" '.dependencies += [$$d]' Libraries/vcpkg.json > "$$tmp" && mv "$$tmp" Libraries/vcpkg.json; \
	                printf "  [vcpkg.json] Added %s\n" "$$dep"; \
	            fi; \
	        elif command -v python3 >/dev/null 2>&1; then \
	            DEP="$$dep" python3 -c "import json,os; dep=os.environ['DEP']; d=json.load(open('Libraries/vcpkg.json')); deps=d.setdefault('dependencies',[]); n=[x if isinstance(x,str) else x.get('name','') for x in deps]; deps.append(dep) if dep not in n else None; open('Libraries/vcpkg.json','w').write(json.dumps(d,indent=2))"; \
	            printf "  [vcpkg.json] Updated (%s)\n" "$$dep"; \
	        else \
	            printf "Error: jq or python3 required to edit Libraries/vcpkg.json\n"; exit 1; \
	        fi; \
	        b=$$(mktemp); a=$$(mktemp); \
	        ls "$$LIBDIR/"*.a 2>/dev/null | sort > "$$b" || :; \
	        $(VCPKG) install "$$dep" \
	            --triplet=$(VCPKG_TRIPLET) \
	            --x-manifest-root=Libraries \
	            --x-install-root=$(VCPKG_INSTALLED_ROOT) \
	            --no-print-usage; \
	        ls "$$LIBDIR/"*.a 2>/dev/null | sort > "$$a" || :; \
	        if [ -n "$(LIBS)" ]; then \
	            NEW="$(LIBS)"; \
	        else \
	            NEW=$$(comm -13 "$$b" "$$a" \
	                | sed 's|.*/lib\([^.]*\)\.a|-l\1|' \
	                | tr '\n' ' ' | sed 's/[[:space:]]*$$//'); \
	        fi; \
	        rm -f "$$b" "$$a"; \
	        if [ -z "$$NEW" ]; then \
	            printf "  %s is header-only — no linker flags needed\n" "$$dep"; \
	        else \
	            printf "  Detected: %s\n" "$$NEW"; \
	            printf "  → Add to engine-sdk/engine.mk _ENG_VCPKG_LIBS: %s\n" "$$NEW"; \
	        fi; \
	    done; \
	fi

search:
	@[ -n "$(Q)" ] || { printf "Usage: make -f Libraries/Makefile search <keyword>\n"; exit 1; }
	$(VCPKG) search "$(Q)"

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	@rm -f $(ALL_ENGINE_OBJS) $(ALL_ENGINE_OBJS:.o=.d)
	@echo "Engine objects cleaned [$(BUILD_TYPE)]"

fclean: clean
	@rm -f $(ENGINE_LIBS)
	@echo "Engine archives removed [$(BUILD_TYPE)]"

# ── Dependency tracking ───────────────────────────────────────────────────────
-include $(wildcard $(ALL_ENGINE_OBJS:.o=.d))
