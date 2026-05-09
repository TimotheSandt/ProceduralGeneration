# demo/Makefile — game project build system
#
# Works in two modes:
#   Template mode  (standalone game project): ENGINE_DIR = engine (default)
#   In-repo mode   (demo/ inside engine repo): override via config.local.mk
#
# ── Quick start ───────────────────────────────────────────────────────────────
#   make debug        build debug binary
#   make release      build release binary
#   make run          run last built binary
#   make clean        remove objects + Generated/
#   make fclean       remove objects, Generated/, and bin/
#
# ── Dependencies ──────────────────────────────────────────────────────────────
#   make install-deps             install all vcpkg.json packages
#   make add-dep DEP=nlohmann-json   add & install a package
#   make search-dep Q=json           search vcpkg catalog

SHELL := sh
-include config.mk
-include config.local.mk

# ── Toolchain ─────────────────────────────────────────────────────────────────
CXX          ?= g++
CC           ?= gcc
RC           ?= windres
CLANG_FORMAT ?= clang-format
CLANG_TIDY   ?= clang-tidy
VCPKG        ?= vcpkg

# ── Engine integration ────────────────────────────────────────────────────────
# Defaults for standalone (template) use. In-repo: override via config.local.mk
#   ENGINE_DIR  ?= engine
#   _ENGINE_MK  ?= $(ENGINE_DIR)/make/engine.mk
#   _VCPKG_ROOT ?= vcpkg_installed
ENGINE_DIR  ?= engine
_ENGINE_MK  ?= $(ENGINE_DIR)/make/engine.mk
_VCPKG_ROOT ?= vcpkg_installed

GAME_SRC_DIR := src
BUILD_TYPE   ?= release

ifneq ($(findstring debug,$(MAKECMDGOALS)),)
    BUILD_TYPE := debug
else ifneq ($(findstring dev,$(MAKECMDGOALS)),)
    BUILD_TYPE := dev
else ifneq ($(findstring release,$(MAKECMDGOALS)),)
    BUILD_TYPE := release
endif

_ENGINE_MK_FOUND := $(wildcard $(_ENGINE_MK))

ifeq ($(_ENGINE_MK_FOUND),)
    _DEMO_ENGINELESS_GOALS := install install-deps search search-dep clean fclean info
    ifeq ($(filter $(_DEMO_ENGINELESS_GOALS),$(MAKECMDGOALS)),)
        $(error Missing engine SDK at $(_ENGINE_MK). Run "make sync-demo" or "make install-demo" from the repository root first)
    endif

    _DEMO_UNAME := $(shell uname -s 2>/dev/null || echo Unknown)
    ifeq ($(OS),Windows_NT)
        ENGINE_OS := Windows
    else ifneq ($(filter Linux,$(_DEMO_UNAME)),)
        ENGINE_OS := Linux
    else ifneq ($(filter Darwin,$(_DEMO_UNAME)),)
        ENGINE_OS := Darwin
    else ifneq ($(filter MSYS% MINGW% CYGWIN%,$(_DEMO_UNAME)),)
        ENGINE_OS := Windows
    else
        ENGINE_OS := Linux
    endif

    ifeq ($(ENGINE_OS),Windows)
        ENGINE_VCPKG_TRIPLET ?= x64-mingw-static
        ENGINE_EXE_EXT       := .exe
    else ifeq ($(ENGINE_OS),Darwin)
        _DEMO_ARCH := $(shell uname -m 2>/dev/null)
        ifeq ($(_DEMO_ARCH),arm64)
            ENGINE_VCPKG_TRIPLET ?= arm64-osx
        else
            ENGINE_VCPKG_TRIPLET ?= x64-osx
        endif
        ENGINE_EXE_EXT :=
    else
        _DEMO_ARCH := $(shell uname -m 2>/dev/null)
        ifeq ($(_DEMO_ARCH),aarch64)
            ENGINE_VCPKG_TRIPLET ?= arm64-linux
        else
            ENGINE_VCPKG_TRIPLET ?= x64-linux
        endif
        ENGINE_EXE_EXT :=
    endif

    ENGINE_CXXFLAGS      :=
    ENGINE_INCLUDES      := -I$(GAME_SRC_DIR)
    ENGINE_LIBS          :=
    ENGINE_LINK_LIBS     :=
    ENGINE_LDFLAGS       :=
    ENGINE_GEN_DIR       := Generated
    GEN_CPP_SOURCES      :=
    GEN_REDIRECT_SOURCES :=
