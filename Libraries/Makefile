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

SHELL := sh

# ── Engine identity (single source of truth) ──────────────────────────────────
# Path relative to wherever make is invoked (always from project root).
_MK_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
-include $(_MK_DIR)config.mk

# ── Overridable by the root Makefile ─────────────────────────────────────────
BUILD_TYPE ?= release
CXX        ?= g++
CC         ?= gcc
AR         ?= ar
CARGO      ?= cargo
VCPKG      ?= vcpkg

ifneq ($(filter debug,$(MAKECMDGOALS)),)
    BUILD_TYPE := debug
else ifneq ($(filter dev,$(MAKECMDGOALS)),)
    BUILD_TYPE := dev
else ifneq ($(filter release,$(MAKECMDGOALS)),)
    BUILD_TYPE := release
endif

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

# ── Git remote detection ──────────────────────────────────────────────────────
# Reads the 'origin' remote and extracts the host and owner/repo slug.
# Supports HTTPS (https://host/owner/repo[.git]) and SSH (git@host:owner/repo[.git]).
# Override REPO_HOST or REPO_SLUG on the command line if needed.
_GIT_REMOTE_URL := $(shell git remote get-url origin 2>/dev/null)
_GIT_REPO_HOST  := $(shell printf '%s' "$(_GIT_REMOTE_URL)" | \
    sed -E 's|https://([^/]+)/.*|\1|; t; s|git@([^:]+):.*|\1|' 2>/dev/null)
_GIT_REPO_SLUG  := $(shell printf '%s' "$(_GIT_REMOTE_URL)" | \
    sed -E 's|https://[^/]+/||; s|git@[^:]+:||; s|\.git$$||' 2>/dev/null)
REPO_HOST ?= $(_GIT_REPO_HOST)
REPO_SLUG ?= $(_GIT_REPO_SLUG)

# ── Rust tools workspace — variables only, rules come after main targets ───────
TOOLS_WORKSPACE   := tools/Cargo.toml
TOOLS_BUILD_RS    := $(wildcard tools/cui-rs/build.rs tools/engine-init-rs/build.rs)
TOOLS_RS_SRCS     := $(shell find tools/cui-rs/src tools/engine-init-rs/src -type f -name "*.rs" 2>/dev/null) $(TOOLS_BUILD_RS)
CARGO_TARGET_DIR  ?= build/cargo-tools

ifeq ($(DETECTED_OS),Windows)
    CUI_EXE         := $(CARGO_TARGET_DIR)/release/cui.exe
    ENGINE_INIT_EXE := $(CARGO_TARGET_DIR)/release/engine.exe
else
    CUI_EXE         := $(CARGO_TARGET_DIR)/release/cui
    ENGINE_INIT_EXE := $(CARGO_TARGET_DIR)/release/engine
endif

_CUI_INSTALLED := tools/$(notdir $(CUI_EXE))

# ── Distribution paths ─────────────────────────────────────────────────────────
# Layout (GitHub release artifacts):
#   dist/engine-<platform>[.exe]      — standalone engine tool binary
#   dist/engine-pkg-<platform>.zip    — engine package (headers, libs, tools)
#
# Staging directory (build-time only, not uploaded):
#   dist/pkg-<platform>/              — package contents before zipping
_DIST_STAGING    := dist/pkg-$(ENGINE_PLATFORM)
DIST_PKG_DIR     := $(_DIST_STAGING)
ENGINE_PKG_ZIP   := dist/engine-pkg-$(ENGINE_PLATFORM).zip
ENGINE_PAYLOAD_DIR := build/engine-payload
ENGINE_PAYLOAD_ZIP := $(ENGINE_PAYLOAD_DIR)/engine-$(ENGINE_PLATFORM)-$(shell date +%s%N 2>/dev/null || date +%s).zip

ifeq ($(DETECTED_OS),Windows)
    ENGINE_TOOL_EXE := dist/engine-$(ENGINE_PLATFORM).exe
else
    ENGINE_TOOL_EXE := dist/engine-$(ENGINE_PLATFORM)
endif

# ── Main targets ──────────────────────────────────────────────────────────────
.PHONY: all debug release dev engine dist engine-tool install-demo clean fclean info sync-demo

