use std::fs;
use std::io::{self, Cursor, Read};
use std::path::{Path, PathBuf};
use zip::ZipArchive;

// ── Embedded payloads (set by build.rs) ──────────────────────────────────────
// ENGINE_ZIP may be a zero-byte stub (dev build without ENGINE_PAYLOAD_ZIP).
// Use has_embedded_engine() before relying on it; GitHub is always tried first.

const ENGINE_ZIP:    &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/engine_payload.zip"));
const TPL_SRC_ZIP:   &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/templates/src.zip"));
const TPL_MAKEFILE:  &str  = include_str!(concat!(env!("OUT_DIR"), "/templates/Makefile"));
const TPL_CONFIG_MK: &str  = include_str!(concat!(env!("OUT_DIR"), "/templates/config.mk"));
const TPL_GITIGNORE: &str  = include_str!(concat!(env!("OUT_DIR"), "/templates/gitignore"));

fn has_embedded_engine() -> bool {
    // A valid ZIP starts with the PK signature (0x50 0x4B).
    ENGINE_ZIP.len() > 4 && ENGINE_ZIP[0] == 0x50 && ENGINE_ZIP[1] == 0x4B
}

// Written once, gitignored — not hash-tracked.
const TPL_CONFIG_LOCAL_MK: &str = "\
# Machine-specific overrides — gitignored, do NOT commit.\n\
# Uncomment and set values relevant to your setup.\n\
\n\
# ENGINE_DIR  := engine\n\
# _ENGINE_MK  := $(ENGINE_DIR)/make/engine.mk\n\
# _VCPKG_ROOT := vcpkg_installed\n\
";

const HASH_TAG: &str = "# @engine-template-hash:";

// ── Engine identity (baked in from Libraries/config.mk via build.rs) ─────────

const ENGINE_NAME:        &str = env!("ENGINE_NAME");
const ENGINE_VERSION:     &str = env!("ENGINE_VERSION");
const ENGINE_PUBLISHER:   &str = env!("ENGINE_PUBLISHER");
const ENGINE_DESCRIPTION: &str = env!("ENGINE_DESCRIPTION");
const ENGINE_HOMEPAGE:    &str = env!("ENGINE_HOMEPAGE");

// ── Release host (baked in from git remote + Makefile) ───────────────────────

const REPO_HOST:   &str = env!("REPO_HOST");    // e.g. "github.com" or "gitlab.com"
const GITHUB_REPO: &str = env!("GITHUB_REPO");  // owner/repo slug

fn platform() -> &'static str {
    if cfg!(target_os = "windows") {
        "windows-x64"
    } else if cfg!(target_os = "macos") && cfg!(target_arch = "aarch64") {
        "macos-arm64"
    } else if cfg!(target_os = "macos") {
        "macos-x64"
    } else {
        "linux-x64"
    }
}

fn engine_asset_name() -> String {
    if cfg!(windows) {
        format!("engine-{}.exe", platform())
    } else {
        format!("engine-{}", platform())
    }
}

fn engine_pkg_name() -> String {
    format!("engine-pkg-{}.zip", platform())
}

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
        "init"          => cmd_init(&args[1..]),
        "update"        => cmd_update(&args[1..]),
        "-h" | "--help" | "help" | "" => { print_usage(); Ok(()) }
        other => {
            print_usage();
            Err(format!("unknown command: {other}"))
        }
    }
}

// ── engine init [project-name] ────────────────────────────────────────────────

fn cmd_init(args: &[String]) -> Result<(), String> {
    let (name, dir, in_place) = match args.first() {
        Some(n) => {
            validate_name(n)?;
            let d = PathBuf::from(n);
            if d.exists() {
                return Err(format!("directory '{}' already exists", d.display()));
            }
            fs::create_dir_all(&d)
                .map_err(|e| format!("failed to create project directory: {e}"))?;
            (n.clone(), d, false)
        }
        None => {
            let cwd = std::env::current_dir()
                .map_err(|e| format!("failed to get current directory: {e}"))?;
            let n = cwd
                .file_name()
                .and_then(|n| n.to_str())
                .unwrap_or("project")
                .to_string();
            validate_name(&n)?;
            (n, PathBuf::from("."), true)
        }
    };

    println!("Initializing project '{name}'...");

    let release = fetch_release_or_warn();
    extract_engine(&dir, release.as_ref())?;
    create_sources(&dir, &name)?;
    write_tracked_file(&dir.join("Makefile"),     &render(TPL_MAKEFILE,  &name), TPL_MAKEFILE)?;
    write_tracked_file(&dir.join(".gitignore"),   &render(TPL_GITIGNORE, &name), TPL_GITIGNORE)?;
    write_once_file(&dir.join("config.mk"),       &render(TPL_CONFIG_MK, &name))?;
    write_once_file(&dir.join("config.local.mk"), TPL_CONFIG_LOCAL_MK)?;

    println!();
    let version_label = release.as_ref().map(|r| r.tag.as_str()).unwrap_or("embedded");
    println!("Project '{name}' ready. (engine {version_label})");
    if !in_place {
        println!("  cd {name}");
    }
    println!("  make install   # install engine vcpkg dependencies");
    println!("  make debug");
    Ok(())
}

