use std::{env, fs, io::Write, path::{Path, PathBuf}};
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

    // Engine payload zip (set by Libraries/Makefile during dist build)
    let zip_src = env::var("ENGINE_PAYLOAD_ZIP")
        .expect("ENGINE_PAYLOAD_ZIP must point to the engine dist zip (set by Libraries/Makefile)");
    let zip_dst = out_dir.join("engine_payload.zip");
    fs::copy(&zip_src, &zip_dst)
        .unwrap_or_else(|e| panic!("failed to copy engine payload from {zip_src}: {e}"));
    println!("cargo:rerun-if-changed={zip_src}");
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

fn visit_files(dir: &Path, f: &mut impl FnMut(&Path)) {
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() { visit_files(&path, f); } else { f(&path); }
        }
    }
}
