# Include configuration
-include config.mk

# This Makefile assumes GNU make recipes run in a POSIX shell.
SHELL := /bin/sh
UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)

# Toolchain
CXX ?= g++
CC ?= gcc
RC ?= windres
VCPKG ?= vcpkg
CLANG_FORMAT ?= clang-format
CLANG_TIDY ?= clang-tidy
PLATFORM_DEFINES :=

# Directories
INCLUDES_BASE := Libraries/includes
LIBRARIES_SRC_DIR := Libraries/src
LIBRARIES_LIB_DIR := Libraries/libs
MAIN_SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
LIB_DIR := lib
BUILD_DIR := build
RES_DIR := res
TEST_DIR := tests
TMP_DEB_DIR := /tmp/proceduralgeneration_deb

# Platform detection
ifeq ($(OS),Windows_NT)
	DETECTED_OS := Windows
else ifneq ($(filter Linux,$(UNAME_S)),)
	DETECTED_OS := Linux
else ifneq ($(filter Darwin,$(UNAME_S)),)
	DETECTED_OS := Darwin
else ifneq ($(filter MSYS% MINGW% CYGWIN%,$(UNAME_S)),)
	DETECTED_OS := Windows
else
	DETECTED_OS := $(UNAME_S)
endif

ifeq ($(DETECTED_OS),Windows)
	EXE_EXT := .exe
	VCPKG_TRIPLET ?= x64-mingw-static
	GLFW_LINK_NAME := glfw3
	PLATFORM_DEFINES += -DNOMINMAX -DWIN32_LEAN_AND_MEAN
	LDFLAGS = -L$(VCPKG_INSTALLED_DIR)/lib -l$(GLFW_LINK_NAME) -lglad -lmsdfgen-core -lfreetype -lpng16 -lzlib -lbz2 -lbrotlidec -lbrotlienc -lbrotlicommon -lvulkan-1 -lshaderc -lshaderc_util -lglslang -lMachineIndependent -lGenericCodeGen -lOSDependent -lSPIRV -lSPIRV-Tools-opt -lSPIRV-Tools -lpsapi -lwinmm -lgdi32 -luser32 -lshell32 -lopengl32 -lstdc++exp
	COPY_LIBS_TARGETS := copy_libs
	CREATE_INSTALLER := create_windows_installer
	ARCHITECTURE := $(ARCHITECTURE_WINDOWS)
	INSTALLER_FILE := $(PROJECT_NAME)-$(VERSION)-$(ARCHITECTURE)-setup.exe
	ifneq ($(wildcard C:/Program\ Files\ (x86)/NSIS/makensis.exe),)
		NSIS_COMPILER := "C:\Program Files (x86)\NSIS\makensis.exe"
	else ifneq ($(wildcard C:/Program Files/NSIS/makensis.exe),)
		NSIS_COMPILER := "C:/Program Files/NSIS/makensis.exe"
	else
		NSIS_COMPILER := makensis
	endif
else ifeq ($(DETECTED_OS),Linux)
	EXE_EXT :=
	# Auto-detect architecture (x86_64 → x64-linux, aarch64 → arm64-linux)
	_ARCH := $(shell uname -m)
	ifeq ($(_ARCH),aarch64)
		VCPKG_TRIPLET ?= arm64-linux
	else
		VCPKG_TRIPLET ?= x64-linux
	endif
	GLFW_LINK_NAME := glfw
	LDFLAGS = -L$(VCPKG_LIB_DIR) -lglfw -lglad -lGL -lvulkan -lfreetype -lpng16 -lz -lbz2 -lbrotlidec -lbrotlicommon -lshaderc -lshaderc_util -lglslang -lSPIRV-Tools-opt -lSPIRV-Tools -lSPIRV -lpthread -lX11 -ldl -lm
	COPY_LIBS_TARGETS :=
	CREATE_INSTALLER := create_linux_installer
	ARCHITECTURE := $(ARCHITECTURE_LINUX)
	INSTALLER_FILE := $(PACKAGE)_$(VERSION)_$(ARCHITECTURE).deb
