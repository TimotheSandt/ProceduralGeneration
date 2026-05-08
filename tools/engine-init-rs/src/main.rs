use std::fs;
use std::io::{self, Cursor};
use std::path::{Path, PathBuf};
use zip::ZipArchive;

const ENGINE_ZIP: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/engine_payload.zip"));

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {e}");
        std::process::exit(1);
    }
}

fn run() -> Result<(), String> {
    let args: Vec<String> = std::env::args().skip(1).collect();

    if args.is_empty() || args[0] == "--help" || args[0] == "-h" {
        print_usage();
        return Ok(());
    }

    let project_name = &args[0];
    validate_name(project_name)?;

    let project_dir = PathBuf::from(project_name);
    if project_dir.exists() {
        return Err(format!("directory '{}' already exists", project_dir.display()));
    }

    println!("Creating project '{project_name}'...");
    fs::create_dir_all(&project_dir)
        .map_err(|e| format!("failed to create project directory: {e}"))?;

    extract_engine(&project_dir)?;
    create_sources(&project_dir, project_name)?;
    create_makefile(&project_dir, project_name)?;
    create_config_mk(&project_dir, project_name)?;
    create_gitignore(&project_dir)?;

    println!();
    println!("Project '{project_name}' created.");
    println!();
    println!("  cd {project_name}");
    println!("  make debug");
    Ok(())
}

fn extract_engine(project_dir: &Path) -> Result<(), String> {
    let engine_dir = project_dir.join("engine");
    fs::create_dir_all(&engine_dir)
        .map_err(|e| format!("failed to create engine directory: {e}"))?;

    let cursor = Cursor::new(ENGINE_ZIP);
    let mut archive = ZipArchive::new(cursor)
        .map_err(|e| format!("failed to read embedded engine payload: {e}"))?;

    for i in 0..archive.len() {
        let mut entry = archive.by_index(i)
            .map_err(|e| format!("failed to read zip entry {i}: {e}"))?;

        // Strip the top-level "engine-<platform>/" prefix from each path
        let raw = entry.name().to_string();
        let rel = strip_top_dir(&raw);
        if rel.is_empty() {
            continue;
        }

        let dest = engine_dir.join(rel);

        if entry.is_dir() {
            fs::create_dir_all(&dest)
                .map_err(|e| format!("failed to create dir {}: {e}", dest.display()))?;
        } else {
            if let Some(parent) = dest.parent() {
                fs::create_dir_all(parent)
                    .map_err(|e| format!("failed to create {}: {e}", parent.display()))?;
            }
            let mut out = fs::File::create(&dest)
                .map_err(|e| format!("failed to create {}: {e}", dest.display()))?;
            io::copy(&mut entry, &mut out)
                .map_err(|e| format!("failed to write {}: {e}", dest.display()))?;

            // Preserve executable bit on Unix
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                if let Some(mode) = entry.unix_mode() {
                    let _ = fs::set_permissions(&dest, fs::Permissions::from_mode(mode));
                }
            }
        }
    }

    println!("  [ok] engine/");
    Ok(())
}

fn create_sources(project_dir: &Path, project_name: &str) -> Result<(), String> {
    let src = project_dir.join("src");
    fs::create_dir_all(&src)
        .map_err(|e| format!("failed to create src/: {e}"))?;

    write_file(&src.join("main.cpp"), &main_cpp())?;
    write_file(&src.join("Game.h"),   &game_h(project_name))?;
    write_file(&src.join("Game.cpp"), &game_cpp())?;

    println!("  [ok] src/main.cpp  src/Game.h  src/Game.cpp");
    Ok(())
}

fn create_makefile(project_dir: &Path, project_name: &str) -> Result<(), String> {
    write_file(&project_dir.join("Makefile"), &makefile(project_name))?;
    println!("  [ok] Makefile");
    Ok(())
}

fn create_config_mk(project_dir: &Path, project_name: &str) -> Result<(), String> {
    write_file(&project_dir.join("config.mk"), &config_mk(project_name))?;
    println!("  [ok] config.mk");
    Ok(())
}

fn create_gitignore(project_dir: &Path) -> Result<(), String> {
    write_file(&project_dir.join(".gitignore"), GITIGNORE)?;
    println!("  [ok] .gitignore");
    Ok(())
}

fn write_file(path: &Path, content: &str) -> Result<(), String> {
    fs::write(path, content)
        .map_err(|e| format!("failed to write {}: {e}", path.display()))
}

