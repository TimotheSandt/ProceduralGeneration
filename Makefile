# Include configuration
-include config.mk

# This Makefile assumes GNU make recipes run in a POSIX shell.
UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)

# Toolchain
CXX ?= g++
CC ?= gcc
RC ?= windres
VCPKG ?= vcpkg

# Directories
INCLUDES_BASE := Libraries/includes
LIBRARIES_SRC_DIR := Libraries/src
LIBRARIES_LIB_DIR := Libraries/libs
MAIN_SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
BUILD_DIR := build
RES_DIR := res
TMP_DEB_DIR := /tmp/proceduralgeneration_deb

# Helpers
MKDIR_P := mkdir -p
RM_F := rm -f
RM_RF := rm -rf
CP_F := cp -f
CP_R := cp -R
MV_F := mv -f
TOUCH := touch
FIND := find

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
	VCPKG_TRIPLET ?= x64-mingw-dynamic
	GLFW_LINK_NAME := glfw3dll
	LDFLAGS = -L$(VCPKG_INSTALLED_DIR)/lib -l$(GLFW_LINK_NAME) -lglad -lfreetype -lpng16 -lzlib -lbz2 -lbrotlidec -lbrotlienc -lbrotlicommon -lpsapi -lwinmm -lgdi32 -lstdc++exp
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
	VCPKG_TRIPLET ?= x64-linux
	GLFW_LINK_NAME := glfw
	LDFLAGS = -lglfw -lGL -lpthread -lX11 -ldl -lm
	COPY_LIBS_TARGETS :=
	CREATE_INSTALLER := create_linux_installer
	ARCHITECTURE := $(ARCHITECTURE_LINUX)
	INSTALLER_FILE := $(PACKAGE)_$(VERSION)_$(ARCHITECTURE).deb
else ifeq ($(DETECTED_OS),Darwin)
	EXE_EXT :=
	VCPKG_TRIPLET ?= x64-osx
	GLFW_LINK_NAME := glfw
	LDFLAGS = -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
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
LOCAL_LIB_DIRS := $(sort $(dir $(shell $(FIND) $(LIBRARIES_LIB_DIR) -type f \( -name "*.a" -o -name "*.lib" -o -name "*.dll" \) 2>/dev/null)))
LOCAL_A_LIBS := $(shell $(FIND) $(LIBRARIES_LIB_DIR) -type f -name "*.a" 2>/dev/null)
LOCAL_IMPORT_LIBS := $(shell $(FIND) $(LIBRARIES_LIB_DIR) -type f -name "*.lib" 2>/dev/null)
LOCAL_DLLS := $(shell $(FIND) $(LIBRARIES_LIB_DIR) -type f -name "*.dll" 2>/dev/null)
LOCAL_LINK_DIR_FLAGS := $(foreach dir,$(LOCAL_LIB_DIRS),-L$(dir))
LOCAL_A_LINK_INPUTS := $(LOCAL_A_LIBS)
LOCAL_IMPORT_LINK_INPUTS := $(LOCAL_IMPORT_LIBS)