else ifeq ($(DETECTED_OS),Darwin)
	EXE_EXT :=
	# Auto-detect architecture (x86_64 → x64-osx, arm64 → arm64-osx)
	_ARCH := $(shell uname -m)
	ifeq ($(_ARCH),arm64)
		VCPKG_TRIPLET ?= arm64-osx
	else
		VCPKG_TRIPLET ?= x64-osx
	endif
	GLFW_LINK_NAME := glfw
	LDFLAGS = -L$(VCPKG_LIB_DIR) -lglfw -lglad -lfreetype -lpng16 -lz -lbz2 -lbrotlidec -lbrotlicommon \
	          -lshaderc -lshaderc_util -lglslang -lSPIRV-Tools-opt -lSPIRV-Tools -lSPIRV \
	          -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	COPY_LIBS_TARGETS :=
	CREATE_INSTALLER :=
	INSTALLER_FILE :=
else
	EXE_EXT :=
	VCPKG_TRIPLET ?= x64-linux
	GLFW_LINK_NAME := glfw
	LDFLAGS =
	COPY_LIBS_TARGETS :=
	CREATE_INSTALLER :=
	INSTALLER_FILE :=
endif

VCPKG_INSTALLED_ROOT ?= ./vcpkg_installed
VCPKG_INSTALLED_DIR := $(VCPKG_INSTALLED_ROOT)/$(VCPKG_TRIPLET)

# Libraries
VCPKG_LIB_DIR := $(VCPKG_INSTALLED_DIR)/lib
VCPKG_BIN_DIR := $(VCPKG_INSTALLED_DIR)/bin
LOCAL_LIB_DIRS := $(sort $(dir $(shell find $(LIBRARIES_LIB_DIR) -type f \( -name "*.a" -o -name "*.lib" -o -name "*.dll" \) 2>/dev/null)))
LOCAL_A_LIBS := $(shell find $(LIBRARIES_LIB_DIR) -type f -name "*.a" 2>/dev/null)
LOCAL_IMPORT_LIBS := $(shell find $(LIBRARIES_LIB_DIR) -type f -name "*.lib" 2>/dev/null)
LOCAL_DLLS := $(shell find $(LIBRARIES_LIB_DIR) -type f -name "*.dll" 2>/dev/null)
LOCAL_LINK_DIR_FLAGS := $(foreach dir,$(LOCAL_LIB_DIRS),-L$(dir))
LOCAL_A_LINK_INPUTS := $(LOCAL_A_LIBS)
LOCAL_IMPORT_LINK_INPUTS := $(LOCAL_IMPORT_LIBS)

