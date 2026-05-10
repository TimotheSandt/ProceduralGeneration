use std::{collections::HashMap, env, fs, io::Write, path::{Path, PathBuf}};
use zip::write::SimpleFileOptions;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let root = manifest_dir
        .parent().unwrap()  // tools/
        .parent().unwrap(); // project root

    let demo_dir = root.join("demo");
    let src_dir  = root.join("demo").join("src");

    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let tpl_dir = out_dir.join("templates");
    fs::create_dir_all(&tpl_dir).expect("failed to create templates output dir");

    // Build system templates come from demo/
    for name in &["Makefile", "config.mk", "gitignore"] {
        let src_path = demo_dir.join(if *name == "gitignore" { ".gitignore" } else { name });
        let dst_path = tpl_dir.join(name);
        fs::copy(&src_path, &dst_path)
            .unwrap_or_else(|e| panic!("failed to copy {name} from demo/: {e}"));
        println!("cargo:rerun-if-changed={}", src_path.display());
    }

    // C++ source tree: demo/src/ → templates/src.zip (preserving directory structure)
    let zip_path = tpl_dir.join("src.zip");
    let zip_file = fs::File::create(&zip_path).expect("failed to create src.zip");
    let mut writer = zip::ZipWriter::new(zip_file);
    let opts = SimpleFileOptions::default()
        .compression_method(zip::CompressionMethod::Deflated);
    zip_dir(&src_dir, &src_dir, &mut writer, opts);
    writer.finish().expect("failed to finalize src.zip");

    // Emit rerun-if-changed for every file under demo/src/
    visit_files(&src_dir, &mut |p| println!("cargo:rerun-if-changed={}", p.display()));

    // ── Engine identity from Libraries/config.mk ──────────────────────────────
    // Priority: env var (set by Makefile) > Libraries/config.mk > built-in default.
    // This lets `cargo build` work without the Makefile, using the file as fallback.
    let engine_config_path = root.join("Libraries").join("config.mk");
    let engine_config = parse_mk_config(&engine_config_path);
    println!("cargo:rerun-if-changed={}", engine_config_path.display());

    let bake = |key: &str, default: &str| {
        let val = env::var(key)
            .ok()
            .filter(|v| !v.is_empty())
            .or_else(|| engine_config.get(key).cloned())
            .unwrap_or_else(|| default.to_string());
        println!("cargo:rustc-env={key}={val}");
        println!("cargo:rerun-if-env-changed={key}");
    };

    bake("ENGINE_NAME",        "GameEngine");
    bake("ENGINE_VERSION",     env!("CARGO_PKG_VERSION"));
    bake("ENGINE_PUBLISHER",   "");
    bake("ENGINE_AUTHOR",      "");
    bake("ENGINE_EMAIL",       "");
    bake("ENGINE_DESCRIPTION", "");
    bake("ENGINE_HOMEPAGE",    "");

    // Git host and repo slug — set by the Makefile's remote detection.
    let repo_host   = env::var("REPO_HOST").unwrap_or_else(|_| "github.com".to_string());
    let github_repo = env::var("GITHUB_REPO")
        .unwrap_or_else(|_| "TimotheSandt/ProceduralGeneration".to_string());
    println!("cargo:rustc-env=REPO_HOST={repo_host}");
    println!("cargo:rustc-env=GITHUB_REPO={github_repo}");
    println!("cargo:rerun-if-env-changed=REPO_HOST");
    println!("cargo:rerun-if-env-changed=GITHUB_REPO");

    // Engine payload zip — embedded as offline fallback.
    // If ENGINE_PAYLOAD_ZIP is not set (dev build), an empty stub is written so
    // include_bytes! still compiles; the runtime detects the stub and skips fallback.
    println!("cargo:rerun-if-env-changed=ENGINE_PAYLOAD_ZIP");
    let zip_dst = out_dir.join("engine_payload.zip");
    match env::var("ENGINE_PAYLOAD_ZIP") {
        Ok(zip_src) => {
            fs::copy(&zip_src, &zip_dst)
                .unwrap_or_else(|e| panic!("failed to copy engine payload from {zip_src}: {e}"));
            println!("cargo:rerun-if-changed={zip_src}");
        }
        Err(_) => {
            // No payload provided — write a zero-byte stub; GitHub will be used at runtime.
            fs::write(&zip_dst, b"").expect("failed to create stub engine_payload.zip");
        }
    }
}

fn zip_dir(
    base: &Path,
    dir: &Path,
    writer: &mut zip::ZipWriter<fs::File>,
    opts: SimpleFileOptions,
) {
    let entries = fs::read_dir(dir)
        .unwrap_or_else(|e| panic!("cannot read {}: {e}", dir.display()));
    for entry in entries {
        let entry = entry.expect("dir entry error");
        let path = entry.path();
        let rel  = path.strip_prefix(base).unwrap();
        let name = rel.to_string_lossy().replace('\\', "/");
        if path.is_dir() {
            zip_dir(base, &path, writer, opts);
        } else {
            writer.start_file(&name, opts)
                .unwrap_or_else(|e| panic!("failed to add {name} to zip: {e}"));
            let data = fs::read(&path)
                .unwrap_or_else(|e| panic!("failed to read {}: {e}", path.display()));
            writer.write_all(&data)
                .unwrap_or_else(|e| panic!("failed to write {name} to zip: {e}"));
        }
    }
}

fn parse_mk_config(path: &Path) -> HashMap<String, String> {
    let mut map = HashMap::new();
    let content = match fs::read_to_string(path) {
        Ok(c) => c,
        Err(_) => return map,
    };
    for line in content.lines() {
        let line = line.trim();
        if line.starts_with('#') || line.is_empty() { continue; }
        // Match KEY ?= VALUE or KEY := VALUE
        let (key, val) = if let Some(pos) = line.find("?=") {
            (&line[..pos], &line[pos + 2..])
        } else if let Some(pos) = line.find(":=") {
            (&line[..pos], &line[pos + 2..])
        } else {
            continue;
        };
        // Strip inline comments and trim whitespace
        let val = val.split('#').next().unwrap_or("").trim();
        map.insert(key.trim().to_string(), val.to_string());
    }
    map
}

fn visit_files(dir: &Path, f: &mut impl FnMut(&Path)) {
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() { visit_files(&path, f); } else { f(&path); }
        }
    }
}
