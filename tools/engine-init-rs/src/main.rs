use std::fs;
use std::io::{self, Cursor};
use std::path::{Path, PathBuf};
use zip::ZipArchive;

// ── Embedded payloads (set by build.rs) ──────────────────────────────────────

const ENGINE_ZIP: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/engine_payload.zip"));

const TPL_MAIN_CPP:  &str = include_str!(concat!(env!("OUT_DIR"), "/templates/main.cpp"));
const TPL_GAME_H:    &str = include_str!(concat!(env!("OUT_DIR"), "/templates/Game.h"));
const TPL_GAME_CPP:  &str = include_str!(concat!(env!("OUT_DIR"), "/templates/Game.cpp"));
const TPL_MAKEFILE:  &str = include_str!(concat!(env!("OUT_DIR"), "/templates/Makefile"));
const TPL_CONFIG_MK: &str = include_str!(concat!(env!("OUT_DIR"), "/templates/config.mk"));
const TPL_GITIGNORE: &str = include_str!(concat!(env!("OUT_DIR"), "/templates/gitignore"));

const HASH_TAG: &str = "# @engine-template-hash:";

// ── Entry point ───────────────────────────────────────────────────────────────

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {e}");
        std::process::exit(1);
    }
}

fn run() -> Result<(), String> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let cmd = args.first().map(String::as_str).unwrap_or("");

    match cmd {
        "init"         => cmd_init(&args[1..]),
        "update"       => cmd_update(&args[1..]),
        "-h" | "--help" | "help" | "" => { print_usage(); Ok(()) }
        other => {
            print_usage();
            Err(format!("unknown command: {other}"))
        }
    }
}

// ── engine init <project-name> ────────────────────────────────────────────────

fn cmd_init(args: &[String]) -> Result<(), String> {
    let name = args.first()
        .ok_or("usage: engine init <project-name>")?;
    validate_name(name)?;

    let dir = PathBuf::from(name);
    if dir.exists() {
        return Err(format!("directory '{}' already exists", dir.display()));
    }
    fs::create_dir_all(&dir)
        .map_err(|e| format!("failed to create project directory: {e}"))?;

    println!("Creating project '{name}'...");
    extract_engine(&dir)?;
    create_sources(&dir, name)?;
    write_template_file(&dir.join("Makefile"),   &render(TPL_MAKEFILE,  name), TPL_MAKEFILE)?;
    write_template_file(&dir.join("config.mk"),  &render(TPL_CONFIG_MK, name), TPL_CONFIG_MK)?;
    write_template_file(&dir.join(".gitignore"), &render(TPL_GITIGNORE, name), TPL_GITIGNORE)?;

    println!();
    println!("Project '{name}' ready.");
    println!("  cd {name}");
    println!("  make debug");
    Ok(())
}

// ── engine update ─────────────────────────────────────────────────────────────

fn cmd_update(args: &[String]) -> Result<(), String> {
    let project_dir = args.first()
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("."));

    if !project_dir.join("engine").exists() {
        return Err(format!(
            "'{}' does not look like an engine project (no engine/ directory)",
            project_dir.display()
        ));
    }

    println!("Updating engine in '{}'...", project_dir.display());

    // Always update engine/
    extract_engine(&project_dir)?;

    // Conditionally update generated text files
    update_template_file(
        &project_dir.join("Makefile"),   TPL_MAKEFILE,  &project_dir)?;
    update_template_file(
        &project_dir.join("config.mk"),  TPL_CONFIG_MK, &project_dir)?;
    update_template_file(
        &project_dir.join(".gitignore"), TPL_GITIGNORE, &project_dir)?;

    println!("Done.");
    Ok(())
}

// ── Engine extraction ─────────────────────────────────────────────────────────