# Includes and resources
INCLUDES_DIRS := $(notdir $(wildcard $(INCLUDES_BASE)/*))
INCLUDES := -I$(INCLUDES_BASE) $(foreach dir,$(INCLUDES_DIRS),-I$(INCLUDES_BASE)/$(dir)) -I$(VCPKG_INSTALLED_DIR)/include
TEST_INCLUDES := $(INCLUDES) -I$(TEST_DIR)
ICON_RC := $(RES_DIR)/icon.rc

# Flags
# -m64 is x86-only; omit it on ARM to avoid "unrecognized command-line option" errors.
ifeq ($(DETECTED_OS),Windows)
	ARCH_FLAG := -m64
else
	_HOST_ARCH := $(shell uname -m 2>/dev/null)
	ifeq ($(_HOST_ARCH),x86_64)
		ARCH_FLAG := -m64
	else
		ARCH_FLAG :=
	endif
endif

CFLAGS := $(ARCH_FLAG) -O2 -DNDEBUG
CXXFLAGS := -std=c++23
DEPFLAGS := -MMD -MP

# Target configuration
BUILD_TYPE := normal
TARGET_NAME := $(PROJECT_NAME)

ifneq ($(findstring debug,$(MAKECMDGOALS)),)
	CFLAGS := -Wall -Wextra $(ARCH_FLAG) -O1 -g -DDEBUG
	BUILD_TYPE := debug
	TARGET_NAME := main
else ifneq ($(findstring dev,$(MAKECMDGOALS)),)
	CFLAGS := $(ARCH_FLAG) -g3 -O0 -DDEBUG
	BUILD_TYPE := dev
	TARGET_NAME := main
else ifneq ($(or $(findstring release,$(MAKECMDGOALS)),$(findstring installer,$(MAKECMDGOALS))),)
	BUILD_TYPE := release
	# Use -flto=jobserver only (subsumes plain -flto) to avoid duplicate flag.
	CFLAGS := -Wall -Wextra -Werror $(ARCH_FLAG) -O3 -DNDEBUG -DRELEASE
	CXXFLAGS += -flto=jobserver
endif

CXXFLAGS += $(CFLAGS) $(PLATFORM_DEFINES) -DGLM_ENABLE_EXPERIMENTAL
LINT_CXXFLAGS := $(filter-out -flto=jobserver,$(CXXFLAGS))

BIN_DIR_TYPE := $(BIN_DIR)/$(BUILD_TYPE)
OBJ_DIR_TYPE := $(OBJ_DIR)/$(BUILD_TYPE)
TARGET := $(BIN_DIR_TYPE)/$(TARGET_NAME)$(EXE_EXT)
TEST_BIN_DIR := $(BIN_DIR)/tests
TEST_OBJ_DIR := $(OBJ_DIR)/tests
TEST_TARGET := $(TEST_BIN_DIR)/tests$(EXE_EXT)
TEST_CPP_SOURCES = $(shell find $(TEST_DIR) -type f -name "*.cpp" 2>/dev/null)
STYLE_SOURCES = $(shell find Libraries src tests -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.c" -o -name "*.cpp" \) 2>/dev/null)

# Source files
LIBRARIES_CPP_SOURCES := $(shell find $(LIBRARIES_SRC_DIR) -type f -name "*.cpp" 2>/dev/null)
LIBRARIES_C_SOURCES := $(shell find $(LIBRARIES_SRC_DIR) -type f -name "*.c" 2>/dev/null)
MAIN_CPP_SOURCES := $(shell find $(MAIN_SRC_DIR) -type f -name "*.cpp" 2>/dev/null)
MAIN_C_SOURCES := $(shell find $(MAIN_SRC_DIR) -type f -name "*.c" 2>/dev/null)

LIB_SOURCES := $(LOCAL_DLLS) $(LOCAL_IMPORT_LIBS) $(LOCAL_A_LIBS)

# CUI precompiler
CUI_TOOL        := tools/cui/main.py
CUI_HEADERS     := $(INCLUDES_BASE)
GEN_DIR         := Generated
CUI_SOURCES     := $(shell $(FIND) $(MAIN_SRC_DIR) -type f -name "*.cui" 2>/dev/null)
GEN_CPP_SOURCES      := $(patsubst $(MAIN_SRC_DIR)/%.cui,$(GEN_DIR)/%.gen.cpp,$(CUI_SOURCES))
GEN_REDIRECT_SOURCES := $(patsubst $(MAIN_SRC_DIR)/%.cui,$(GEN_DIR)/%.cui,$(CUI_SOURCES))
SCANNED_HEADERS := $(shell $(FIND) $(CUI_HEADERS) -type f -name "*.h" 2>/dev/null)

ALL_CPP_SOURCES := $(LIBRARIES_CPP_SOURCES) $(MAIN_CPP_SOURCES) $(GEN_CPP_SOURCES)
ALL_C_SOURCES := $(LIBRARIES_C_SOURCES) $(MAIN_C_SOURCES)

# Generated/ before src/ so redirect .cui headers shadow the DSL source files
INCLUDES += -I$(GEN_DIR) -I$(MAIN_SRC_DIR)
TEST_INCLUDES += -I$(GEN_DIR) -I$(MAIN_SRC_DIR)

# Objects
LIBRARIES_CPP_OBJECTS := $(LIBRARIES_CPP_SOURCES:$(LIBRARIES_SRC_DIR)/%.cpp=$(OBJ_DIR_TYPE)/Libraries/%.o)
LIBRARIES_C_OBJECTS := $(LIBRARIES_C_SOURCES:$(LIBRARIES_SRC_DIR)/%.c=$(OBJ_DIR_TYPE)/Libraries/%.o)
MAIN_CPP_OBJECTS := $(MAIN_CPP_SOURCES:$(MAIN_SRC_DIR)/%.cpp=$(OBJ_DIR_TYPE)/src/%.o)
MAIN_C_OBJECTS := $(MAIN_C_SOURCES:$(MAIN_SRC_DIR)/%.c=$(OBJ_DIR_TYPE)/src/%.o)
GEN_CPP_OBJECTS := $(GEN_CPP_SOURCES:$(GEN_DIR)/%.gen.cpp=$(OBJ_DIR_TYPE)/Generated/%.o)

# Static archive for Libraries/ — output to lib/ so it survives make clean
LIB_ARCHIVE := $(LIB_DIR)/$(BUILD_TYPE)/libcorelibs.a

# App objects (src/ + Generated/) — rebuilt on every app change
APP_OBJECTS := $(MAIN_CPP_OBJECTS) $(MAIN_C_OBJECTS) $(GEN_CPP_OBJECTS)

# ALL_OBJECTS kept for check-syntax / lint targets
ALL_OBJECTS := $(LIBRARIES_CPP_OBJECTS) $(LIBRARIES_C_OBJECTS) $(APP_OBJECTS)

TEST_CPP_OBJECTS := $(TEST_CPP_SOURCES:$(TEST_DIR)/%.cpp=$(TEST_OBJ_DIR)/tests/%.o)
TEST_OBJECTS := $(TEST_CPP_OBJECTS) $(GEN_CPP_OBJECTS)

ifneq ($(strip $(ICON_NAME)),)
ifneq ($(wildcard $(ICON_NAME)),)
ifeq ($(DETECTED_OS),Windows)
	APP_OBJECTS += $(OBJ_DIR_TYPE)/src/icon.o
endif
endif
endif

COPY_TARGETS := copy_res $(COPY_LIBS_TARGETS)

# Build rules
all: $(TARGET)

debug dev: $(TARGET)

test: $(TEST_TARGET) copy_test_libs
	@./$(TEST_TARGET)

installer: $(CREATE_INSTALLER)

release: $(TARGET) installer
	@rm -rf "$(BUILD_DIR)/$(BUILD_TYPE)"
	@echo "Release build complete"

# Execution rules
run run-dev run-debug: $(TARGET)
	@./$(TARGET)

run-release: $(TARGET) installer
	@./$(TARGET)

# Windows installer
create_windows_installer: | $(BUILD_DIR)
	@echo "Creating Windows installer..."
	@echo "Installer File: $(INSTALLER_FILE)"
	@$(NSIS_COMPILER) \
		-DPRODUCT_NAME="$(PROJECT_NAME)" \
		-DVERSION="$(VERSION)" \
		-DPUBLISHER="$(PUBLISHER)" \
		-DMANTAINER="$(MAINTAINER)" \
		-DARCH="$(ARCHITECTURE)" \
		-DICON_NAME="$(ICON_NAME)" \
		-DOUTPUT_FILE="$(INSTALLER_FILE)" \
		-DPROJECT_DESCRIPTION="$(PROJECT_DESCRIPTION)" \
		installers/windows/installer.nsi
	@test -f "installers/windows/$(INSTALLER_FILE)" || { echo "Installer file was not generated!"; exit 1; }
	@mv -f "installers/windows/$(INSTALLER_FILE)" "$(BUILD_DIR)/"
	@echo "Windows installer created: $(BUILD_DIR)/$(INSTALLER_FILE)"

# Linux installer
create_linux_installer: | $(BUILD_DIR)
	@rm -rf "$(TMP_DEB_DIR)"
	@echo "Creating Linux .deb package..."
	@mkdir -p "$(TMP_DEB_DIR)/usr/bin" "$(TMP_DEB_DIR)/usr/share/proceduralgeneration" "$(TMP_DEB_DIR)/DEBIAN"
	@touch "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Package: $(PACKAGE)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Version: $(VERSION)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Maintainer: $(MAINTAINER)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Section: $(SECTION)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Priority: $(PRIORITY)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Architecture: $(ARCHITECTURE)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Depends: $(DEPENDS)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Description: $(PROJECT_DESCRIPTION)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@cp -f installers/linux/DEBIAN/postinst "$(TMP_DEB_DIR)/DEBIAN/"
	@cp -f installers/linux/DEBIAN/postrm "$(TMP_DEB_DIR)/DEBIAN/"
	@chmod 755 "$(TMP_DEB_DIR)/DEBIAN" "$(TMP_DEB_DIR)/DEBIAN/postinst" "$(TMP_DEB_DIR)/DEBIAN/postrm"
	@chmod 644 "$(TMP_DEB_DIR)/DEBIAN/control"
	@cp -f "bin/$(BUILD_TYPE)/$(PROJECT_NAME)" "$(TMP_DEB_DIR)/usr/bin/proceduralgeneration"
	@cp -R "$(RES_DIR)/." "$(TMP_DEB_DIR)/usr/share/proceduralgeneration/"
	@set -- "bin/$(BUILD_TYPE)"/*.so*; if [ -e "$$1" ]; then cp -f "$$@" "$(TMP_DEB_DIR)/usr/share/proceduralgeneration/"; fi
	@set -- "bin/$(BUILD_TYPE)"/*.dylib; if [ -e "$$1" ]; then cp -f "$$@" "$(TMP_DEB_DIR)/usr/share/proceduralgeneration/"; fi
	@dpkg-deb --build "$(TMP_DEB_DIR)" "$(BUILD_DIR)/$(INSTALLER_FILE)"
	@rm -rf "$(TMP_DEB_DIR)"
	@echo "Linux .deb created: $(BUILD_DIR)/$(INSTALLER_FILE)"

# Dependencies via vcpkg
install install_deps i:
	@echo "Checking and installing dependencies with vcpkg..."
	$(VCPKG) install --triplet=$(VCPKG_TRIPLET) --x-manifest-root=. --x-install-root=$(VCPKG_INSTALLED_ROOT)

remove_deps:
	@echo "Removing dependencies with vcpkg..."
	@rm -rf ./vcpkg_installed

reset_deps:
	@echo "Resetting dependencies with vcpkg..."
	@rm -rf $(VCPKG_INSTALLED_ROOT)
	$(VCPKG) install --triplet=$(VCPKG_TRIPLET) --x-manifest-root=. --x-install-root=$(VCPKG_INSTALLED_ROOT)

# Icon resource
$(OBJ_DIR_TYPE)/src/icon.o: $(BIN_DIR_TYPE)/$(ICON_RC)
	@mkdir -p "$(dir $@)"
	$(RC) -i $< -o $@

$(BIN_DIR_TYPE)/$(ICON_RC): $(ICON_NAME) | $(BIN_DIR_TYPE)
	@mkdir -p "$(dir $@)"
	@printf '1 ICON "%s"\n' "$(ICON_NAME)" > "$@"

# CUI generation: regenerate .gen.cpp/.gen.h when .cui source or scanned headers change
$(GEN_DIR)/%.gen.cpp: $(MAIN_SRC_DIR)/%.cui $(SCANNED_HEADERS)
	@$(MKDIR_P) "$(dir $@)"
	python $(CUI_TOOL) run --headers $(CUI_HEADERS) --output $(dir $@) $<

# Compilation rule for generated files
$(OBJ_DIR_TYPE)/Generated/%.o: $(GEN_DIR)/%.gen.cpp
	@$(MKDIR_P) "$(dir $@)"
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Generated) $(BUILD_TYPE): $<"

.PHONY: cui-gen
cui-gen: $(GEN_CPP_SOURCES)

# Ensure all generated sources (and their headers) exist before any object is compiled.
# Order-only (|) so objects are not recompiled just because a .gen.cpp timestamp changed;
# the normal pattern-rule dependency handles that for generated objects.
$(ALL_OBJECTS): | $(GEN_CPP_SOURCES)

# Asset copy
copy_libs: | $(BIN_DIR_TYPE)
	@echo "Copying libraries to $(BIN_DIR_TYPE)"
	@set -- "$(VCPKG_BIN_DIR)"/*.dll; if [ -e "$$1" ]; then cp -f "$$@" "$(BIN_DIR_TYPE)/" 2>/dev/null || :; fi
	@set -- $(LOCAL_DLLS); if [ -n "$(strip $(LOCAL_DLLS))" ] && [ -e "$$1" ]; then cp -f "$$@" "$(BIN_DIR_TYPE)/" 2>/dev/null || :; fi
	@set -- $(LOCAL_IMPORT_LIBS); if [ -n "$(strip $(LOCAL_IMPORT_LIBS))" ] && [ -e "$$1" ]; then cp -f "$$@" "$(BIN_DIR_TYPE)/" 2>/dev/null || :; fi
	@set -- $(LOCAL_A_LIBS); if [ -n "$(strip $(LOCAL_A_LIBS))" ] && [ -e "$$1" ]; then cp -f "$$@" "$(BIN_DIR_TYPE)/" 2>/dev/null || :; fi

copy_res: | $(BIN_DIR_TYPE)
	@echo "Copying resources to $(BIN_DIR_TYPE)"
	@if [ "$(BUILD_TYPE)" = "release" ] ; then \
		rm -rf "$(BIN_DIR_TYPE)/$(RES_DIR)"; \
		cp -R "$(RES_DIR)/." "$(BIN_DIR_TYPE)/"; \
	else \
		cp -R "$(RES_DIR)" "$(BIN_DIR_TYPE)/"; \
	fi

copy_test_libs: | $(TEST_BIN_DIR)
	@echo "Copying test libraries to $(TEST_BIN_DIR)"
	@set -- "$(VCPKG_BIN_DIR)"/*.dll; if [ -e "$$1" ]; then cp -f "$$@" "$(TEST_BIN_DIR)/" 2>/dev/null || :; fi
	@set -- $(LOCAL_DLLS); if [ -n "$(strip $(LOCAL_DLLS))" ] && [ -e "$$1" ]; then cp -f "$$@" "$(TEST_BIN_DIR)/" 2>/dev/null || :; fi

all_copy: $(COPY_TARGETS)

clean_bin:
	@echo "Cleaning $(BUILD_TYPE) binaries..."
	@rm -rf "$(BIN_DIR_TYPE)"

# clean: remove app objects + Generated only; Libraries archive preserved
clean:
	@echo "Cleaning app objects (Libraries archive preserved)..."
	@rm -f $(APP_OBJECTS) $(APP_OBJECTS:.o=.d)
	@rm -rf $(GEN_DIR)
	@rm -rf "$(BIN_DIR_TYPE)"
	@echo "App objects deleted"

# Static archive: rebuilt only when Libraries sources change
$(LIB_ARCHIVE): $(LIBRARIES_CPP_OBJECTS) $(LIBRARIES_C_OBJECTS)
	@mkdir -p "$(dir $@)"
	$(AR) rcs $@ $^
	@echo "Archived (Libraries) $(BUILD_TYPE): $@"

# Build target: links archive + app objects
$(TARGET): clean_bin all_copy $(APP_OBJECTS) $(LIB_ARCHIVE) | $(BIN_DIR_TYPE)
	$(CXX) $(CXXFLAGS) $(APP_OBJECTS) $(LIB_ARCHIVE) $(LOCAL_LINK_DIR_FLAGS) $(LOCAL_A_LINK_INPUTS) $(LOCAL_IMPORT_LINK_INPUTS) $(LDFLAGS) -o $@
	@echo "Compilation successful for: $(TARGET)"

$(TEST_TARGET): $(TEST_OBJECTS) $(LIB_ARCHIVE) | $(TEST_BIN_DIR)
	$(CXX) $(filter-out -flto=jobserver,$(CXXFLAGS)) $(TEST_OBJECTS) $(LIB_ARCHIVE) $(LOCAL_LINK_DIR_FLAGS) $(LOCAL_A_LINK_INPUTS) $(LOCAL_IMPORT_LINK_INPUTS) $(LDFLAGS) -o $@
	@echo "Compilation successful for: $(TEST_TARGET)"

# Compilation rules
$(OBJ_DIR_TYPE)/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Libraries) $(BUILD_TYPE): $<"

$(OBJ_DIR_TYPE)/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Libraries) $(BUILD_TYPE): $<"

$(OBJ_DIR_TYPE)/src/%.o: $(MAIN_SRC_DIR)/%.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Main) $(BUILD_TYPE): $<"

$(OBJ_DIR_TYPE)/src/%.o: $(MAIN_SRC_DIR)/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Main) $(BUILD_TYPE): $<"

$(TEST_OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(filter-out -flto=jobserver,$(CXXFLAGS)) $(DEPFLAGS) $(TEST_INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Tests): $<"

# Directories
$(BIN_DIR_TYPE):
	@echo "Creating $@ directory"
	@mkdir -p "$@"

$(BUILD_DIR):
	@mkdir -p "$@"

$(TEST_BIN_DIR):
	@mkdir -p "$@"

# Clean rules
# fclean: remove everything including compiled engine archives
fclean:
	@rm -rf $(OBJ_DIR)
	@rm -rf $(GEN_DIR)
	@rm -rf $(BIN_DIR)
	@rm -rf $(LIB_DIR)
	@rm -f installers/windows/*.exe
	@rm -f installers/linux/*.deb
	@echo "Full clean done"

# clean-libs: remove only the engine archives (forces engine recompilation)
clean-libs:
	@rm -rf $(LIB_DIR)
	@echo "Engine archives removed"

# clean-exec: remove only the compiled executables, keep resources and lib objects
clean-exec:
	@rm -f "$(BIN_DIR)/debug/main$(EXE_EXT)"
	@rm -f "$(BIN_DIR)/dev/main$(EXE_EXT)"
	@rm -f "$(BIN_DIR)/release/$(PROJECT_NAME)$(EXE_EXT)"
	@echo "Executables removed"

fclean-build: fclean
	@rm -rf $(BUILD_DIR)

re: fclean all
re-debug: fclean debug
re-dev: fclean dev
re-release: fclean release

# Check syntax and style
check: check-syntax check-format lint

check-syntax:
	@echo "Checking C++ syntax..."
	@if [ -n "$(strip $(ALL_CPP_SOURCES))" ]; then $(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only $(ALL_CPP_SOURCES); fi
	@echo "Checking C syntax..."
	@if [ -n "$(strip $(ALL_C_SOURCES))" ]; then $(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only $(ALL_C_SOURCES); fi

check-format:
	@echo "Checking code formatting..."
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "$(CLANG_FORMAT) not found"; exit 1; }
	@if [ -n "$(strip $(STYLE_SOURCES))" ]; then $(CLANG_FORMAT) --dry-run --Werror $(STYLE_SOURCES); fi

format:
	@echo "Formatting code..."
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "$(CLANG_FORMAT) not found"; exit 1; }
ifeq ($(DETECTED_OS),Windows)
	@powershell -NoProfile -Command "& '.\\tools\\format_sources.ps1' '$(CLANG_FORMAT)'"
else
	@if [ -n "$(strip $(STYLE_SOURCES))" ]; then $(CLANG_FORMAT) -i $(STYLE_SOURCES); fi
endif

lint:
	@echo "Running clang-tidy..."
	@command -v $(CLANG_TIDY) >/dev/null 2>&1 || { echo "$(CLANG_TIDY) not found"; exit 1; }
	@if [ -n "$(strip $(ALL_CPP_SOURCES))" ]; then $(CLANG_TIDY) $(ALL_CPP_SOURCES) -- $(LINT_CXXFLAGS) $(INCLUDES); fi
	@if [ -n "$(strip $(TEST_CPP_SOURCES))" ]; then $(CLANG_TIDY) $(TEST_CPP_SOURCES) -- $(LINT_CXXFLAGS) $(TEST_INCLUDES); fi

# Info
debug-info info-debug: info
dev-info info-dev: info
release-info info-release: info

info:
	@echo "Project: $(PROJECT_NAME)"
	@echo "Detected OS: $(DETECTED_OS)"
	@echo "Recipe shell: $(SHELL)"
	@echo "Structure:"
	@echo "  - Libraries/src/: $(words $(LIBRARIES_CPP_SOURCES)) C++ files, $(words $(LIBRARIES_C_SOURCES)) C files"
	@echo "  - src/: $(words $(MAIN_CPP_SOURCES)) C++ files, $(words $(MAIN_C_SOURCES)) C files"
	@echo "Total sources: $(words $(ALL_CPP_SOURCES)) C++, $(words $(ALL_C_SOURCES)) C"
	@echo "C++ Compiler: $(CXX)"
	@echo "C Compiler: $(CC)"
	@echo "C++ Flags: $(CXXFLAGS)"
	@echo "C Flags: $(CFLAGS)"
	@echo "Includes Directories: $(INCLUDES_DIRS)"
	@echo "Target: $(TARGET)"
ifeq ($(DETECTED_OS),Linux)
	@echo "Dependencies: run 'make install_deps' to install via vcpkg"
else ifeq ($(DETECTED_OS),Darwin)
	@echo "Dependencies: run 'make install_deps' to install via vcpkg"
else ifeq ($(DETECTED_OS),Windows)
	@echo "Dependencies: use MinGW/clang with a POSIX shell and install libraries via vcpkg"
	@echo "LIBS: $(notdir $(LIB_SOURCES))"
else
	@echo "Unknown OS"
endif

# Phony rules
.PHONY: all release dev debug
.PHONY: test
.PHONY: run run-release run-dev run-debug
.PHONY: clean clean-exec clean-libs clean_bin fclean fclean-build re re-debug re-dev re-release
.PHONY: info info-debug info-dev info-release debug-info dev-info release-info
.PHONY: check check-syntax check-format format lint copy_libs copy_res copy_test_libs all_copy
.PHONY: create_windows_installer create_linux_installer installer
.PHONY: install install_deps i remove_deps reset_deps

# Dependencies
-include $(wildcard $(ALL_OBJECTS:.o=.d))
-include $(wildcard $(TEST_OBJECTS:.o=.d))
