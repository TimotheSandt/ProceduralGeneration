

# Variables
CXX = g++
CC = gcc
PROJECT_NAME = ProceduralGeneration

# Flags
CFLAGS = -Wall -Wextra -Werror -m64
CXXFLAGS = $(CFLAGS) -std=c++23
# dev flags
CFLAGS_DEV = -Wall -Wextra -m64
CXXFLAGS_DEV = $(CFLAGS_DEV) -std=c++23

# Includes
INCLUDES_BASE = Libraries/includes
INCLUDES_DIRS := $(notdir $(wildcard $(INCLUDES_BASE)/*))
INCLUDES := -I$(INCLUDES_BASE) $(foreach dir,$(INCLUDES_DIRS),-I$(INCLUDES_BASE)/$(dir))

# Linker
LDFLAGS = -LLibraries/libs/ThirdParty/ -lglfw3dll -lstb_image -lpsapi

# Directories
LIBRARIES_SRC_DIR = Libraries/src
LIBRARIES_LIB_DIR = Libraries/libs
MAIN_SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
RES_DIR = res

# Target Executable
TARGET = $(BIN_DIR)/normal/$(PROJECT_NAME)
TARGET_DEBUG = $(BIN_DIR)/debug/main
TARGET_RELEASE = $(BIN_DIR)/release/$(PROJECT_NAME)

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
# Normal build objects
LIBRARIES_CPP_OBJECTS_NORMAL = $(LIBRARIES_CPP_SOURCES:$(LIBRARIES_SRC_DIR)/%.cpp=$(OBJ_DIR)/normal/Libraries/%.o)
LIBRARIES_C_OBJECTS_NORMAL = $(LIBRARIES_C_SOURCES:$(LIBRARIES_SRC_DIR)/%.c=$(OBJ_DIR)/normal/Libraries/%.o)
MAIN_CPP_OBJECTS_NORMAL = $(MAIN_CPP_SOURCES:$(MAIN_SRC_DIR)/%.cpp=$(OBJ_DIR)/normal/src/%.o)
MAIN_C_OBJECTS_NORMAL = $(MAIN_C_SOURCES:$(MAIN_SRC_DIR)/%.c=$(OBJ_DIR)/normal/src/%.o)
ALL_OBJECTS_NORMAL = $(LIBRARIES_CPP_OBJECTS_NORMAL) $(LIBRARIES_C_OBJECTS_NORMAL) $(MAIN_CPP_OBJECTS_NORMAL) $(MAIN_C_OBJECTS_NORMAL)

# Debug build objects
LIBRARIES_CPP_OBJECTS_DEBUG = $(LIBRARIES_CPP_SOURCES:$(LIBRARIES_SRC_DIR)/%.cpp=$(OBJ_DIR)/debug/Libraries/%.o)
LIBRARIES_C_OBJECTS_DEBUG = $(LIBRARIES_C_SOURCES:$(LIBRARIES_SRC_DIR)/%.c=$(OBJ_DIR)/debug/Libraries/%.o)
MAIN_CPP_OBJECTS_DEBUG = $(MAIN_CPP_SOURCES:$(MAIN_SRC_DIR)/%.cpp=$(OBJ_DIR)/debug/src/%.o)
MAIN_C_OBJECTS_DEBUG = $(MAIN_C_SOURCES:$(MAIN_SRC_DIR)/%.c=$(OBJ_DIR)/debug/src/%.o)
ALL_OBJECTS_DEBUG = $(LIBRARIES_CPP_OBJECTS_DEBUG) $(LIBRARIES_C_OBJECTS_DEBUG) $(MAIN_CPP_OBJECTS_DEBUG) $(MAIN_C_OBJECTS_DEBUG)

# Release build objects
LIBRARIES_CPP_OBJECTS_RELEASE = $(LIBRARIES_CPP_SOURCES:$(LIBRARIES_SRC_DIR)/%.cpp=$(OBJ_DIR)/release/Libraries/%.o)
LIBRARIES_C_OBJECTS_RELEASE = $(LIBRARIES_C_SOURCES:$(LIBRARIES_SRC_DIR)/%.c=$(OBJ_DIR)/release/Libraries/%.o)
MAIN_CPP_OBJECTS_RELEASE = $(MAIN_CPP_SOURCES:$(MAIN_SRC_DIR)/%.cpp=$(OBJ_DIR)/release/src/%.o)
MAIN_C_OBJECTS_RELEASE = $(MAIN_C_SOURCES:$(MAIN_SRC_DIR)/%.c=$(OBJ_DIR)/release/src/%.o)
ALL_OBJECTS_RELEASE = $(LIBRARIES_CPP_OBJECTS_RELEASE) $(LIBRARIES_C_OBJECTS_RELEASE) $(MAIN_CPP_OBJECTS_RELEASE) $(MAIN_C_OBJECTS_RELEASE)

# Build Rules
all: $(TARGET)

debug: CXXFLAGS = $(CXXFLAGS_DEV)
debug: CFLAGS = $(CFLAGS_DEV)

dev: CXXFLAGS = $(CXXFLAGS_DEV)
dev: CFLAGS = $(CFLAGS_DEV)


# Executable Rules
debug: $(TARGET_DEBUG)
release: $(TARGET_RELEASE)
dev: $(TARGET)

# Copy libs
copy_libs_normal: | $(BIN_DIR)/normal
	@echo "Copying libraries to $(BIN_DIR)/normal"
	@cp $(LIBRARIES_DLL_SOURCES) $(BIN_DIR)/normal/
	@cp $(LIBRARIES_LIB_SOURCES) $(BIN_DIR)/normal/

copy_libs_debug: | $(BIN_DIR)/debug
	@echo "Copying libraries to $(BIN_DIR)/debug"
	@cp $(LIBRARIES_DLL_SOURCES) $(BIN_DIR)/debug/
	@cp $(LIBRARIES_LIB_SOURCES) $(BIN_DIR)/debug/

copy_libs_release: | $(BIN_DIR)/release
	@echo "Copying libraries to $(BIN_DIR)/release"
	@cp $(LIBRARIES_DLL_SOURCES) $(BIN_DIR)/release/
	@cp $(LIBRARIES_LIB_SOURCES) $(BIN_DIR)/release/

copy_res_normal: | $(BIN_DIR)/normal
	@echo "Copying resources to $(BIN_DIR)/normal"
	@cp -r $(RES_DIR) $(BIN_DIR)/normal

copy_res_debug: | $(BIN_DIR)/debug
	@echo "Copying resources to $(BIN_DIR)/debug"
	@cp -r $(RES_DIR) $(BIN_DIR)/debug

copy_res_release: | $(BIN_DIR)/release
	@echo "Copying resources to $(BIN_DIR)/release"
	@cp -r $(RES_DIR) $(BIN_DIR)/release

all_copy_normal: copy_libs_normal copy_res_normal
all_copy_debug: copy_libs_debug copy_res_debug
all_copy_release: copy_libs_release copy_res_release
 
# Build all objects
# Normal build
$(TARGET): CXXFLAGS += -DNDEBUG -O1
$(TARGET): $(ALL_OBJECTS_NORMAL) all_copy_normal | $(BIN_DIR)/normal
	$(CXX) $(CXXFLAGS) $(ALL_OBJECTS_NORMAL) $(LDFLAGS) -o $@
	@echo "Compilation successful for: $(TARGET)"

# Debug build
$(TARGET_DEBUG): CXXFLAGS += -DDEBUG -g3 -O0
$(TARGET_DEBUG): $(ALL_OBJECTS_DEBUG) all_copy_debug | $(BIN_DIR)/debug
	$(CXX) $(CXXFLAGS) $(ALL_OBJECTS_DEBUG) $(LDFLAGS) -o $@
	@echo "Debug compilation successful for: $(TARGET_DEBUG)"

# Release build
$(TARGET_RELEASE): CXXFLAGS += -DNDEBUG -O3
$(TARGET_RELEASE): $(ALL_OBJECTS_RELEASE) all_copy_release | $(BIN_DIR)/release
	$(CXX) $(CXXFLAGS) $(ALL_OBJECTS_RELEASE) $(LDFLAGS) -o $@
	@echo "Release compilation successful for: $(TARGET_RELEASE)"

# Files Compilation
# C++ source files
$(OBJ_DIR)/normal/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.cpp | $(OBJ_DIR)/normal
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Libraries) normal: $<"
$(OBJ_DIR)/debug/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.cpp | $(OBJ_DIR)/debug
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DDEBUG -g3 -O0 $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Libraries) debug: $<"
$(OBJ_DIR)/release/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.cpp | $(OBJ_DIR)/release
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DNDEBUG -O3 $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Libraries) release: $<"

# C source files
$(OBJ_DIR)/normal/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.c | $(OBJ_DIR)/normal
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Libraries) normal: $<"
$(OBJ_DIR)/debug/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.c | $(OBJ_DIR)/debug
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DDEBUG -g3 -O0 $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Libraries) debug: $<"
$(OBJ_DIR)/release/Libraries/%.o: $(LIBRARIES_SRC_DIR)/%.c | $(OBJ_DIR)/release
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DNDEBUG -O3 $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Libraries) release: $<"

# C++ main source files
$(OBJ_DIR)/normal/src/%.o: $(MAIN_SRC_DIR)/%.cpp | $(OBJ_DIR)/normal
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Main) normal: $<"
$(OBJ_DIR)/debug/src/%.o: $(MAIN_SRC_DIR)/%.cpp | $(OBJ_DIR)/debug
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DDEBUG -g3 -O0 $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Main) debug: $<"
$(OBJ_DIR)/release/src/%.o: $(MAIN_SRC_DIR)/%.cpp | $(OBJ_DIR)/release
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DNDEBUG -O3 $(INCLUDES) -c $< -o $@
	@echo "Compiled (C++ Main) release: $<"

# C main source files
$(OBJ_DIR)/normal/src/%.o: $(MAIN_SRC_DIR)/%.c | $(OBJ_DIR)/normal
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Main) normal: $<"
$(OBJ_DIR)/debug/src/%.o: $(MAIN_SRC_DIR)/%.c | $(OBJ_DIR)/debug
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DDEBUG -g3 -O0 $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Main) debug: $<"
$(OBJ_DIR)/release/src/%.o: $(MAIN_SRC_DIR)/%.c | $(OBJ_DIR)/release
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DNDEBUG -O3 $(INCLUDES) -c $< -o $@
	@echo "Compiled (C Main) release: $<"

# Create Directories
$(OBJ_DIR)/normal:
	mkdir -p $(OBJ_DIR)/normal/Libraries/Game $(OBJ_DIR)/normal/Libraries/Graphics $(OBJ_DIR)/normal/Libraries/Profiler $(OBJ_DIR)/normal/Libraries/ThirdParty $(OBJ_DIR)/normal/src

$(OBJ_DIR)/debug:
	mkdir -p $(OBJ_DIR)/debug/Libraries/Game $(OBJ_DIR)/debug/Libraries/Graphics $(OBJ_DIR)/debug/Libraries/Profiler $(OBJ_DIR)/debug/Libraries/ThirdParty $(OBJ_DIR)/debug/src

$(OBJ_DIR)/release:
	mkdir -p $(OBJ_DIR)/release/Libraries/Game $(OBJ_DIR)/release/Libraries/Graphics $(OBJ_DIR)/release/Libraries/Profiler $(OBJ_DIR)/release/Libraries/ThirdParty $(OBJ_DIR)/release/src

$(BIN_DIR)/normal:
	mkdir -p $(BIN_DIR)/normal

$(BIN_DIR)/debug:
	mkdir -p $(BIN_DIR)/debug

$(BIN_DIR)/release:
	mkdir -p $(BIN_DIR)/release


# Clean Rules
clean:
	rm -rf $(OBJ_DIR)
	@echo "Objects deleted"

fclean: clean
	rm -rf $(BIN_DIR)
	@echo "Executables deleted"

re: fclean all

# Execution Rules
run: $(TARGET)
	./$(TARGET)

run-debug: $(TARGET_DEBUG)
	./$(TARGET_DEBUG)

run-release: $(TARGET_RELEASE)
	./$(TARGET_RELEASE)

# Check 
check:
	@echo "Checking C++ syntax..."
	@if [ "$(ALL_CPP_SOURCES)" != "" ]; then $(CXX) $(CXXFLAGS) $(INCLUDES) -fsyntax-only $(ALL_CPP_SOURCES); fi
	@echo "Checking C syntax..."
	@if [ "$(ALL_C_SOURCES)" != "" ]; then $(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only $(ALL_C_SOURCES); fi

# Info 
info:
	@echo "Project: $(PROJECT_NAME)"
	@echo "Structure:"
	@echo "  - Libraries/src/ : $(words $(LIBRARIES_CPP_SOURCES)) C++ files, $(words $(LIBRARIES_C_SOURCES)) C files"
	@echo "  - src/ : $(words $(MAIN_CPP_SOURCES)) C++ files, $(words $(MAIN_C_SOURCES)) C files"
	@echo "Total sources: $(words $(ALL_CPP_SOURCES)) C++, $(words $(ALL_C_SOURCES)) C"
	@echo "C++ Compiler: $(CXX)"
	@echo "C Compiler: $(CC)"
	@echo "C++ Flags: $(CXXFLAGS)"
	@echo "C Flags: $(CFLAGS)"
	@echo "Includes Directories: $(INCLUDES_DIRS)"
	@echo "LIBS : $(notdir $(LIB_SOURCES))"
	@echo "Targets: $(TARGET), $(TARGET_DEBUG), $(TARGET_RELEASE)"

# Phony Rules
.PHONY: all clean fclean re debug release dev run run-debug run-release check info

# Dependencies
-include $(ALL_OBJECTS_NORMAL:.o=.d) $(ALL_OBJECTS_DEBUG:.o=.d) $(ALL_OBJECTS_RELEASE:.o=.d)