# Includes and resources
INCLUDES_DIRS := $(notdir $(wildcard $(INCLUDES_BASE)/*))
INCLUDES := -I$(INCLUDES_BASE) $(foreach dir,$(INCLUDES_DIRS),-I$(INCLUDES_BASE)/$(dir)) -I$(VCPKG_INSTALLED_DIR)/include
ICON_RC := $(RES_DIR)/icon.rc

# Flags
CFLAGS := -m64 -O2 -DNDEBUG
CXXFLAGS := -std=c++23

# Target configuration
BUILD_TYPE := normal
TARGET_NAME := $(PROJECT_NAME)

ifneq ($(findstring debug,$(MAKECMDGOALS)),)
	CFLAGS := -Wall -Wextra -m64 -O1 -g -DDEBUG
	BUILD_TYPE := debug
	TARGET_NAME := main
else ifneq ($(findstring dev,$(MAKECMDGOALS)),)
	CFLAGS := -m64 -g3 -O0 -DDEBUG
	BUILD_TYPE := dev
	TARGET_NAME := main
else ifneq ($(or $(findstring release,$(MAKECMDGOALS)),$(findstring installer,$(MAKECMDGOALS))),)
	CFLAGS := -Wall -Wextra -Werror -m64 -O3 -flto -DNDEBUG -DRELEASE
	CXXFLAGS += -flto=jobserver
endif

CXXFLAGS += $(CFLAGS) -DGLM_ENABLE_EXPERIMENTAL

BIN_DIR_TYPE := $(BIN_DIR)/$(BUILD_TYPE)
OBJ_DIR_TYPE := $(OBJ_DIR)/$(BUILD_TYPE)
TARGET := $(BIN_DIR_TYPE)/$(TARGET_NAME)$(EXE_EXT)

# Source files
LIBRARIES_CPP_SOURCES := $(shell $(FIND) $(LIBRARIES_SRC_DIR) -type f -name "*.cpp" 2>/dev/null)
LIBRARIES_C_SOURCES := $(shell $(FIND) $(LIBRARIES_SRC_DIR) -type f -name "*.c" 2>/dev/null)
MAIN_CPP_SOURCES := $(shell $(FIND) $(MAIN_SRC_DIR) -type f -name "*.cpp" 2>/dev/null)
MAIN_C_SOURCES := $(shell $(FIND) $(MAIN_SRC_DIR) -type f -name "*.c" 2>/dev/null)

LIB_SOURCES := $(LOCAL_DLLS) $(LOCAL_IMPORT_LIBS) $(LOCAL_A_LIBS)
ALL_CPP_SOURCES := $(LIBRARIES_CPP_SOURCES) $(MAIN_CPP_SOURCES)
ALL_C_SOURCES := $(LIBRARIES_C_SOURCES) $(MAIN_C_SOURCES)

# Objects
LIBRARIES_CPP_OBJECTS := $(LIBRARIES_CPP_SOURCES:$(LIBRARIES_SRC_DIR)/%.cpp=$(OBJ_DIR_TYPE)/Libraries/%.o)
LIBRARIES_C_OBJECTS := $(LIBRARIES_C_SOURCES:$(LIBRARIES_SRC_DIR)/%.c=$(OBJ_DIR_TYPE)/Libraries/%.o)
MAIN_CPP_OBJECTS := $(MAIN_CPP_SOURCES:$(MAIN_SRC_DIR)/%.cpp=$(OBJ_DIR_TYPE)/src/%.o)
MAIN_C_OBJECTS := $(MAIN_C_SOURCES:$(MAIN_SRC_DIR)/%.c=$(OBJ_DIR_TYPE)/src/%.o)
ALL_OBJECTS := $(LIBRARIES_CPP_OBJECTS) $(LIBRARIES_C_OBJECTS) $(MAIN_CPP_OBJECTS) $(MAIN_C_OBJECTS)

ifneq ($(strip $(ICON_NAME)),)
ifneq ($(wildcard $(ICON_NAME)),)
ifeq ($(DETECTED_OS),Windows)
	ALL_OBJECTS += $(OBJ_DIR_TYPE)/src/icon.o
endif
endif
endif

COPY_TARGETS := copy_res $(COPY_LIBS_TARGETS)

# Build rules
all: $(TARGET)

debug dev: $(TARGET)

installer: $(CREATE_INSTALLER)

release: $(TARGET) installer
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
	@$(MV_F) "installers/windows/$(INSTALLER_FILE)" "$(BUILD_DIR)/"
	@echo "Windows installer created: $(BUILD_DIR)/$(INSTALLER_FILE)"

# Linux installer
create_linux_installer: | $(BUILD_DIR)
	@$(RM_RF) "$(TMP_DEB_DIR)"
	@echo "Creating Linux .deb package..."
	@$(MKDIR_P) "$(TMP_DEB_DIR)/usr/bin" "$(TMP_DEB_DIR)/usr/share/proceduralgeneration" "$(TMP_DEB_DIR)/DEBIAN"
	@$(TOUCH) "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Package: $(PACKAGE)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Version: $(VERSION)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Maintainer: $(MAINTAINER)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Section: $(SECTION)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Priority: $(PRIORITY)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Architecture: $(ARCHITECTURE)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Depends: $(DEPENDS)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@echo 'Description: $(PROJECT_DESCRIPTION)' >> "$(TMP_DEB_DIR)/DEBIAN/control"
	@$(CP_F) installers/linux/DEBIAN/postinst "$(TMP_DEB_DIR)/DEBIAN/"
	@$(CP_F) installers/linux/DEBIAN/postrm "$(TMP_DEB_DIR)/DEBIAN/"
	@chmod 755 "$(TMP_DEB_DIR)/DEBIAN" "$(TMP_DEB_DIR)/DEBIAN/postinst" "$(TMP_DEB_DIR)/DEBIAN/postrm"
	@chmod 644 "$(TMP_DEB_DIR)/DEBIAN/control"
	@$(CP_F) "bin/$(BUILD_TYPE)/$(PROJECT_NAME)" "$(TMP_DEB_DIR)/usr/bin/proceduralgeneration"
	@$(CP_R) "bin/$(BUILD_TYPE)/res" "$(TMP_DEB_DIR)/usr/share/proceduralgeneration/"
	@set -- "bin/$(BUILD_TYPE)"/*.so*; if [ -e "$$1" ]; then $(CP_F) "$$@" "$(TMP_DEB_DIR)/usr/share/proceduralgeneration/"; fi
	@set -- "bin/$(BUILD_TYPE)"/*.dylib; if [ -e "$$1" ]; then $(CP_F) "$$@" "$(TMP_DEB_DIR)/usr/share/proceduralgeneration/"; fi
	@dpkg-deb --build "$(TMP_DEB_DIR)" "$(BUILD_DIR)/$(INSTALLER_FILE)"
	@$(RM_RF) "$(TMP_DEB_DIR)"
	@echo "Linux .deb created: $(BUILD_DIR)/$(INSTALLER_FILE)"

# Dependencies via vcpkg
install_deps:
	@echo "Checking and installing dependencies with vcpkg..."
	$(VCPKG) install --triplet=$(VCPKG_TRIPLET) --x-install-root=$(VCPKG_INSTALLED_ROOT)

remove_deps:
	@echo "Removing dependencies with vcpkg..."
	@$(RM_RF) ./vcpkg_installed

reset_deps:
	@echo "Resetting dependencies with vcpkg..."
	@$(RM_RF) $(VCPKG_INSTALLED_ROOT)
	$(VCPKG) install --triplet=$(VCPKG_TRIPLET) --x-install-root=$(VCPKG_INSTALLED_ROOT)

# Icon resource
$(OBJ_DIR_TYPE)/src/icon.o: $(BIN_DIR_TYPE)/$(ICON_RC)
	@$(MKDIR_P) "$(dir $@)"
	$(RC) -i $< -o $@

$(BIN_DIR_TYPE)/$(ICON_RC): $(ICON_NAME) | $(BIN_DIR_TYPE)
	@$(MKDIR_P) "$(dir $@)"
	@printf '1 ICON "%s"\n' "$(ICON_NAME)" > "$@"

# Asset copy
copy_libs: | $(BIN_DIR_TYPE)
	@echo "Copying libraries to $(BIN_DIR_TYPE)"
	@set -- "$(VCPKG_BIN_DIR)"/*.dll; if [ -e "$$1" ]; then $(CP_F) "$$@" "$(BIN_DIR_TYPE)/" 2>/dev/null || :; fi
	@set -- $(LOCAL_DLLS); if [ -n "$(strip $(LOCAL_DLLS))" ] && [ -e "$$1" ]; then $(CP_F) "$$@" "$(BIN_DIR_TYPE)/" 2>/dev/null || :; fi
	@set -- $(LOCAL_IMPORT_LIBS); if [ -n "$(strip $(LOCAL_IMPORT_LIBS))" ] && [ -e "$$1" ]; then $(CP_F) "$$@" "$(BIN_DIR_TYPE)/" 2>/dev/null || :; fi
	@set -- $(LOCAL_A_LIBS); if [ -n "$(strip $(LOCAL_A_LIBS))" ] && [ -e "$$1" ]; then $(CP_F) "$$@" "$(BIN_DIR_TYPE)/" 2>/dev/null || :; fi

copy_res: | $(BIN_DIR_TYPE)
	@echo "Copying resources to $(BIN_DIR_TYPE)"
	@if [ -d "$(RES_DIR)" ]; then \
		$(MKDIR_P) "$(BIN_DIR_TYPE)/$(RES_DIR)" && \
		cp -Rf "$(RES_DIR)/." "$(BIN_DIR_TYPE)/$(RES_DIR)/" 2>/dev/null || :; \
	fi

all_copy: $(COPY_TARGETS)

# Build target
$(TARGET): all_copy $(ALL_OBJECTS) | $(BIN_DIR_TYPE)
	$(CXX) $(CXXFLAGS) $(ALL_OBJECTS) $(LOCAL_LINK_DIR_FLAGS) $(LOCAL_A_LINK_INPUTS) $(LOCAL_IMPORT_LINK_INPUTS) $(LDFLAGS) -o $@
	@echo "Compilation successful for: $(TARGET)"

# Compilation rules
$(OBJ_DIR_TYPE)/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.cpp
	@$(MKDIR_P) "$(dir $@)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Libraries) $(BUILD_TYPE): $<"

$(OBJ_DIR_TYPE)/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.c
	@$(MKDIR_P) "$(dir $@)"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Libraries) $(BUILD_TYPE): $<"

$(OBJ_DIR_TYPE)/src/%.o: $(MAIN_SRC_DIR)/%.cpp
	@$(MKDIR_P) "$(dir $@)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Main) $(BUILD_TYPE): $<"

$(OBJ_DIR_TYPE)/src/%.o: $(MAIN_SRC_DIR)/%.c
	@$(MKDIR_P) "$(dir $@)"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Main) $(BUILD_TYPE): $<"

# Directories
$(BIN_DIR_TYPE):
	@echo "Creating $@ directory"
	@$(MKDIR_P) "$@"

$(BUILD_DIR):
	@$(MKDIR_P) "$@"

# Clean rules
clean:
	@$(RM_RF) $(OBJ_DIR)
	@$(RM_F) installers/windows/*.exe
	@$(RM_F) installers/linux/*.deb
	@echo "Objects deleted"

fclean: clean
	@$(RM_RF) $(BIN_DIR)
	@echo "Executables deleted"

fclean-build: fclean
	@$(RM_RF) $(BUILD_DIR)

re: fclean all
re-debug: fclean debug
re-dev: fclean dev
re-release: fclean release

# Check syntax
check:
	@echo "Checking C++ syntax..."
	@if [ -n "$(strip $(ALL_CPP_SOURCES))" ]; then $(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only $(ALL_CPP_SOURCES); fi
	@echo "Checking C syntax..."
	@if [ -n "$(strip $(ALL_C_SOURCES))" ]; then $(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only $(ALL_C_SOURCES); fi

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
	@echo "Dependencies: sudo apt install libglfw3-dev libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libstb-dev"
else ifeq ($(DETECTED_OS),Darwin)
	@echo "Dependencies: brew install glfw glm"
else ifeq ($(DETECTED_OS),Windows)
	@echo "Dependencies: use MinGW/clang with a POSIX shell and install libraries via vcpkg"
	@echo "LIBS: $(notdir $(LIB_SOURCES))"
else
	@echo "Unknown OS"
endif

# Phony rules
.PHONY: all release dev debug
.PHONY: run run-release run-dev run-debug
.PHONY: clean fclean fclean-build re re-debug re-dev re-release
.PHONY: info info-debug info-dev info-release debug-info dev-info release-info
.PHONY: check copy_libs copy_res all_copy
.PHONY: create_windows_installer create_linux_installer installer
.PHONY: install_deps remove_deps reset_deps

# Dependencies
-include $(wildcard $(ALL_OBJECTS:.o=.d))