# ── Tool check helper ─────────────────────────────────────────────────────────
# Usage: $(call _chk,binary,install hint)
.PHONY: info
info:
	@echo ""
	@echo "Engine build -- required tools"
	@echo "=============================="
	@if command -v $(CXX) >/dev/null 2>&1; then printf "  [ok] %s\n" "$(CXX)"; \
	    else printf "  [!!] %-24s -- scoop install mingw / brew install gcc / apt install g++\n" "$(CXX)"; fi
	@if command -v $(AR) >/dev/null 2>&1; then printf "  [ok] %s\n" "$(AR)"; \
	    else printf "  [!!] %-24s -- scoop install mingw / brew install binutils / apt install binutils\n" "$(AR)"; fi
	@if command -v $(VCPKG) >/dev/null 2>&1; then printf "  [ok] %s\n" "$(VCPKG)"; \
	    else printf "  [!!] %-24s -- https://vcpkg.io/en/getting-started\n" "$(VCPKG)"; fi
	@echo ""
	@echo "  make dist -- needs one of: zip / 7z / python3"
	@if command -v zip >/dev/null 2>&1; then \
	    printf "  [ok] zip\n"; \
	elif command -v 7z >/dev/null 2>&1; then \
	    printf "  [ok] 7z  (zip fallback)\n"; \
	elif command -v python3 >/dev/null 2>&1; then \
	    printf "  [ok] python3  (zip fallback)\n"; \
	else \
	    printf "  [!!] none found  -- scoop install zip / scoop install 7zip\n"; \
	fi
	@echo ""
	@echo "  make engine-tool -- optional"
	@if command -v cargo >/dev/null 2>&1; then printf "  [ok] cargo\n"; \
	    else printf "  [!!] %-24s -- https://rustup.rs\n" "cargo"; fi
	@echo ""

all debug release dev: $(ENGINE_LIBS) $(_CUI_INSTALLED)
	@echo "Engine [$(BUILD_TYPE)] — all archives up to date"

engine: install-demo
	@echo "Engine fully prepared in demo/engine/"

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
	CARGO_TARGET_DIR="$(abspath $(CARGO_TARGET_DIR))" \
	    $(CARGO) build --release --manifest-path $(TOOLS_WORKSPACE) --bin cui

$(_CUI_INSTALLED): $(CUI_EXE)
ifeq ($(DETECTED_OS),Windows)
	@powershell -NoProfile -Command "Copy-Item -Path '$(CUI_EXE)' -Destination '$@' -Force"
else
	cp "$(CUI_EXE)" "$@"
endif
	@echo "Installed CUI precompiler: $@"

.PHONY: cui-tool
cui-tool: $(_CUI_INSTALLED)

# ── Distribution package ──────────────────────────────────────────────────────
# Builds debug+release archives, packages headers, engine.mk, CUI, and vcpkg.json
# into $(DIST_PKG_DIR)/ then zips it as $(ENGINE_PKG_ZIP).

dist: $(_CUI_INSTALLED)
	@$(MAKE) -f Libraries/Makefile BUILD_TYPE=debug debug
	@$(MAKE) -f Libraries/Makefile BUILD_TYPE=release release
ifeq ($(DETECTED_OS),Windows)
	@mkdir -p "$(DIST_PKG_DIR)/includes" "$(DIST_PKG_DIR)/lib/debug" "$(DIST_PKG_DIR)/lib/release" \
	          "$(DIST_PKG_DIR)/make" "$(DIST_PKG_DIR)/tools"
	@powershell -NoProfile -Command "Copy-Item -Path '$(INCLUDES_BASE)/*' -Destination '$(DIST_PKG_DIR)/includes' -Recurse -Force"
	@powershell -NoProfile -Command "Copy-Item -Path 'lib/debug/*.a' -Destination '$(DIST_PKG_DIR)/lib/debug' -Force"
	@powershell -NoProfile -Command "Copy-Item -Path 'lib/release/*.a' -Destination '$(DIST_PKG_DIR)/lib/release' -Force"
	@powershell -NoProfile -Command "Copy-Item -Path 'engine-sdk/engine.mk' -Destination '$(DIST_PKG_DIR)/make' -Force"
	@powershell -NoProfile -Command "Copy-Item -Path '$(_CUI_INSTALLED)' -Destination '$(DIST_PKG_DIR)/tools' -Force"
	@powershell -NoProfile -Command "Copy-Item -Path 'Libraries/vcpkg.json' -Destination '$(DIST_PKG_DIR)' -Force"
	@powershell -NoProfile -Command "Copy-Item -Path 'engine-sdk/dist.gitignore' -Destination '$(DIST_PKG_DIR)/.gitignore' -Force"
	@powershell -NoProfile -Command "Copy-Item -Path 'engine-sdk/dist-README.md' -Destination '$(DIST_PKG_DIR)/README.md' -Force"
