# Makefile — ProceduralGeneration (game project)
#
# This Makefile is an example of how to use the C++ Game Engine.
# It compiles only the game code in src/ and links against the pre-built engine archives.
#
# ── Prerequisites ─────────────────────────────────────────────────────────────
#   - Engine release unpacked (lib/ includes/ make/ tools/ present in ENGINE_DIR)
#   - GNU Make, g++ / clang++
#
# ── Quick start ───────────────────────────────────────────────────────────────
#   make debug        build + run debug
#   make release      build release
#   make run          run last built binary
#   make clean        remove game objects + Generated/
#   make fclean       remove everything (objects, bin/)
#   make re-debug     fclean + debug

SHELL := /bin/sh
-include config.mk

# ── Toolchain ─────────────────────────────────────────────────────────────────
CXX           ?= g++
CC            ?= gcc
RC            ?= windres
CLANG_FORMAT  ?= clang-format
CLANG_TIDY    ?= clang-tidy
VCPKG         ?= vcpkg

# ── Engine integration ────────────────────────────────────────────────────────
# ENGINE_DIR points to the unpacked engine release (or the engine repo root).
# For a standalone project using a downloaded release, set this to the release directory:
#   ENGINE_DIR := ../engine-windows-x64
ENGINE_DIR   := .
GAME_SRC_DIR := src
BUILD_TYPE   ?= release

ifneq ($(findstring debug,$(MAKECMDGOALS)),)
    BUILD_TYPE := debug
else ifneq ($(findstring dev,$(MAKECMDGOALS)),)
    BUILD_TYPE := dev
else ifneq ($(findstring release,$(MAKECMDGOALS)),)
    BUILD_TYPE := release
endif

include $(ENGINE_DIR)/make/engine.mk

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
TEST_SRCS    := $(shell find $(TEST_DIR) -type f -name "*.cpp" 2>/dev/null)
TEST_OBJS    := $(TEST_SRCS:$(TEST_DIR)/%.cpp=$(OBJ_DIR)/tests/%.o) $(GEN_OBJS)
TEST_TARGET  := bin/tests/tests$(ENGINE_EXE_EXT)

# Style sources (game code only — engine has its own)
STYLE_SRCS := $(shell find $(SRC_DIR) $(TEST_DIR) -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.cpp" -o -name "*.c" \) 2>/dev/null)
LINT_FLAGS  := $(filter-out -flto=jobserver,$(ENGINE_CXXFLAGS))

# vcpkg platform-specific copy targets
ifeq ($(ENGINE_OS),Windows)
    COPY_EXTRA := copy_libs
else
    COPY_EXTRA :=
endif

# ── Engine build (alias for Libraries/Makefile dist) ─────────────────────────
.PHONY: engine engine-dist
engine engine-dist:
	$(MAKE) -f Libraries/Makefile dist

# ── Main targets ──────────────────────────────────────────────────────────────
.PHONY: all debug dev release run run-debug run-dev run-release

all debug dev release: $(TARGET)

run run-debug run-dev: $(TARGET)
	@./$(TARGET)

run-release: $(TARGET)
	@./$(TARGET)

# ── Link ──────────────────────────────────────────────────────────────────────
$(TARGET): $(ALL_OBJS) $(ENGINE_LIBS) | $(BIN_DIR) copy_res $(COPY_EXTRA)
	$(CXX) $(ENGINE_CXXFLAGS) $(ALL_OBJS) $(ENGINE_LINK_LIBS) $(ENGINE_LDFLAGS) -o $@
	@echo "Compiled: $@"

# ── Compilation rules ─────────────────────────────────────────────────────────
$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(ENGINE_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -c $< -o $@
	@echo "  CXX $<"

$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(ENGINE_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -c $< -o $@
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
	@cp -r $(RES_DIR) $(BIN_DIR)/

copy_libs: | $(BIN_DIR)
	@set -- "$(ENGINE_DIR)/vcpkg_installed/$(ENGINE_VCPKG_TRIPLET)/bin"/*.dll; \
	 if [ -e "$$1" ]; then cp -f "$$@" "$(BIN_DIR)/" 2>/dev/null || :; fi

# ── Dependencies (vcpkg) ──────────────────────────────────────────────────────
.PHONY: install install_deps

install install_deps:
	$(VCPKG) install --triplet=$(ENGINE_VCPKG_TRIPLET) \
	    --x-manifest-root=. \
	    --x-install-root=$(ENGINE_DIR)/vcpkg_installed

# ── Directories ───────────────────────────────────────────────────────────────
$(BIN_DIR) bin/tests:
	@mkdir -p "$@"

# ── Clean ─────────────────────────────────────────────────────────────────────
.PHONY: clean fclean re re-debug re-dev re-release

# clean: game objects + Generated/ only — engine archives untouched
clean:
	@rm -rf obj/ $(ENGINE_GEN_DIR) bin/
	@echo "Cleaned (engine archives preserved)"

fclean:
	@rm -rf obj/ $(ENGINE_GEN_DIR) bin/
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
	    $(CXX) $(ENGINE_CXXFLAGS) $(ENGINE_INCLUDES) -fsyntax-only $(SRCS_CPP); fi

check-format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "clang-format not found"; exit 1; }
	@if [ -n "$(strip $(STYLE_SRCS))" ]; then \
	    $(CLANG_FORMAT) --dry-run --Werror $(STYLE_SRCS); fi

format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "clang-format not found"; exit 1; }
ifeq ($(ENGINE_OS),Windows)
	@powershell -NoProfile -Command "& '.\\tools\\format_sources.ps1' '$(CLANG_FORMAT)'"
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