// ── engine update [path] ──────────────────────────────────────────────────────

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

    let release = fetch_release_or_warn();
    if let Some(r) = &release {
        println!("  Latest: {}", r.tag);
    }

    extract_engine(&project_dir, release.as_ref())?;
    update_tracked_file(&project_dir.join("Makefile"),   TPL_MAKEFILE,  &project_dir)?;
    update_tracked_file(&project_dir.join(".gitignore"), TPL_GITIGNORE, &project_dir)?;
    merge_config_mk(&project_dir.join("config.mk"), TPL_CONFIG_MK, &project_dir)?;
    write_once_file(&project_dir.join("config.local.mk"), TPL_CONFIG_LOCAL_MK)?;
    update_tool_binary(&project_dir, release.as_ref())?;

    println!("Done.");
    Ok(())
}

// ── GitHub release ────────────────────────────────────────────────────────────

fn fetch_release_or_warn() -> Option<Release> {
    println!("Fetching latest engine release from GitHub...");
    match Release::fetch_latest() {
        Ok(r) => Some(r),
        Err(e) => {
            eprintln!("  Warning: cannot reach GitHub ({e})");
            if has_embedded_engine() {
                eprintln!("  Falling back to embedded engine package.");
            }
            None
        }
    }
}

struct Release {
    tag:    String,
    assets: Vec<(String, String)>, // (name, browser_download_url)
}

impl Release {
    fn fetch_latest() -> Result<Self, String> {
        if REPO_HOST == "github.com" {
            Self::fetch_github()
        } else if REPO_HOST.contains("gitlab") {
            Self::fetch_gitlab()
        } else {
            Err(format!(
                "unsupported git host '{REPO_HOST}' — only github.com and gitlab.* are supported"
            ))
        }
    }

    fn fetch_github() -> Result<Self, String> {
        let url = format!("https://api.github.com/repos/{GITHUB_REPO}/releases/latest");
        let resp = ureq::get(&url)
            .set("User-Agent", &format!("{ENGINE_NAME}/{ENGINE_VERSION}"))
            .set("Accept",     "application/vnd.github+json")
            .call()
            .map_err(|e| format!("GitHub API error: {e}"))?;

        let body: serde_json::Value = resp.into_json()
            .map_err(|e| format!("failed to parse GitHub release JSON: {e}"))?;

        let tag = body["tag_name"].as_str()
            .ok_or("missing tag_name in GitHub response")?
            .to_string();

        let assets = body["assets"].as_array()
            .map(|arr| arr.iter().filter_map(|a| {
                Some((a["name"].as_str()?.to_string(),
                      a["browser_download_url"].as_str()?.to_string()))
            }).collect())
            .unwrap_or_default();

        Ok(Self { tag, assets })
    }

    fn fetch_gitlab() -> Result<Self, String> {
        // GitLab API: GET /api/v4/projects/:id/releases
        // We URL-encode the owner/repo slug as the project id.
        let encoded = GITHUB_REPO.replace('/', "%2F");
        let url = format!("https://{REPO_HOST}/api/v4/projects/{encoded}/releases");
        let resp = ureq::get(&url)
            .set("User-Agent", &format!("{ENGINE_NAME}/{ENGINE_VERSION}"))
            .call()
            .map_err(|e| format!("GitLab API error: {e}"))?;

        let body: serde_json::Value = resp.into_json()
            .map_err(|e| format!("failed to parse GitLab release JSON: {e}"))?;

        // GitLab returns an array sorted newest-first.
        let release = body.as_array()
            .and_then(|a| a.first())
            .ok_or("no releases found on GitLab")?;

        let tag = release["tag_name"].as_str()
            .ok_or("missing tag_name in GitLab response")?
            .to_string();

        // GitLab generic-package / release assets live under assets.links
        let assets = release["assets"]["links"].as_array()
            .map(|arr| arr.iter().filter_map(|a| {
                Some((a["name"].as_str()?.to_string(),
                      a["url"].as_str()?.to_string()))
            }).collect())
            .unwrap_or_default();

        Ok(Self { tag, assets })
    }

