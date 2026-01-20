# Include configuration
-include config.mk

# Variables
CXX = g++
CC = gcc

# OS detection and shell setup
ifeq ($(OS),Windows_NT)
	DETECTED_OS := Windows
	RM = cmd /C del /Q /F
	RMDIR = cmd /C rmdir /S /Q
	MKDIR = cmd /C mkdir
	CP = cmd /C copy /Y
	CPDIR = cmd /C xcopy /E /I /Y
	MV = cmd /C move
	SHELL_TYPE = windows
	PATH_SEP = \\
	EXE_EXT = .exe

	# Vcpkg Configuration
	VCPKG_TRIPLET ?= x64-mingw-static
	VCPKG_ROOT := ./vcpkg_installed/$(VCPKG_TRIPLET)
else
	DETECTED_OS := $(shell uname -s)
	RM = rm -f
	RMDIR = rm -rf
	MKDIR = mkdir -p
	CP = cp
	CPDIR = cp -r
	MV = mv
	SHELL_TYPE = unix
	PATH_SEP = /
	EXE_EXT =

	# Vcpkg Configuration
	VCPKG_TRIPLET ?= x64-linux
	VCPKG_ROOT := ./vcpkg_installed/$(VCPKG_TRIPLET)
endif

# Platform-specific linker flags
ifeq ($(DETECTED_OS),Linux)
	LDFLAGS = -lglfw -lGL -lpthread -lX11 -ldl -lm
	COPY_LIBS =
else ifeq ($(DETECTED_OS),Darwin)
	LDFLAGS = -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	COPY_LIBS =
else ifeq ($(DETECTED_OS),Windows)
	# Vcpkg libs
	LDFLAGS = -L$(VCPKG_ROOT)/lib -lglfw3 -lglad -lpsapi -lwinmm -lgdi32
	COPY_LIBS = copy_libs
else
	LDFLAGS =
	COPY_LIBS =
endif

# Installer creation for release
ifeq ($(DETECTED_OS),Windows)
	CREATE_INSTALLER = create_windows_installer
	ARCHITECTURE = $(ARCHITECTURE_WINDOWS)
	INSTALLER_FILE = $(PROJECT_NAME)-$(VERSION)-$(ARCHITECTURE)-setup.exe

	# Detect makensis
	ifneq ($(wildcard C:/Program\ Files\ (x86)/NSIS/makensis.exe),)
		NSIS_COMPILER = "C:\Program Files (x86)\NSIS\makensis.exe"
	else ifeq ($(wildcard C:/Program Files/NSIS/makensis.exe),)
		NSIS_COMPILER = "C:/Program Files/NSIS/makensis.exe"
	else
		NSIS_COMPILER = makensis
	endif
else ifeq ($(DETECTED_OS),Linux)
	CREATE_INSTALLER = create_linux_installer
	ARCHITECTURE = $(ARCHITECTURE_LINUX)
	INSTALLER_FILE = $(PACKAGE)_$(VERSION)_$(ARCHITECTURE).deb
else
	CREATE_INSTALLER =
	INSTALLER_FILE =
endif