fn extract_engine(project_dir: &Path) -> Result<(), String> {
    let engine_dir = project_dir.join("engine");
    fs::create_dir_all(&engine_dir)
        .map_err(|e| format!("failed to create engine/: {e}"))?;

    let cursor = Cursor::new(ENGINE_ZIP);
    let mut archive = ZipArchive::new(cursor)
        .map_err(|e| format!("failed to read embedded engine payload: {e}"))?;

    for i in 0..archive.len() {
        let mut entry = archive.by_index(i)
            .map_err(|e| format!("failed to read zip entry {i}: {e}"))?;

        let raw = entry.name().to_string();
        let rel = strip_top_dir(&raw); // strips leading "dist/"
        if rel.is_empty() { continue; }

        let dest = engine_dir.join(rel);

        if entry.is_dir() {
            fs::create_dir_all(&dest)
                .map_err(|e| format!("failed to create {}: {e}", dest.display()))?;
        } else {
            if let Some(p) = dest.parent() {
                fs::create_dir_all(p)
                    .map_err(|e| format!("failed to create {}: {e}", p.display()))?;
            }
            let mut out = fs::File::create(&dest)
                .map_err(|e| format!("failed to create {}: {e}", dest.display()))?;
            io::copy(&mut entry, &mut out)
                .map_err(|e| format!("failed to write {}: {e}", dest.display()))?;

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

// ── Source files ──────────────────────────────────────────────────────────────

fn create_sources(project_dir: &Path, name: &str) -> Result<(), String> {
    let src = project_dir.join("src");
    fs::create_dir_all(&src)
        .map_err(|e| format!("failed to create src/: {e}"))?;

    fs::write(src.join("main.cpp"), TPL_MAIN_CPP)
        .map_err(|e| format!("failed to write main.cpp: {e}"))?;
    fs::write(src.join("Game.cpp"), TPL_GAME_CPP)
        .map_err(|e| format!("failed to write Game.cpp: {e}"))?;
    fs::write(src.join("Game.h"), &render(TPL_GAME_H, name))
        .map_err(|e| format!("failed to write Game.h: {e}"))?;

    println!("  [ok] src/main.cpp  src/Game.h  src/Game.cpp");
    Ok(())
}

// ── Template helpers ──────────────────────────────────────────────────────────

fn render(template: &str, project_name: &str) -> String {
    let guard = project_name.to_uppercase().replace(['-', ' '], "_");
    template
        .replace("{{PROJECT_NAME}}", project_name)
        .replace("{{GUARD}}", &guard)
}

fn template_hash(content: &str) -> String {
    let mut h = 0xcbf29ce484222325u64;
    for b in content.as_bytes() {
        h ^= u64::from(*b);
        h = h.wrapping_mul(0x100000001b3);
    }
    format!("{h:016x}")
}

fn write_template_file(path: &Path, content: &str, template: &str) -> Result<(), String> {
    let hash = template_hash(template);
    let tagged = format!("{HASH_TAG} {hash}\n{content}");
    fs::write(path, &tagged)
        .map_err(|e| format!("failed to write {}: {e}", path.display()))?;
    println!("  [ok] {}", path.display());
    Ok(())
}

fn update_template_file(path: &Path, new_template: &str, project_dir: &Path) -> Result<(), String> {
    let new_hash = template_hash(new_template);
    let project_name = project_dir
        .file_name().and_then(|n| n.to_str()).unwrap_or("project");
    let new_content = render(new_template, project_name);
    let new_tagged  = format!("{HASH_TAG} {new_hash}\n{new_content}");

    if !path.exists() {
        // File missing → write it
        fs::write(path, &new_tagged)
            .map_err(|e| format!("failed to write {}: {e}", path.display()))?;
        println!("  [created] {}", path.display());
        return Ok(());
    }

    let existing = fs::read_to_string(path)
        .map_err(|e| format!("failed to read {}: {e}", path.display()))?;

    let stored_hash = existing
        .lines().next()
        .and_then(|l| l.strip_prefix(&format!("{HASH_TAG} ")))
        .map(str::trim)
        .unwrap_or("");

    if stored_hash == new_hash {
        // Template unchanged → nothing to do
        println!("  [skip] {} (template unchanged)", path.display());
        return Ok(());
    }

    // Check if the user has modified the file beyond the hash line
    let body = existing.lines().skip(1).collect::<Vec<_>>().join("\n") + "\n";
    let old_content = render(new_template, project_name); // approximate old content
    if body.trim() == old_content.trim() {
        // Content matches old template → safe to update silently
        fs::write(path, &new_tagged)
            .map_err(|e| format!("failed to write {}: {e}", path.display()))?;
        println!("  [updated] {}", path.display());
    } else {
        // User has customised this file → ask
        println!("  [!] {} has been modified. Update anyway? [y/N]", path.display());
        let mut input = String::new();
        io::stdin().read_line(&mut input).ok();
        if input.trim().eq_ignore_ascii_case("y") {
            fs::write(path, &new_tagged)
                .map_err(|e| format!("failed to write {}: {e}", path.display()))?;
            println!("  [updated] {}", path.display());
        } else {
            println!("  [kept] {}", path.display());
        }
    }

    Ok(())
}

// ── Utilities ─────────────────────────────────────────────────────────────────

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
    eprintln!("Usage:");
    eprintln!("  engine init <project-name>   create a new project");
    eprintln!("  engine update [path]         update engine/ and generated files");
    eprintln!();
    eprintln!("'engine init' creates:");
    eprintln!("  engine/      pre-built engine (headers, libraries, tools)");
    eprintln!("  src/         minimal C++ sources (main.cpp, Game.h, Game.cpp)");
    eprintln!("  Makefile     ready to use with 'make debug' / 'make release'");
    eprintln!("  config.mk    project configuration (name, version, …)");
    eprintln!("  .gitignore   standard ignores for this project layout");
    eprintln!();
    eprintln!("'engine update' updates engine/ with the embedded engine version.");
    eprintln!("  Makefile and config.mk are updated if unchanged, or prompted if modified.");
}
