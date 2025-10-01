# Include configuration
-include config.mk

# Variables
CXX = g++
CC = gcc

# OS detection
OS := $(shell uname -s)
ifeq ($(OS),Linux)
	LDFLAGS = -lglfw -lGL -lpthread -lX11 -ldl -lm -lstb
	COPY_LIBS =
else ifeq ($(OS),Darwin)
	LDFLAGS = -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	COPY_LIBS =
else ifeq ($(OS),Windows_NT)
	LDFLAGS = -LLibraries/libs/ThirdParty/ -lglfw3dll -lstb_image -lpsapi -lwinmm
	COPY_LIBS = copy_libs
else
	LDFLAGS =
	COPY_LIBS =
endif

# Installer creation for release
ifeq ($(OS),Windows_NT)
CREATE_INSTALLER = create_windows_installer
ARCHITECTURE = $(ARCHITECTURE_WINDOWS)
INSTALLER_FILE = $(PROJECT_NAME)-$(VERSION)-$(ARCHITECTURE)-setup.exe
else ifeq ($(OS),Linux)
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
INCLUDES := -I$(INCLUDES_BASE) $(foreach dir,$(INCLUDES_DIRS),-I$(INCLUDES_BASE)/$(dir))

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
else ifneq ($(findstring release,$(MAKECMDGOALS)),)
CFLAGS := -Wall -Wextra -Werror -m64 -O3 -flto -DNDEBUG -DRELEASE
BUILD_TYPE = release
CXXFLAGS += -flto=jobserver
endif

CXXFLAGS += $(CFLAGS) 

BIN_DIR_TYPE = $(BIN_DIR)/$(BUILD_TYPE)
OBJ_DIR_TYPE = $(OBJ_DIR)/$(BUILD_TYPE)

TARGET = $(BIN_DIR_TYPE)/$(TARGET_NAME)



# Source Files

LIBRARIES_CPP_SOURCES = $(shell find $(LIBRARIES_SRC_DIR) -name "*.cpp" -type f)
LIBRARIES_C_SOURCES = $(shell find $(LIBRARIES_SRC_DIR) -name "*.c" -type f)

LIBRARIES_DLL_SOURCES = $(shell find $(LIBRARIES_LIB_DIR) -name "*.dll" -type f)
LIBRARIES_LIB_SOURCES = $(shell find $(LIBRARIES_LIB_DIR) -name "*.lib" -type f)
LIB_SOURCES = $(LIBRARIES_DLL_SOURCES) $(LIBRARIES_LIB_SOURCES)

MAIN_CPP_SOURCES = $(shell find $(MAIN_SRC_DIR) -name "*.cpp" -type f)
MAIN_C_SOURCES = $(shell find $(MAIN_SRC_DIR) -name "*.c" -type f)

# Combines all source files into single variables for convenience.
ALL_CPP_SOURCES = $(LIBRARIES_CPP_SOURCES) $(MAIN_CPP_SOURCES)
ALL_C_SOURCES = $(LIBRARIES_C_SOURCES) $(MAIN_C_SOURCES)

# --- Build Objects ---
# Build objects
LIBRARIES_CPP_OBJECTS = $(LIBRARIES_CPP_SOURCES:$(LIBRARIES_SRC_DIR)/%.cpp=$(OBJ_DIR_TYPE)/Libraries/%.o)
LIBRARIES_C_OBJECTS = $(LIBRARIES_C_SOURCES:$(LIBRARIES_SRC_DIR)/%.c=$(OBJ_DIR_TYPE)/Libraries/%.o)
MAIN_CPP_OBJECTS = $(MAIN_CPP_SOURCES:$(MAIN_SRC_DIR)/%.cpp=$(OBJ_DIR_TYPE)/src/%.o)
MAIN_C_OBJECTS = $(MAIN_C_SOURCES:$(MAIN_SRC_DIR)/%.c=$(OBJ_DIR_TYPE)/src/%.o)
ALL_OBJECTS = $(LIBRARIES_CPP_OBJECTS) $(LIBRARIES_C_OBJECTS) $(MAIN_CPP_OBJECTS) $(MAIN_C_OBJECTS)


ifneq ("$(wildcard ${ICON_NAME})", "")
LIBRARIES_CPP_OBJECTS += $(OBJ_DIR_TYPE)/src/icon.o
endif

# Build Rules
.PHONY: all debug dev release