    fn find_asset(&self, name: &str) -> Option<&str> {
        self.assets.iter()
            .find(|(n, _)| n == name)
            .map(|(_, url)| url.as_str())
    }
}

fn download_bytes(url: &str, label: &str) -> Result<Vec<u8>, String> {
    let resp = ureq::get(url)
        .set("User-Agent", &format!("{ENGINE_NAME}/{ENGINE_VERSION}"))
        .call()
        .map_err(|e| format!("download of {label} failed: {e}"))?;

    let mut bytes = Vec::new();
    resp.into_reader()
        .read_to_end(&mut bytes)
        .map_err(|e| format!("failed to read {label}: {e}"))?;
    Ok(bytes)
}

fn parse_version(v: &str) -> Option<(u32, u32, u32)> {
    let v = v.trim_start_matches('v');
    let mut parts = v.splitn(3, '.');
    let major: u32 = parts.next()?.parse().ok()?;
    let minor: u32 = parts.next()?.parse().ok()?;
    let patch: u32 = parts.next()
        .and_then(|p| p.split('-').next())
        .and_then(|p| p.parse().ok())
        .unwrap_or(0);
    Some((major, minor, patch))
}

fn is_newer(remote_tag: &str, local: &str) -> bool {
    match (parse_version(remote_tag), parse_version(local)) {
        (Some(r), Some(l)) => r > l,
        _ => false,
    }
}

// ── Engine extraction ─────────────────────────────────────────────────────────

fn extract_engine(project_dir: &Path, release: Option<&Release>) -> Result<(), String> {
    // Try GitHub first if a release is available.
    if let Some(r) = release {
        let pkg_name = engine_pkg_name();
        if let Some(url) = r.find_asset(&pkg_name) {
            println!("  Downloading {} ({})...", pkg_name, r.tag);
            match download_bytes(url, &pkg_name) {
                Ok(bytes) => return extract_zip_bytes(project_dir, &bytes, "engine/"),
                Err(e) => eprintln!("  Warning: download failed ({e}), trying embedded package..."),
            }
        } else {
            eprintln!("  Warning: release {} has no asset '{}', trying embedded package...",
                r.tag, pkg_name);
        }
    }

    // Fallback: use the engine payload embedded at build time.
    if has_embedded_engine() {
        println!("  Using embedded engine package...");
        return extract_zip_bytes(project_dir, ENGINE_ZIP, "engine/");
    }

    Err(format!(
        "no engine package available: {REPO_HOST} unreachable and this binary has no embedded payload.\n\
         Download engine-pkg-{}.zip from {REPO_HOST}/{GITHUB_REPO} and run 'engine update' when online.",
        platform()
    ))
}