else
    include $(_ENGINE_MK)
endif

# ── Game-specific vcpkg ───────────────────────────────────────────────────────
# Game deps are installed to $(_VCPKG_ROOT)/ (separate from the engine's own
# vcpkg_installed inside ENGINE_DIR). In template mode this is vcpkg_installed/
# at the project root; in in-repo mode it is ../vcpkg_installed (shared).
_GAME_VCPKG_DIR       = $(_VCPKG_ROOT)/$(ENGINE_VCPKG_TRIPLET)
_ADD_DEP_VCPKG_ROOT   = $(_VCPKG_ROOT)
_ADD_DEP_VCPKG_LIBDIR = $(_VCPKG_ROOT)/$(ENGINE_VCPKG_TRIPLET)/lib

GAME_CXXFLAGS = $(EXTRA_CXXFLAGS) \
    $(if $(wildcard $(_GAME_VCPKG_DIR)/include),-I$(_GAME_VCPKG_DIR)/include)
GAME_LDFLAGS  = \
    $(if $(wildcard $(_GAME_VCPKG_DIR)/lib),-L$(_GAME_VCPKG_DIR)/lib) \
    $(EXTRA_LDLIBS)

# ── Directories ───────────────────────────────────────────────────────────────
SRC_DIR  := src
OBJ_DIR  := obj/$(BUILD_TYPE)
BIN_DIR  := bin/$(BUILD_TYPE)
TEST_DIR := tests
RES_DIR  := res

# ── Build target name ─────────────────────────────────────────────────────────
ifeq ($(BUILD_TYPE),release)
    TARGET_NAME := $(PROJECT_NAME)
else
    TARGET_NAME := main
endif
TARGET := $(BIN_DIR)/$(TARGET_NAME)$(ENGINE_EXE_EXT)

# ── Sources ───────────────────────────────────────────────────────────────────
SRCS_CPP := $(shell find $(SRC_DIR) -type f -name "*.cpp" 2>/dev/null)
SRCS_C   := $(shell find $(SRC_DIR) -type f -name "*.c"   2>/dev/null)

OBJS_CPP := $(SRCS_CPP:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/src/%.o)
OBJS_C   := $(SRCS_C:$(SRC_DIR)/%.c=$(OBJ_DIR)/src/%.o)
GEN_OBJS := $(GEN_CPP_SOURCES:$(ENGINE_GEN_DIR)/%.gen.cpp=$(OBJ_DIR)/gen/%.o)
ALL_OBJS := $(OBJS_CPP) $(OBJS_C) $(GEN_OBJS)
TEST_GAME_OBJS := $(filter-out $(OBJ_DIR)/src/main.o,$(OBJS_CPP) $(OBJS_C))

# Windows icon resource
ICON_RC :=
ifneq ($(strip $(ICON_NAME)),)
ifneq ($(wildcard $(ICON_NAME)),)
ifeq ($(ENGINE_OS),Windows)
    ICON_OBJ := $(OBJ_DIR)/src/icon.o
    ALL_OBJS += $(ICON_OBJ)
endif
endif
endif