all debug dev: $(TARGET)
release: $(TARGET) $(CREATE_INSTALLER)
	@echo "Release build complete"

# Execution Rules
run run-dev run-debug: $(TARGET)
	./$(TARGET)
run-release: $(TARGET) $(CREATE_INSTALLER)
	./$(TARGET)
	

create_windows_installer: $(TARGET)
	@echo "Creating Windows installer..."

	@if [ ! -f "installers/windows/installer.nsi" ]; then \
		echo "NSIS script not found at installers/windows/installer.nsi"; \
		echo "Please install NSIS and ensure makensis is in PATH."; \
		exit 1; \
	fi

	@echo "Installer File: $(INSTALLER_FILE)"
	@cd installers/windows && \
	makensis \
		/DPRODUCT_NAME="$(PROJECT_NAME)" \
		/DVERSION="$(VERSION)" \
		/DPUBLISHER="$(PUBLISHER)" \
		/DMANTAINER="$(MAINTAINER)" \
		/DARCH="$(ARCHITECTURE)" \
		/DICON_NAME="$(ICON_NAME)" \
		/DOUTPUT_FILE="$(INSTALLER_FILE)" \
		/DPROJECT_DESCRIPTION="$(PROJECT_DESCRIPTION)" \
		installer.nsi || { \
			echo "makensis failed"; exit 1; \
		}

	@if [ ! -f "installers/windows/$(INSTALLER_FILE)" ]; then \
		echo "Installer file was not generated!"; \
		exit 1; \
	fi

	@mv installers/windows/$(INSTALLER_FILE) $(BUILD_DIR)/
	@echo "Windows installer created: $(BUILD_DIR)/$(INSTALLER_FILE)"


create_linux_installer: $(TARGET)
	@rm -rf /tmp/proceduralgeneration_deb
	@echo "Creating Linux .deb package..."
	@mkdir -p /tmp/proceduralgeneration_deb/usr/bin /tmp/proceduralgeneration_deb/usr/share/proceduralgeneration
	@mkdir -p /tmp/proceduralgeneration_deb/DEBIAN

	@touch /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Package: $(PACKAGE)' 				>> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Version: $(VERSION)' 				>> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Maintainer: $(MAINTAINER)' 			>> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Section: $(SECTION)' 				>> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Priority: $(PRIORITY)' 				>> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Architecture: $(ARCHITECTURE)' 		>> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Depends: $(DEPENDS)' 				>> /tmp/proceduralgeneration_deb/DEBIAN/control
	@echo 'Description: $(PROJECT_DESCRIPTION)' >> /tmp/proceduralgeneration_deb/DEBIAN/control
	
	@cp installers/linux/DEBIAN/postinst /tmp/proceduralgeneration_deb/DEBIAN/
	@cp installers/linux/DEBIAN/postrm /tmp/proceduralgeneration_deb/DEBIAN/
	@chmod 755 /tmp/proceduralgeneration_deb/DEBIAN
	@chmod 644 /tmp/proceduralgeneration_deb/DEBIAN/control
	@chmod 755 /tmp/proceduralgeneration_deb/DEBIAN/postinst /tmp/proceduralgeneration_deb/DEBIAN/postrm
	@cp bin/${BUILD_TYPE}/$(PROJECT_NAME) /tmp/proceduralgeneration_deb/usr/bin/proceduralgeneration
	@cp -r bin/${BUILD_TYPE}/res /tmp/proceduralgeneration_deb/usr/share/proceduralgeneration/
	@echo "Package created: /tmp/proceduralgeneration_deb"
	@dpkg-deb --build /tmp/proceduralgeneration_deb/ $(BUILD_DIR)/$(INSTALLER_FILE)
	@rm -rf /tmp/proceduralgeneration_deb
	@echo "Linux .deb created: bin/${BUILD_TYPE}/$(INSTALLER_FILE)"

$(OBJ_DIR_TYPE)/src/icon.o: $(BIN_DIR_TYPE)/$(ICON_RC)
	$(RC) -i $< -o $@

$(BIN_DIR_TYPE)/$(ICON_RC): $(ICON_NAME)
	@echo "1 ICON \"$(ICON_NAME)\"" > $@


# Copy libs
copy_libs: | $(BIN_DIR_TYPE)
	@echo "Copying libraries to $(BIN_DIR_TYPE)"
	@cp $(LIBRARIES_DLL_SOURCES) $(BIN_DIR_TYPE)
	@cp $(LIBRARIES_LIB_SOURCES) $(BIN_DIR_TYPE)