fn strip_top_dir(path: &str) -> &str {
    match path.find('/') {
        Some(i) => &path[i + 1..],
        None => "",
    }
}

fn validate_name(name: &str) -> Result<(), String> {
    if name.is_empty() {
        return Err("project name cannot be empty".to_string());
    }
    if name.chars().any(|c| matches!(c, '/' | '\\' | ':' | '*' | '?' | '"' | '<' | '>' | '|')) {
        return Err(format!("invalid character in project name: '{name}'"));
    }
    Ok(())
}

fn print_usage() {
    eprintln!("Usage: engine-init <project-name>");
    eprintln!();
    eprintln!("Creates a new game project directory with:");
    eprintln!("  engine/      pre-built engine (headers, libraries, tools)");
    eprintln!("  src/         minimal C++ sources (main.cpp, Game.h, Game.cpp)");
    eprintln!("  Makefile     ready to use with 'make debug' / 'make release'");
    eprintln!("  config.mk    project configuration (name, version, …)");
    eprintln!("  .gitignore   standard ignores for this project layout");
}

// ── Generated file contents ───────────────────────────────────────────────────

fn main_cpp() -> String {
    r#"#include "Game.h"

int main() {
    Game game;
    game.run();
    return 0;
}
"#.to_string()
}

fn game_h(project_name: &str) -> String {
    let guard = format!("{}_GAME_H", project_name.to_uppercase().replace(['-', ' '], "_"));
    format!(
        r#"#pragma once
#ifndef {guard}
#define {guard}

class Game {{
public:
    Game();
    ~Game();

    void run();
}};

#endif // {guard}
"#
    )
}

fn game_cpp() -> String {
    r#"#include "Game.h"

Game::Game() {}
Game::~Game() {}

void Game::run() {
    // TODO: implement your game loop here
}
"#.to_string()
}

fn makefile(project_name: &str) -> String {
    format!(
        r#"SHELL := /bin/sh
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

PROJECT_NAME ?= {project_name}
TARGET := $(BIN_DIR)/$(PROJECT_NAME)$(ENGINE_EXE_EXT)

SRCS_CPP := $(shell find $(SRC_DIR) -type f -name "*.cpp" 2>/dev/null)
SRCS_C   := $(shell find $(SRC_DIR) -type f -name "*.c"   2>/dev/null)
OBJS_CPP := $(SRCS_CPP:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/src/%.o)
OBJS_C   := $(SRCS_C:$(SRC_DIR)/%.c=$(OBJ_DIR)/src/%.o)
GEN_OBJS := $(GEN_CPP_SOURCES:$(ENGINE_GEN_DIR)/%.gen.cpp=$(OBJ_DIR)/gen/%.o)
ALL_OBJS := $(OBJS_CPP) $(OBJS_C) $(GEN_OBJS)

.PHONY: all debug dev release run run-debug run-release clean fclean

all debug dev release: $(TARGET)

run run-debug: $(TARGET)
	@./$(TARGET)

run-release: $(TARGET)
	@./$(TARGET)

$(TARGET): $(ALL_OBJS) $(ENGINE_LIBS) | $(BIN_DIR)
	$(CXX) $(ENGINE_CXXFLAGS) $(ALL_OBJS) $(ENGINE_LINK_LIBS) $(ENGINE_LDFLAGS) -o $@
	@echo "Built: $@"

$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(ENGINE_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -c $< -o $@

$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(ENGINE_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -c $< -o $@

$(OBJ_DIR)/gen/%.o: $(ENGINE_GEN_DIR)/%.gen.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(ENGINE_CXXFLAGS) -MMD -MP $(ENGINE_INCLUDES) -c $< -o $@

$(ALL_OBJS): | $(GEN_CPP_SOURCES) $(GEN_REDIRECT_SOURCES)

$(BIN_DIR):
	@mkdir -p "$@"

clean:
	@rm -rf obj/ Generated/ bin/

fclean: clean

-include $(wildcard $(ALL_OBJS:.o=.d))
"#
    )
}

fn config_mk(project_name: &str) -> String {
    format!(
        r#"PROJECT_NAME        ?= {project_name}
VERSION             ?= 0.1.0
MAINTAINER          ?=
PROJECT_DESCRIPTION ?=
ICON_NAME           ?=
"#
    )
}

const GITIGNORE: &str = r#".*/
.vscode/
.VSCodeCounter/
bin/
obj/
build/
Generated/
logs/
*.log
imgui.ini
engine/lib/
engine/tools/
"#;