# Test sources
TEST_SRCS   := $(shell find $(TEST_DIR) -type f -name "*.cpp" 2>/dev/null)
TEST_OBJS   := $(TEST_SRCS:$(TEST_DIR)/%.cpp=$(OBJ_DIR)/tests/%.o) $(TEST_GAME_OBJS) $(GEN_OBJS)
TEST_TARGET := bin/tests/tests$(ENGINE_EXE_EXT)

# Style sources
STYLE_SRCS := $(shell find $(SRC_DIR) $(TEST_DIR) -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.cpp" -o -name "*.c" \) 2>/dev/null)
LINT_FLAGS  := $(filter-out -flto=jobserver,$(ENGINE_CXXFLAGS))

# vcpkg platform-specific copy targets
ifeq ($(ENGINE_OS),Windows)
    COPY_EXTRA := copy_libs
else
    COPY_EXTRA :=
endif

# ── Main targets ──────────────────────────────────────────────────────────────
.PHONY: all debug dev release run run-debug run-dev run-release

all debug dev release: $(TARGET)

run run-debug run-dev: $(TARGET)
	@./$(TARGET)

run-release: $(TARGET)
	@./$(TARGET)

# ── Link ──────────────────────────────────────────────────────────────────────
$(TARGET): $(ALL_OBJS) $(ENGINE_LIBS) | $(BIN_DIR) copy_res $(COPY_EXTRA)
	$(CXX) $(ENGINE_CXXFLAGS) $(ALL_OBJS) \
	    $(ENGINE_LINK_LIBS) $(ENGINE_LDFLAGS) $(GAME_LDFLAGS) -o $@
	@echo "Compiled: $@"

# ── Compilation rules ─────────────────────────────────────────────────────────
$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(ENGINE_CXXFLAGS) $(GAME_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -c $< -o $@
	@echo "  CXX $<"

$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(ENGINE_CXXFLAGS) $(GAME_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -c $< -o $@
	@echo "  CC  $<"

$(OBJ_DIR)/gen/%.o: $(ENGINE_GEN_DIR)/%.gen.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(ENGINE_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -c $< -o $@
	@echo "  CXX [gen] $<"

# Ensure CUI files are generated before any object is compiled
$(ALL_OBJS): | $(GEN_CPP_SOURCES) $(GEN_REDIRECT_SOURCES)

# Windows icon resource
$(OBJ_DIR)/src/icon.o: $(BIN_DIR)/icon.rc
	@mkdir -p "$(dir $@)"
	$(RC) -i $< -o $@

$(BIN_DIR)/icon.rc: $(ICON_NAME) | $(BIN_DIR)
	@printf '1 ICON "%s"\n' "$(ICON_NAME)" > "$@"

# ── Tests ─────────────────────────────────────────────────────────────────────
.PHONY: test
test: $(TEST_TARGET)
	@./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) $(ENGINE_LIBS) | bin/tests
	$(CXX) $(ENGINE_CXXFLAGS) $(TEST_OBJS) $(ENGINE_LINK_LIBS) $(ENGINE_LDFLAGS) -o $@

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(ENGINE_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -I$(TEST_DIR) -c $< -o $@

# ── Resources & libraries ─────────────────────────────────────────────────────
.PHONY: copy_res copy_libs

copy_res: | $(BIN_DIR)
ifeq ($(ENGINE_OS),Windows)
	@if [ -d "$(RES_DIR)" ]; then mkdir -p "$(BIN_DIR)/$(RES_DIR)"; powershell -NoProfile -Command "Copy-Item -Path '$(RES_DIR)/*' -Destination '$(BIN_DIR)/$(RES_DIR)' -Recurse -Force"; fi
else
	@if [ -d "$(RES_DIR)" ]; then cp -r "$(RES_DIR)" "$(BIN_DIR)/"; fi
endif

copy_libs: | $(BIN_DIR)
	@set -- "$(ENGINE_DIR)/vcpkg_installed/$(ENGINE_VCPKG_TRIPLET)/bin"/*.dll; \
	 if [ -e "$$1" ]; then cp -f "$$@" "$(BIN_DIR)/" 2>/dev/null || :; fi
	@set -- "$(_GAME_VCPKG_DIR)/bin"/*.dll; \
	 if [ -e "$$1" ]; then cp -f "$$@" "$(BIN_DIR)/" 2>/dev/null || :; fi

# ── Directories ───────────────────────────────────────────────────────────────
$(BIN_DIR) bin/tests:
	@mkdir -p "$@"

# ── vcpkg dependency management ───────────────────────────────────────────────
# install:   no args → install all vcpkg.json deps (incremental, skips installed)
#            with args → add packages, install them, auto-detect linker flags
# search:    search the vcpkg catalog
#
# Usage:
#   make install
#   make install fmt nlohmann-json
#   make install fmt LIBS="-lfmt"   # rare: manual linker flag override
#   make search json

# Capture positional args: 'make install <pkg>...' / 'make search <kw>'
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

.PHONY: i install install-deps search search-dep

install install-deps:
	@if [ -z "$(_INSTALL_ARGS)" ]; then \
	    if [ -f "$(ENGINE_DIR)/vcpkg.json" ]; then \
	        $(VCPKG) install \
	            --triplet=$(ENGINE_VCPKG_TRIPLET) \
	            --x-manifest-root=$(ENGINE_DIR) \
	            --x-install-root=$(ENGINE_DIR)/vcpkg_installed \
	            --no-print-usage; \
	    fi; \
	    $(VCPKG) install \
	        --triplet=$(ENGINE_VCPKG_TRIPLET) \
	        --x-manifest-root=. \
	        --x-install-root=$(_ADD_DEP_VCPKG_ROOT) \
	        --no-print-usage; \
	else \
	    [ -f vcpkg.json ] || printf '{\n  "name": "game",\n  "version": "0.1.0",\n  "dependencies": []\n}\n' > vcpkg.json; \
	    LIBDIR="$(_ADD_DEP_VCPKG_LIBDIR)"; \
	    for dep in $(_INSTALL_ARGS); do \
	        if command -v jq >/dev/null 2>&1; then \
	            if jq --arg d "$$dep" -e '.dependencies[] | select((type=="string" and .==$$d) or (.name?==$$d))' vcpkg.json >/dev/null 2>&1; then \
	                printf "  [vcpkg.json] %s already listed\n" "$$dep"; \
	            else \
	                tmp=$$(mktemp); jq --arg d "$$dep" '.dependencies += [$$d]' vcpkg.json > "$$tmp" && mv "$$tmp" vcpkg.json; \
	                printf "  [vcpkg.json] Added %s\n" "$$dep"; \
	            fi; \
	        elif command -v python3 >/dev/null 2>&1; then \
	            DEP="$$dep" python3 -c "import json,os; dep=os.environ['DEP']; d=json.load(open('vcpkg.json')); deps=d.setdefault('dependencies',[]); n=[x if isinstance(x,str) else x.get('name','') for x in deps]; deps.append(dep) if dep not in n else None; open('vcpkg.json','w').write(json.dumps(d,indent=2))"; \
	            printf "  [vcpkg.json] Updated (%s)\n" "$$dep"; \
	        else \
	            printf "Error: jq or python3 required to edit vcpkg.json\n"; exit 1; \
	        fi; \
	        b=$$(mktemp); a=$$(mktemp); \
	        ls "$$LIBDIR/"*.a 2>/dev/null | sort > "$$b" || :; \
	        $(VCPKG) install "$$dep" \
	            --triplet=$(ENGINE_VCPKG_TRIPLET) \
	            --x-manifest-root=. \
	            --x-install-root=$(_ADD_DEP_VCPKG_ROOT) \
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
	            printf "  [config.mk] %s is header-only — no linker flags needed\n" "$$dep"; \
	        else \
	            printf "  Detected: %s\n" "$$NEW"; \
	            EXISTING=$$(grep '^EXTRA_LDLIBS' config.mk 2>/dev/null \
	                | sed 's/^EXTRA_LDLIBS[^=]*=[[:space:]]*//' \
	                | tr -s ' ' | sed 's/^[[:space:]]*//;s/[[:space:]]*$$//'); \
	            ALL=$$(printf '%s %s' "$$EXISTING" "$$NEW" \
	                | tr -s ' ' | sed 's/^[[:space:]]*//;s/[[:space:]]*$$//'); \
	            tmp=$$(mktemp); found=0; \
	            while IFS= read -r line; do \
	                case "$$line" in \
	                    EXTRA_LDLIBS*) printf 'EXTRA_LDLIBS ?= %s\n' "$$ALL" >> "$$tmp"; found=1;; \
	                    *) printf '%s\n' "$$line" >> "$$tmp";; \
	                esac; \
	            done < config.mk; \
	            [ "$$found" -eq 1 ] || printf '\nEXTRA_LDLIBS ?= %s\n' "$$ALL" >> "$$tmp"; \
	            mv "$$tmp" config.mk; \
	            printf "  [config.mk] EXTRA_LDLIBS = %s\n" "$$ALL"; \
	        fi; \
	    done; \
	fi

search search-dep:
	@[ -n "$(Q)" ] || { printf "Usage: make search <keyword>\n"; exit 1; }
	$(VCPKG) search "$(Q)"

# ── Clean ─────────────────────────────────────────────────────────────────────
.PHONY: clean fclean re re-debug re-dev re-release

clean:
	@rm -rf obj/ $(ENGINE_GEN_DIR)
	@echo "Cleaned"

fclean: clean
	@rm -rf bin/
	@echo "Full clean done"

re:         fclean all
re-debug:   fclean debug
re-dev:     fclean dev
re-release: fclean release

# ── Code quality ──────────────────────────────────────────────────────────────
.PHONY: check check-syntax check-format format lint

check: check-syntax check-format lint

check-syntax:
	@echo "Checking syntax..."
	@if [ -n "$(strip $(SRCS_CPP))" ]; then \
	    $(CXX) $(ENGINE_CXXFLAGS) $(GAME_CXXFLAGS) $(ENGINE_INCLUDES) -fsyntax-only $(SRCS_CPP); fi

check-format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "clang-format not found"; exit 1; }
	@if [ -n "$(strip $(STYLE_SRCS))" ]; then \
	    $(CLANG_FORMAT) --dry-run --Werror $(STYLE_SRCS); fi

format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "clang-format not found"; exit 1; }
ifeq ($(ENGINE_OS),Windows)
	@powershell -NoProfile -Command "& '..\tools\format_sources.ps1' '$(CLANG_FORMAT)'"