copy_res: | $(BIN_DIR_TYPE)
	@echo "Copying resources to $(BIN_DIR_TYPE)"
	@cp -r $(RES_DIR) $(BIN_DIR_TYPE)
	@cp -r $(RES_DIR)/* $(BIN_DIR_TYPE)

ifeq ($(OS),Windows_NT)
all_copy: $(COPY_LIBS) copy_res
else
all_copy: copy_res
endif

# Build all objects
$(TARGET): all_copy $(ALL_OBJECTS) | $(BIN_DIR_TYPE)
	$(CXX) $(CXXFLAGS) $(ALL_OBJECTS) $(LDFLAGS) -o $@
	@echo "Compilation successful for: $(TARGET)"


# Files Compilation
# C++ source files
$(OBJ_DIR_TYPE)/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.cpp | $(OBJ_DIR)/${BUILD_TYPE}
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Libraries) $(BUILD_TYPE): $<"

# C source files
$(OBJ_DIR_TYPE)/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.c | $(OBJ_DIR)/${BUILD_TYPE}
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Libraries) $(BUILD_TYPE): $<"

# C++ main source files
$(OBJ_DIR_TYPE)/src/%.o: $(MAIN_SRC_DIR)/%.cpp | $(OBJ_DIR)/${BUILD_TYPE}
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Main) $(BUILD_TYPE): $<"

# C main source files
$(OBJ_DIR_TYPE)/src/%.o: $(MAIN_SRC_DIR)/%.c | $(OBJ_DIR)/${BUILD_TYPE}
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Main) $(BUILD_TYPE): $<"


# Create Directories
$(OBJ_DIR)/${BUILD_TYPE}:
	mkdir -p $@/Libraries/Game $@/Libraries/Graphics $@/Libraries/Profiler $@/Libraries/ThirdParty $@/src


$(BIN_DIR)/${BUILD_TYPE}:
	@echo "Creating $@ directory"
	mkdir -p $@

$(BUILD_DIR):
	mkdir -p $@


.PHONY: clean fclean re
# Clean Rules
clean:
	rm -rf $(OBJ_DIR)
	rm -f installers/windows/*.exe
	rm -f installers/linux/*.deb
	@echo "Objects deleted"

fclean: clean
	rm -rf $(BIN_DIR)
	@echo "Executables deleted"

fclean-build: fclean
	rm -rf $(BUILD_DIR)

re: fclean all
re-debug: fclean debug
re-dev: fclean dev
re-release: fclean release


.PHONY: check
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
	@echo "OS: $(OS)"
	@echo "Structure:"
	@echo "  - Libraries/src/ : $(words $(LIBRARIES_CPP_SOURCES)) C++ files, $(words $(LIBRARIES_C_SOURCES)) C files"
	@echo "  - src/ : $(words $(MAIN_CPP_SOURCES)) C++ files, $(words $(MAIN_C_SOURCES)) C files"
	@echo "Total sources: $(words $(ALL_CPP_SOURCES)) C++, $(words $(ALL_C_SOURCES)) C"
	@echo "C++ Compiler: $(CXX)"
	@echo "C Compiler: $(CC)"
	@echo "C++ Flags: $(CXXFLAGS)"
	@echo "C Flags: $(CFLAGS)"
	@echo "Includes Directories: $(INCLUDES_DIRS)"
	@echo "Targets: $(TARGET)"
	@echo "OS : $(OS)"
ifeq ($(OS),Linux)
	@echo "  Linux/WSL: sudo apt install libglfw3-dev libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libstb-dev"
else ifeq ($(OS),Darwin)
	@echo "  macOS: brew install glfw glm"
else ifeq ($(OS),Windows_NT)
	@echo "Dependencies:  Windows (MinGW): Use bundled libs or install via vcpkg: vcpkg install glfw3 stb-image"
	@echo "LIBS : $(notdir $(LIB_SOURCES))"
else
	@echo "  Unknown OS"
endif


# Phony Rules
.PHONY: all release dev debug 
.PHONY: run run-release run-dev run-debug
.PHONY: clean fclean re re-debug re-dev re-release
.PHONY: info info-debug info-dev info-release debug-info dev-info release-info
.PHONY: check

# Dependencies
-include $(ALL_OBJECTS:.o=.d)