# Includes
INCLUDES_BASE = Libraries/includes
INCLUDES_DIRS := $(notdir $(wildcard $(INCLUDES_BASE)/*))
INCLUDES := -I$(INCLUDES_BASE) $(foreach dir,$(INCLUDES_DIRS),-I$(INCLUDES_BASE)/$(dir)) -I$(VCPKG_ROOT)/include

# Directories
LIBRARIES_SRC_DIR = Libraries/src
LIBRARIES_LIB_DIR = Libraries/libs
MAIN_SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
BUILD_DIR = build
RES_DIR = res

# Icon
RC = windres
ICON_RC = $(RES_DIR)/icon.rc

# Flags
CFLAGS = -Wall -Wextra -Werror -m64 -O2 -DNDEBUG

# Target Executable
BUILD_TYPE = normal
TARGET_NAME = $(PROJECT_NAME)

CXXFLAGS = -std=c++23

ifneq ($(findstring debug,$(MAKECMDGOALS)),)
	CFLAGS := -Wall -Wextra -m64 -O1 -DDEBUG
	BUILD_TYPE = debug
	TARGET_NAME = main
else ifneq ($(findstring dev,$(MAKECMDGOALS)),)
	CFLAGS := -Wall -Wextra -m64 -g3 -O0 -DDEBUG
	BUILD_TYPE = dev
	TARGET_NAME = main
else ifneq ($(or $(findstring release,$(MAKECMDGOALS)),$(findstring installer,$(MAKECMDGOALS))),)
	CFLAGS := -Wall -Wextra -Werror -m64 -O3 -flto -DNDEBUG -DRELEASE
	BUILD_TYPE = release
	CXXFLAGS += -flto=jobserver
endif

CXXFLAGS += $(CFLAGS) -DGLM_ENABLE_EXPERIMENTAL

BIN_DIR_TYPE = $(BIN_DIR)/$(BUILD_TYPE)
OBJ_DIR_TYPE = $(OBJ_DIR)/$(BUILD_TYPE)

TARGET = $(BIN_DIR_TYPE)/$(TARGET_NAME)$(EXE_EXT)

# Source Files - use shell find on Unix, forfiles on Windows
ifeq ($(SHELL_TYPE),windows)
	# Windows: Use wildcard with subst for relative paths
	rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
	LIBRARIES_CPP_SOURCES = $(call rwildcard,$(LIBRARIES_SRC_DIR),*.cpp)
	LIBRARIES_C_SOURCES = $(call rwildcard,$(LIBRARIES_SRC_DIR),*.c)
	LIBRARIES_DLL_SOURCES = $(call rwildcard,$(LIBRARIES_LIB_DIR),*.dll)
	LIBRARIES_LIB_SOURCES = $(call rwildcard,$(LIBRARIES_LIB_DIR),*.lib)
	MAIN_CPP_SOURCES = $(call rwildcard,$(MAIN_SRC_DIR),*.cpp)
	MAIN_C_SOURCES = $(call rwildcard,$(MAIN_SRC_DIR),*.c)
else
	# Unix: Use find
	LIBRARIES_CPP_SOURCES = $(shell find $(LIBRARIES_SRC_DIR) -name "*.cpp" -type f 2>/dev/null)
	LIBRARIES_C_SOURCES = $(shell find $(LIBRARIES_SRC_DIR) -name "*.c" -type f 2>/dev/null)
	LIBRARIES_DLL_SOURCES = $(shell find $(LIBRARIES_LIB_DIR) -name "*.dll" -type f 2>/dev/null)
	LIBRARIES_LIB_SOURCES = $(shell find $(LIBRARIES_LIB_DIR) -name "*.lib" -type f 2>/dev/null)
	MAIN_CPP_SOURCES = $(shell find $(MAIN_SRC_DIR) -name "*.cpp" -type f 2>/dev/null)
	MAIN_C_SOURCES = $(shell find $(MAIN_SRC_DIR) -name "*.c" -type f 2>/dev/null)
endif

LIB_SOURCES = $(LIBRARIES_DLL_SOURCES) $(LIBRARIES_LIB_SOURCES)

# Combines all source files
ALL_CPP_SOURCES = $(LIBRARIES_CPP_SOURCES) $(MAIN_CPP_SOURCES)
ALL_C_SOURCES = $(LIBRARIES_C_SOURCES) $(MAIN_C_SOURCES)

# Build objects
LIBRARIES_CPP_OBJECTS = $(LIBRARIES_CPP_SOURCES:$(LIBRARIES_SRC_DIR)/%.cpp=$(OBJ_DIR_TYPE)/Libraries/%.o)
LIBRARIES_C_OBJECTS = $(LIBRARIES_C_SOURCES:$(LIBRARIES_SRC_DIR)/%.c=$(OBJ_DIR_TYPE)/Libraries/%.o)
MAIN_CPP_OBJECTS = $(MAIN_CPP_SOURCES:$(MAIN_SRC_DIR)/%.cpp=$(OBJ_DIR_TYPE)/src/%.o)
MAIN_C_OBJECTS = $(MAIN_C_SOURCES:$(MAIN_SRC_DIR)/%.c=$(OBJ_DIR_TYPE)/src/%.o)
ALL_OBJECTS = $(LIBRARIES_CPP_OBJECTS) $(LIBRARIES_C_OBJECTS) $(MAIN_CPP_OBJECTS) $(MAIN_C_OBJECTS)

ifeq ($(DETECTED_OS),Windows)
	ifneq ("$(wildcard ${ICON_NAME})", "")
		LIBRARIES_CPP_OBJECTS += $(OBJ_DIR_TYPE)/src/icon.o
	endif
endif

# Build Rules
all: $(TARGET)

debug dev: $(TARGET)

installer: $(CREATE_INSTALLER)

release: $(TARGET) installer
	@echo "Release build complete"

# Execution Rules
run run-dev run-debug: $(TARGET)
ifeq ($(SHELL_TYPE),windows)
	$(TARGET)
else
	./$(TARGET)
endif

run-release: $(TARGET) installer
ifeq ($(SHELL_TYPE),windows)
	$(TARGET)
else
	./$(TARGET)
endif

create_windows_installer:
	@echo "Creating Windows installer..."
	@echo Installer File: $(INSTALLER_FILE)
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
	@if not exist "installers\windows\$(INSTALLER_FILE)" ( \
		echo Installer file was not generated! && \
		exit /b 1 \
	)
	@$(MV) "installers\windows\$(INSTALLER_FILE)" "$(BUILD_DIR)"
	@echo Windows installer created: $(BUILD_DIR)/$(INSTALLER_FILE)

create_linux_installer:
	@rm -rf /tmp/proceduralgeneration_deb
	@echo "Creating Linux .deb package..."
	@mkdir -p /tmp/proceduralgeneration_deb/usr/bin /tmp/proceduralgeneration_deb/usr/share/proceduralgeneration
	@mkdir -p /tmp/proceduralgeneration_deb/DEBIAN
	@touch /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Package: $(PACKAGE)' >> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Version: $(VERSION)' >> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Maintainer: $(MAINTAINER)' >> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Section: $(SECTION)' >> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Priority: $(PRIORITY)' >> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Architecture: $(ARCHITECTURE)' >> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Depends: $(DEPENDS)' >> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Description: $(PROJECT_DESCRIPTION)' >> /tmp/proceduralgeneration_deb/DEBIAN/control
	@cp installers/linux/DEBIAN/postinst /tmp/proceduralgeneration_deb/DEBIAN/
	@cp installers/linux/DEBIAN/postrm /tmp/proceduralgeneration_deb/DEBIAN/
	@chmod 755 /tmp/proceduralgeneration_deb/DEBIAN
	@chmod 644 /tmp/proceduralgeneration_deb/DEBIAN/control
	@chmod 755 /tmp/proceduralgeneration_deb/DEBIAN/postinst /tmp/proceduralgeneration_deb/DEBIAN/postrm
	@cp bin/${BUILD_TYPE}/$(PROJECT_NAME) /tmp/proceduralgeneration_deb/usr/bin/proceduralgeneration
	@cp -r bin/${BUILD_TYPE}/res /tmp/proceduralgeneration_deb/usr/share/proceduralgeneration/
	@dpkg-deb --build /tmp/proceduralgeneration_deb/ $(BUILD_DIR)/$(INSTALLER_FILE)
	@rm -rf /tmp/proceduralgeneration_deb
	@echo "Linux .deb created: $(BUILD_DIR)/$(INSTALLER_FILE)"

# Règle pour installer les dépendances via vcpkg
install_deps:
	@echo "Vérification et installation des dépendances avec vcpkg..."
	vcpkg install --triplet=$(VCPKG_TRIPLET) --x-install-root=./vcpkg_installed

$(OBJ_DIR_TYPE)/src/icon.o: $(BIN_DIR_TYPE)/$(ICON_RC)
	$(RC) -i $< -o $@

$(BIN_DIR_TYPE)/$(ICON_RC): $(ICON_NAME)
ifeq ($(SHELL_TYPE),windows)
	@echo 1 ICON "$(ICON_NAME)" > $@
else
	@echo "1 ICON \"$(ICON_NAME)\"" > $@
endif

# Copy libs
copy_libs: | $(BIN_DIR_TYPE)
	@echo "Copying libraries to $(BIN_DIR_TYPE)"
ifeq ($(SHELL_TYPE),windows)
	@if exist "$(subst /,\,$(VCPKG_ROOT))\bin\*.dll" copy "$(subst /,\,$(VCPKG_ROOT))\bin\*.dll" "$(subst /,\,$(BIN_DIR_TYPE))" >nul 2>nul
else
	@cp $(VCPKG_ROOT)/bin/*.dll $(BIN_DIR_TYPE) 2>/dev/null || :
endif

copy_res: | $(BIN_DIR_TYPE)
	@echo "Copying resources to $(BIN_DIR_TYPE)"
ifeq ($(SHELL_TYPE),windows)
	@if exist "$(subst /,\,$(RES_DIR))" xcopy "$(subst /,\,$(RES_DIR))" "$(subst /,\,$(BIN_DIR_TYPE))\res" /E /I /Y >nul
else
	@cp -r $(RES_DIR) $(BIN_DIR_TYPE)
	@cp -r $(RES_DIR)/* $(BIN_DIR_TYPE)
endif

ifeq ($(DETECTED_OS),Windows)
all_copy: $(COPY_LIBS) copy_res
else
all_copy: copy_res
endif

# Build all objects
$(TARGET): all_copy $(ALL_OBJECTS) | $(BIN_DIR_TYPE)
	$(CXX) $(CXXFLAGS) $(ALL_OBJECTS) $(LDFLAGS) -o $@
	@echo "Compilation successful for: $(TARGET)"

$(ALL_OBJECTS): | install_deps

$(COPY_LIBS): | install_deps

# Files Compilation
$(OBJ_DIR_TYPE)/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.cpp | $(OBJ_DIR)/${BUILD_TYPE}
ifeq ($(SHELL_TYPE),windows)
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))" 2>nul || cd .
else
	@mkdir -p $(dir $@)
endif
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Libraries) $(BUILD_TYPE): $<"

$(OBJ_DIR_TYPE)/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.c | $(OBJ_DIR)/${BUILD_TYPE}
ifeq ($(SHELL_TYPE),windows)
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))" 2>nul || cd .
else
	@mkdir -p $(dir $@)
endif
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Libraries) $(BUILD_TYPE): $<"

$(OBJ_DIR_TYPE)/src/%.o: $(MAIN_SRC_DIR)/%.cpp | $(OBJ_DIR)/${BUILD_TYPE}
ifeq ($(SHELL_TYPE),windows)
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))" 2>nul || cd .
else
	@mkdir -p $(dir $@)
endif
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Main) $(BUILD_TYPE): $<"

$(OBJ_DIR_TYPE)/src/%.o: $(MAIN_SRC_DIR)/%.c | $(OBJ_DIR)/${BUILD_TYPE}
ifeq ($(SHELL_TYPE),windows)
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))" 2>nul || cd .
else
	@mkdir -p $(dir $@)
endif
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Main) $(BUILD_TYPE): $<"

# Create Directories
$(OBJ_DIR)/${BUILD_TYPE}:
ifeq ($(SHELL_TYPE),windows)
	@if not exist "$(subst /,\,$@\Libraries\Game)" mkdir "$(subst /,\,$@\Libraries\Game)"
	@if not exist "$(subst /,\,$@\Libraries\Graphics)" mkdir "$(subst /,\,$@\Libraries\Graphics)"
	@if not exist "$(subst /,\,$@\Libraries\Profiler)" mkdir "$(subst /,\,$@\Libraries\Profiler)"
	@if not exist "$(subst /,\,$@\Libraries\ThirdParty)" mkdir "$(subst /,\,$@\Libraries\ThirdParty)"
	@if not exist "$(subst /,\,$@\src)" mkdir "$(subst /,\,$@\src)"
else
	@mkdir -p $@/Libraries/Game $@/Libraries/Graphics $@/Libraries/Profiler $@/Libraries/ThirdParty $@/src
endif

$(BIN_DIR)/${BUILD_TYPE}:
	@echo "Creating $@ directory"
ifeq ($(SHELL_TYPE),windows)
	@if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
	@mkdir -p $@
endif

$(BUILD_DIR):
ifeq ($(SHELL_TYPE),windows)
	@if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
	@mkdir -p $@
endif

# Clean Rules
clean:
ifeq ($(SHELL_TYPE),windows)
	@if exist "$(subst /,\,$(OBJ_DIR))" rmdir /S /Q "$(subst /,\,$(OBJ_DIR))"
	@if exist "installers\windows\*.exe" del /Q "installers\windows\*.exe"
	@if exist "installers\linux\*.deb" del /Q "installers\linux\*.deb"
else
	@rm -rf $(OBJ_DIR)
	@rm -f installers/windows/*.exe
	@rm -f installers/linux/*.deb
endif
	@echo "Objects deleted"

fclean: clean
ifeq ($(SHELL_TYPE),windows)
	@if exist "$(subst /,\,$(BIN_DIR))" rmdir /S /Q "$(subst /,\,$(BIN_DIR))"
else
	@rm -rf $(BIN_DIR)
endif
	@echo "Executables deleted"

fclean-build: fclean
ifeq ($(SHELL_TYPE),windows)
	@if exist "$(subst /,\,$(BUILD_DIR))" rmdir /S /Q "$(subst /,\,$(BUILD_DIR))"
else
	@rm -rf $(BUILD_DIR)
endif

re: fclean all
re-debug: fclean debug
re-dev: fclean dev
re-release: fclean release

# Check
check:
	@echo "Checking C++ syntax..."
	@if [ "$(ALL_CPP_SOURCES)" != "" ]; then $(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only $(ALL_CPP_SOURCES); fi
	@echo "Checking C syntax..."
	@if [ "$(ALL_C_SOURCES)" != "" ]; then $(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only $(ALL_C_SOURCES); fi

# Info
debug-info info-debug: info
dev-info info-dev: info
release-info info-release: info
info:
	@echo "Project: $(PROJECT_NAME)"
	@echo "Detected OS: $(DETECTED_OS)"
	@echo "Shell Type: $(SHELL_TYPE)"
	@echo "Structure:"
	@echo "  - Libraries/src/ : $(words $(LIBRARIES_CPP_SOURCES)) C++ files, $(words $(LIBRARIES_C_SOURCES)) C files"
	@echo "  - src/ : $(words $(MAIN_CPP_SOURCES)) C++ files, $(words $(MAIN_C_SOURCES)) C files"
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
	@echo "Dependencies: Use bundled libs or install via vcpkg: vcpkg install glfw3 stb-image"
	@echo "LIBS: $(notdir $(LIB_SOURCES))"
else
	@echo "Unknown OS"
endif

# Phony Rules
.PHONY: all release dev debug
.PHONY: run run-release run-dev run-debug
.PHONY: clean fclean fclean-build re re-debug re-dev re-release
.PHONY: info info-debug info-dev info-release debug-info dev-info release-info
.PHONY: check copy_libs copy_res all_copy
.PHONY: create_windows_installer create_linux_installer installer install_deps

# Dependencies (only include if they exist)
-include $(wildcard $(ALL_OBJECTS:.o=.d))