else
	@if [ -n "$(strip $(STYLE_SRCS))" ]; then $(CLANG_FORMAT) -i $(STYLE_SRCS); fi
endif

lint:
	@command -v $(CLANG_TIDY) >/dev/null 2>&1 || { echo "clang-tidy not found"; exit 1; }
	@if [ -n "$(strip $(SRCS_CPP))" ]; then \
	    $(CLANG_TIDY) $(SRCS_CPP) -- $(LINT_FLAGS) $(ENGINE_INCLUDES); fi

# ── Info ──────────────────────────────────────────────────────────────────────
.PHONY: info

info:
	@echo "Project:      $(PROJECT_NAME)"
	@echo "Build type:   $(BUILD_TYPE)"
	@echo "Platform:     $(ENGINE_OS)"
	@echo "Target:       $(TARGET)"
	@echo "Game sources: $(words $(SRCS_CPP)) C++ / $(words $(SRCS_C)) C"
	@echo "CUI views:    $(words $(GEN_CPP_SOURCES)) generated"
	@echo "Engine libs:  $(notdir $(ENGINE_LIBS))"
	@echo "Compiler:     $(CXX)"
	@echo ""
	@echo "Required tools:"
	@if command -v $(CXX) >/dev/null 2>&1; then printf "  [ok] %s\n" "$(CXX)"; \
	    else printf "  [!!] %-24s -- scoop install mingw / brew install gcc / apt install g++\n" "$(CXX)"; fi
	@if command -v $(AR) >/dev/null 2>&1; then printf "  [ok] %s\n" "$(AR)"; \
	    else printf "  [!!] %-24s -- scoop install mingw / brew install binutils / apt install binutils\n" "$(AR)"; fi
	@if command -v $(VCPKG) >/dev/null 2>&1; then printf "  [ok] %s\n" "$(VCPKG)"; \
	    else printf "  [!!] %-24s -- https://vcpkg.io/en/getting-started\n" "$(VCPKG)"; fi