else
	@rm -rf "$(_DIST_STAGING)"
	@mkdir -p "$(DIST_PKG_DIR)/lib/debug" "$(DIST_PKG_DIR)/lib/release" \
	          "$(DIST_PKG_DIR)/make" "$(DIST_PKG_DIR)/tools"
	@cp -r $(INCLUDES_BASE)       "$(DIST_PKG_DIR)/includes"
	@cp lib/debug/*.a             "$(DIST_PKG_DIR)/lib/debug/"
	@cp lib/release/*.a           "$(DIST_PKG_DIR)/lib/release/"
	@cp engine-sdk/engine.mk      "$(DIST_PKG_DIR)/make/"
	@cp "$(_CUI_INSTALLED)"       "$(DIST_PKG_DIR)/tools/"
	@cp Libraries/vcpkg.json      "$(DIST_PKG_DIR)/vcpkg.json"
	@cp engine-sdk/dist.gitignore "$(DIST_PKG_DIR)/.gitignore"
	@cp engine-sdk/dist-README.md "$(DIST_PKG_DIR)/README.md"
endif
	@echo "Engine package ready: $(DIST_PKG_DIR)/"

# ── zip helper ────────────────────────────────────────────────────────────────
# Zips from the *parent* of _DIST_STAGING so the top-level dir in the archive
# is the staging dir name (e.g. pkg-linux-x64/...).  strip_top_dir() in Rust
# strips that prefix, placing files directly under engine/.
_STAGING_PARENT := $(patsubst %/,%,$(dir $(_DIST_STAGING)))
_STAGING_NAME   := $(notdir $(_DIST_STAGING))
define _DO_ZIP
	cd "$(_STAGING_PARENT)" && \
	if command -v zip >/dev/null 2>&1; then \
	    zip -r "$(abspath $(1))" "$(_STAGING_NAME)/"; \
	elif command -v 7z >/dev/null 2>&1; then \
	    7z a -tzip "$(abspath $(1))" "$(_STAGING_NAME)/" > /dev/null; \
	else \
	    python3 -c "\
import zipfile,os,sys; \
out=sys.argv[1]; top=sys.argv[2]; cwd=os.getcwd(); src=os.path.join(cwd,top); \
z=zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED); \
[z.write(os.path.join(d,f),os.path.join(top,os.path.relpath(os.path.join(d,f),src))) \
 for d,_,fs in os.walk(src) for f in fs]; \
z.close()" "$(abspath $(1))" "$(_STAGING_NAME)"; \
	fi
endef

$(ENGINE_PKG_ZIP): dist
	@mkdir -p dist
	$(call _DO_ZIP,$(ENGINE_PKG_ZIP))
	@echo "Zipped: $(ENGINE_PKG_ZIP)"

# ── engine tool binary ────────────────────────────────────────────────────────
# Embeds the engine package zip as an offline fallback via ENGINE_PAYLOAD_ZIP.
# GITHUB_REPO is baked in at compile time so the URL is not hardcoded in source.
$(ENGINE_INIT_EXE): $(TOOLS_WORKSPACE) $(TOOLS_RS_SRCS) dist
	@mkdir -p "$(ENGINE_PAYLOAD_DIR)"
	$(call _DO_ZIP,$(ENGINE_PAYLOAD_ZIP))
	REPO_HOST="$(REPO_HOST)" \
	GITHUB_REPO="$(REPO_SLUG)" \
	ENGINE_NAME="$(ENGINE_NAME)" \
	ENGINE_VERSION="$(ENGINE_VERSION)" \
	ENGINE_PUBLISHER="$(ENGINE_PUBLISHER)" \
	ENGINE_AUTHOR="$(ENGINE_AUTHOR)" \
	ENGINE_EMAIL="$(ENGINE_EMAIL)" \
	ENGINE_DESCRIPTION="$(ENGINE_DESCRIPTION)" \
	ENGINE_HOMEPAGE="$(ENGINE_HOMEPAGE)" \
	ENGINE_PAYLOAD_ZIP="$(abspath $(ENGINE_PAYLOAD_ZIP))" \
	CARGO_TARGET_DIR="$(abspath $(CARGO_TARGET_DIR))" \
	    $(CARGO) build --release --manifest-path $(TOOLS_WORKSPACE) --bin engine
	@echo "Built: $(ENGINE_INIT_EXE)"

$(ENGINE_TOOL_EXE): $(ENGINE_INIT_EXE)
	@mkdir -p dist
ifeq ($(DETECTED_OS),Windows)
	@powershell -NoProfile -Command "Copy-Item -Path '$(ENGINE_INIT_EXE)' -Destination '$@' -Force"
else
	cp "$(ENGINE_INIT_EXE)" "$@"
endif
	@echo "Engine tool: $@"

.PHONY: engine-tool
engine-tool: $(ENGINE_PKG_ZIP) $(ENGINE_TOOL_EXE)
	@echo "engine-tool done: $(ENGINE_TOOL_EXE)  $(ENGINE_PKG_ZIP)"

# ── Sync demo ─────────────────────────────────────────────────────────────────
# Copies the current BUILD_TYPE artifacts directly into demo/engine/.
# Faster than install-demo: no dist packaging step required.
# vcpkg_installed/ is linked (not copied) to avoid duplicating hundreds of MB.
.PHONY: sync-demo
sync-demo: $(ENGINE_LIBS) $(_CUI_INSTALLED)
	@mkdir -p demo/engine/lib/$(BUILD_TYPE) demo/engine/make demo/engine/tools
ifeq ($(DETECTED_OS),Windows)
	@mkdir -p demo/engine/includes
	@powershell -NoProfile -Command "Copy-Item -Path '$(INCLUDES_BASE)/*' -Destination 'demo/engine/includes' -Recurse -Force"
else
	@rm -rf demo/engine/includes && cp -r $(INCLUDES_BASE) demo/engine/includes
endif
ifeq ($(DETECTED_OS),Windows)
	@for f in $(ENGINE_LIBS); do powershell -NoProfile -Command "Copy-Item -Path '$$f' -Destination 'demo/engine/lib/$(BUILD_TYPE)' -Force"; done
	@powershell -NoProfile -Command "Copy-Item -Path 'engine-sdk/engine.mk' -Destination 'demo/engine/make' -Force"
	@powershell -NoProfile -Command "Copy-Item -Path '$(_CUI_INSTALLED)' -Destination 'demo/engine/tools' -Force"
else
	@cp $(ENGINE_LIBS) demo/engine/lib/$(BUILD_TYPE)/
	@cp engine-sdk/engine.mk demo/engine/make/
	@cp "$(_CUI_INSTALLED)" demo/engine/tools/
endif
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
install-demo: engine-tool
ifeq ($(DETECTED_OS),Windows)
	@mkdir -p demo/engine
	@powershell -NoProfile -Command "Copy-Item -Path '$(DIST_PKG_DIR)/*' -Destination 'demo/engine' -Recurse -Force"
else
	@rm -rf demo/engine
	@cp -r "$(DIST_PKG_DIR)" demo/engine
endif
	@if [ "$(DETECTED_OS)" = "Windows" ]; then \
	    TARGET=$$(cd "$(VCPKG_INSTALLED_ROOT)" && pwd -W 2>/dev/null || echo "$(VCPKG_INSTALLED_ROOT)"); \
	    MSYS_NO_PATHCONV=1 cmd /c "mklink /J \"demo\\engine\\vcpkg_installed\" \"$$TARGET\"" 2>/dev/null \
	        || ln -sfn "../../$(VCPKG_INSTALLED_ROOT)" demo/engine/vcpkg_installed; \
	else \
	    ln -sfn "../../$(VCPKG_INSTALLED_ROOT)" demo/engine/vcpkg_installed; \
	fi
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
	            if jq --arg d "$$dep" -e '.dependencies[] | select((type=="string" and .==$$d) or (.name?==$$d))' Libraries/vcpkg.json >/dev/null 2>&1; then \
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