fn extract_zip_bytes(project_dir: &Path, bytes: &[u8], subdir: &str) -> Result<(), String> {
    let dest_root = project_dir.join(subdir.trim_end_matches('/'));
    fs::create_dir_all(&dest_root)
        .map_err(|e| format!("failed to create {}: {e}", dest_root.display()))?;

    let cursor = Cursor::new(bytes);
    let mut archive = ZipArchive::new(cursor)
        .map_err(|e| format!("failed to open engine package zip: {e}"))?;

    for i in 0..archive.len() {
        let mut entry = archive.by_index(i)
            .map_err(|e| format!("failed to read zip entry {i}: {e}"))?;

        let raw = entry.name().to_string();
        // The pkg zip has a top-level directory prefix — strip it.
        let rel = strip_top_dir(&raw);
        if rel.is_empty() { continue; }

        let dest = dest_root.join(rel);

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

    println!("  [ok] {subdir}");
    Ok(())
}

// ── Source files ──────────────────────────────────────────────────────────────

fn create_sources(project_dir: &Path, name: &str) -> Result<(), String> {
    let cursor = Cursor::new(TPL_SRC_ZIP);
    let mut archive = ZipArchive::new(cursor)
        .map_err(|e| format!("failed to read embedded src template: {e}"))?;

    let mut count = 0;
    for i in 0..archive.len() {
        let mut entry = archive.by_index(i)
            .map_err(|e| format!("failed to read src entry {i}: {e}"))?;
        if entry.is_dir() { continue; }

        let rel  = entry.name().to_string();
        let dest = project_dir.join("src").join(&rel);
        if let Some(parent) = dest.parent() {
            fs::create_dir_all(parent)
                .map_err(|e| format!("failed to create {}: {e}", parent.display()))?;
        }

        let mut data = Vec::new();
        io::copy(&mut entry, &mut data)
            .map_err(|e| format!("failed to read src/{rel}: {e}"))?;

        let content = if is_text_file(&rel) {
            render(&String::from_utf8_lossy(&data), name).into_bytes()
        } else {
            data
        };
        fs::write(&dest, &content)
            .map_err(|e| format!("failed to write {}: {e}", dest.display()))?;
        count += 1;
    }

    println!("  [ok] src/ ({count} files)");
    Ok(())
}

fn is_text_file(name: &str) -> bool {
    matches!(
        Path::new(name).extension().and_then(|e| e.to_str()).unwrap_or(""),
        "cpp" | "h" | "hpp" | "c" | "inl" | "glsl" | "vert" | "frag" | "cui" | "txt" | "md"
    )
}

// ── Tool binary self-update ───────────────────────────────────────────────────
// If engine[.exe] exists in the project dir and a newer release is available,
// replaces it with the binary from the GitHub release.

fn update_tool_binary(project_dir: &Path, release: Option<&Release>) -> Result<(), String> {
    let ext = if cfg!(windows) { ".exe" } else { "" };
    let dest = project_dir.join(format!("engine{ext}"));
    if !dest.exists() {
        return Ok(());
    }

    let release = match release {
        Some(r) => r,
        None => {
            println!("  [skip] engine{ext} (GitHub unreachable, binary not updated)");
            return Ok(());
        }
    };

    if !is_newer(&release.tag, ENGINE_VERSION) {
        println!("  [skip] engine{ext} (already up to date — v{ENGINE_VERSION})");
        return Ok(());
    }

    let asset_name = engine_asset_name();
    let url = release.find_asset(&asset_name)
        .ok_or_else(|| format!("release {} has no asset '{asset_name}'", release.tag))?;

    println!("  Downloading {} ({})...", asset_name, release.tag);
    let bytes = download_bytes(url, &asset_name)?;

    #[cfg(windows)]
    {
        let backup = dest.with_extension("exe.old");
        let _ = fs::remove_file(&backup);
        fs::rename(&dest, &backup)
            .map_err(|e| format!("failed to backup engine binary: {e}"))?;
    }

    if let Err(e) = fs::write(&dest, &bytes) {
        #[cfg(windows)]
        let _ = fs::rename(dest.with_extension("exe.old"), &dest);
        return Err(format!("failed to write updated binary: {e}"));
    }

    #[cfg(windows)]
    { let _ = fs::remove_file(dest.with_extension("exe.old")); }

    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        let _ = fs::set_permissions(&dest, fs::Permissions::from_mode(0o755));
    }

    println!("  [ok] engine{ext} (updated {} → {})", ENGINE_VERSION, release.tag);
    Ok(())
}

// ── config.mk merge ───────────────────────────────────────────────────────────

fn merge_config_mk(path: &Path, template: &str, project_dir: &Path) -> Result<(), String> {
    let project_name = project_dir
        .file_name().and_then(|n| n.to_str()).unwrap_or("project");
    let rendered = render(template, project_name);

    if !path.exists() {
        fs::write(path, &rendered)
            .map_err(|e| format!("failed to write {}: {e}", path.display()))?;
        println!("  [created] {}", path.display());
        return Ok(());
    }

    let existing = fs::read_to_string(path)
        .map_err(|e| format!("failed to read {}: {e}", path.display()))?;

    let defined: Vec<String> = existing.lines()
        .filter(|l| !l.starts_with(HASH_TAG))
        .filter_map(|line| {
            let t = line.trim();
            if t.starts_with('#') || t.is_empty() { return None; }
            let pos = t.find("?=").or_else(|| t.find(":="))?;
            Some(t[..pos].trim().to_owned())
        })
        .collect();

    let new_lines: Vec<String> = rendered.lines()
        .filter_map(|line| {
            let t = line.trim();
            if t.starts_with('#') || t.is_empty() { return None; }
            let pos = t.find("?=").or_else(|| t.find(":="))?;
            let var = t[..pos].trim();
            if defined.contains(&var.to_owned()) { return None; }
            Some(line.to_owned())
        })
        .collect();

    if new_lines.is_empty() {
        println!("  [skip] {} (no new fields)", path.display());
        return Ok(());
    }

    let added: Vec<&str> = new_lines.iter()
        .filter_map(|line| {
            let pos = line.find("?=").or_else(|| line.find(":="))?;
            Some(line[..pos].trim())
        })
        .collect();

    let mut content = existing;
    if !content.ends_with('\n') { content.push('\n'); }
    content.push_str("\n# Added by engine update\n");
    for line in &new_lines {
        content.push_str(line);
        content.push('\n');
    }
    fs::write(path, &content)
        .map_err(|e| format!("failed to write {}: {e}", path.display()))?;
    println!("  [updated] {} (added: {})", path.display(), added.join(", "));
    Ok(())
}