# ── Dependency files ──────────────────────────────────────────────────────────
-include $(wildcard $(ALL_OBJS:.o=.d))
-include $(wildcard $(TEST_OBJS:.o=.d))

# ── Installer (packaging) ─────────────────────────────────────────────────────
.PHONY: installer create_windows_installer create_linux_installer

BUILD_ARTIFACTS_DIR := build
TMP_DEB_DIR         := /tmp/proceduralgeneration_deb

ifeq ($(ENGINE_OS),Windows)
    INSTALLER_FILE := $(PROJECT_NAME)-$(VERSION)-x64-setup.exe
    NSIS_COMPILER  := makensis
    installer: create_windows_installer
else ifeq ($(ENGINE_OS),Linux)
    INSTALLER_FILE := $(PROJECT_NAME)_$(VERSION)_amd64.deb
    installer: create_linux_installer
else
    installer:
	    @echo "No installer target for this platform"
endif

create_windows_installer: | $(BUILD_ARTIFACTS_DIR)
	$(NSIS_COMPILER) \
	    -DPRODUCT_NAME="$(PROJECT_NAME)" \
	    -DVERSION="$(VERSION)" \
	    -DARCH="x64" \
	    -DICON_NAME="$(ICON_NAME)" \
	    -DOUTPUT_FILE="$(INSTALLER_FILE)" \
	    installers/windows/installer.nsi
	@mv -f "installers/windows/$(INSTALLER_FILE)" "$(BUILD_ARTIFACTS_DIR)/"
	@echo "Installer: $(BUILD_ARTIFACTS_DIR)/$(INSTALLER_FILE)"

