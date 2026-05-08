SHELL := /bin/sh
-include config.mk

CXX          ?= g++
CC           ?= gcc
RC           ?= windres
CLANG_FORMAT ?= clang-format

ENGINE_DIR   := engine
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

SRC_DIR := src
OBJ_DIR := obj/$(BUILD_TYPE)
BIN_DIR := bin/$(BUILD_TYPE)
RES_DIR := res

ifeq ($(BUILD_TYPE),release)
    TARGET_NAME := $(PROJECT_NAME)
else
    TARGET_NAME := main
endif
TARGET := $(BIN_DIR)/$(TARGET_NAME)$(ENGINE_EXE_EXT)

SRCS_CPP := $(shell find $(SRC_DIR) -type f -name "*.cpp" 2>/dev/null)
SRCS_C   := $(shell find $(SRC_DIR) -type f -name "*.c"   2>/dev/null)
OBJS_CPP := $(SRCS_CPP:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/src/%.o)
OBJS_C   := $(SRCS_C:$(SRC_DIR)/%.c=$(OBJ_DIR)/src/%.o)
GEN_OBJS := $(GEN_CPP_SOURCES:$(ENGINE_GEN_DIR)/%.gen.cpp=$(OBJ_DIR)/gen/%.o)
ALL_OBJS := $(OBJS_CPP) $(OBJS_C) $(GEN_OBJS)

ifeq ($(ENGINE_OS),Windows)
    COPY_EXTRA := copy_libs
else
    COPY_EXTRA :=
endif

.PHONY: all debug dev release run run-debug run-release clean fclean

all debug dev release: $(TARGET)

run run-debug: $(TARGET)
	@./$(TARGET)

run-release: $(TARGET)
	@./$(TARGET)

$(TARGET): $(ALL_OBJS) $(ENGINE_LIBS) | $(BIN_DIR) copy_res $(COPY_EXTRA)
	$(CXX) $(ENGINE_CXXFLAGS) $(ALL_OBJS) $(ENGINE_LINK_LIBS) $(ENGINE_LDFLAGS) -o $@
	@echo "Built: $@"

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

$(ALL_OBJS): | $(GEN_CPP_SOURCES) $(GEN_REDIRECT_SOURCES)

$(BIN_DIR) bin/tests:
	@mkdir -p "$@"

.PHONY: copy_res copy_libs

copy_res: | $(BIN_DIR)
	@if [ -d "$(RES_DIR)" ]; then cp -r $(RES_DIR) $(BIN_DIR)/; fi

copy_libs: | $(BIN_DIR)
	@set -- "$(ENGINE_DIR)/vcpkg_installed/$(ENGINE_VCPKG_TRIPLET)/bin"/*.dll; \
	 if [ -e "$$1" ]; then cp -f "$$@" "$(BIN_DIR)/" 2>/dev/null || :; fi

clean:
	@rm -rf obj/ Generated/ bin/
	@echo "Cleaned"

fclean: clean

-include $(wildcard $(ALL_OBJS:.o=.d))