// ── Template file helpers ─────────────────────────────────────────────────────

fn template_hash(content: &str) -> String {
    let mut h = 0xcbf29ce484222325u64;
    for b in content.as_bytes() {
        h ^= u64::from(*b);
        h = h.wrapping_mul(0x100000001b3);
    }
    format!("{h:016x}")
}

fn write_tracked_file(path: &Path, content: &str, template: &str) -> Result<(), String> {
    let tagged = format!("{HASH_TAG} {}\n{content}", template_hash(template));
    fs::write(path, &tagged)
        .map_err(|e| format!("failed to write {}: {e}", path.display()))?;
    println!("  [ok] {}", path.display());
    Ok(())
}

fn write_once_file(path: &Path, content: &str) -> Result<(), String> {
    if path.exists() {
        println!("  [skip] {} (already exists)", path.display());
        return Ok(());
    }
    fs::write(path, content)
        .map_err(|e| format!("failed to write {}: {e}", path.display()))?;
    println!("  [ok] {}", path.display());
    Ok(())
}

fn update_tracked_file(path: &Path, new_template: &str, project_dir: &Path) -> Result<(), String> {
    let new_hash = template_hash(new_template);
    let project_name = project_dir
        .file_name().and_then(|n| n.to_str()).unwrap_or("project");
    let new_content = render(new_template, project_name);
    let new_tagged  = format!("{HASH_TAG} {new_hash}\n{new_content}");

    if !path.exists() {
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
        println!("  [skip] {} (template unchanged)", path.display());
        return Ok(());
    }

    let body = existing.lines().skip(1).collect::<Vec<_>>().join("\n") + "\n";
    if body.trim() == new_content.trim() {
        fs::write(path, &new_tagged)
            .map_err(|e| format!("failed to write {}: {e}", path.display()))?;
        println!("  [updated] {}", path.display());
    } else {
        println!("  [!] {} has local modifications. Update anyway? [y/N]", path.display());
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

// ── Render / utilities ────────────────────────────────────────────────────────

fn render(template: &str, project_name: &str) -> String {
    let guard = project_name.to_uppercase().replace(['-', ' '], "_");
    let lower = project_name.to_lowercase().replace(' ', "-");
    template
        .replace("{{PROJECT_NAME}}", project_name)
        .replace("{{PROJECT_NAME_LOWER}}", &lower)
        .replace("{{GUARD}}", &guard)
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
    eprintln!("{ENGINE_NAME} v{ENGINE_VERSION}");
    if !ENGINE_PUBLISHER.is_empty() { eprintln!("  by {ENGINE_PUBLISHER}"); }
    if !ENGINE_DESCRIPTION.is_empty() { eprintln!("  {ENGINE_DESCRIPTION}"); }
    if !ENGINE_HOMEPAGE.is_empty() { eprintln!("  {ENGINE_HOMEPAGE}"); }
    eprintln!();
    eprintln!("Usage:");
    eprintln!("  engine init [project-name]   create a new project");
    eprintln!("  engine update [path]         update engine package and template files");
    eprintln!();
    eprintln!("'engine init' with no argument initialises the current directory,");
    eprintln!("using the directory name as the project name.");
    eprintln!();
    eprintln!("Both commands download the engine package from the latest release on {REPO_HOST}.");
    eprintln!("An internet connection is required (falls back to the embedded package if offline).");
    eprintln!();
    eprintln!("Created files:");
    eprintln!("  engine/          pre-built engine (headers, libraries, tools)");
    eprintln!("  src/             C++ sources from the reference demo project");
    eprintln!("  Makefile         build system  — updated by 'engine update'");
    eprintln!("  .gitignore       standard ignores — updated by 'engine update'");
    eprintln!("  config.mk        project config (name, version, ...) — yours to edit");
    eprintln!("  config.local.mk  machine-specific paths — gitignored");
    eprintln!();
    eprintln!("'engine update' (in a project directory):");
    eprintln!("  - Downloads and replaces engine/ with the latest release");
    eprintln!("  - Updates Makefile and .gitignore if unmodified (prompts otherwise)");
    eprintln!("  - Appends new fields to config.mk without touching existing values");
    eprintln!("  - Updates a local engine[.exe] binary if one exists in the project");
}