create_linux_installer: | $(BUILD_ARTIFACTS_DIR)
	@rm -rf "$(TMP_DEB_DIR)"
	@mkdir -p "$(TMP_DEB_DIR)/usr/bin" "$(TMP_DEB_DIR)/usr/share/$(PROJECT_NAME)" "$(TMP_DEB_DIR)/DEBIAN"
	@printf 'Package: %s\nVersion: %s\nArchitecture: amd64\nMaintainer: %s\nDescription: %s\n' \
	    "$(PROJECT_NAME)" "$(VERSION)" "$(MAINTAINER)" "$(PROJECT_DESCRIPTION)" \
	    > "$(TMP_DEB_DIR)/DEBIAN/control"
	@cp -f "$(TARGET)" "$(TMP_DEB_DIR)/usr/bin/$(PROJECT_NAME)"
	@cp -R "$(RES_DIR)/." "$(TMP_DEB_DIR)/usr/share/$(PROJECT_NAME)/"
	@dpkg-deb --build "$(TMP_DEB_DIR)" "$(BUILD_ARTIFACTS_DIR)/$(INSTALLER_FILE)"
	@rm -rf "$(TMP_DEB_DIR)"
	@echo "Package: $(BUILD_ARTIFACTS_DIR)/$(INSTALLER_FILE)"

$(BUILD_ARTIFACTS_DIR):
	@mkdir -p "$@"
