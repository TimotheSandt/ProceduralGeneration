use std::{env, fs, path::PathBuf};

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let templates_src = manifest_dir
        .parent().unwrap()  // tools/
        .parent().unwrap()  // project root
        .join("engine-sdk/templates");

    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let templates_dst = out_dir.join("templates");
    fs::create_dir_all(&templates_dst).expect("failed to create templates output dir");

    for name in &["main.cpp", "Game.h", "Game.cpp", "Makefile", "config.mk", "gitignore"] {
        let src = templates_src.join(name);
        let dst = templates_dst.join(name);
        fs::copy(&src, &dst)
            .unwrap_or_else(|e| panic!("failed to copy template {name}: {e}"));
        println!("cargo:rerun-if-changed={}", src.display());
    }

    let zip_src = env::var("ENGINE_PAYLOAD_ZIP")
        .expect("ENGINE_PAYLOAD_ZIP must point to the engine dist zip (set by Libraries/Makefile)");
    let zip_dst = out_dir.join("engine_payload.zip");
    fs::copy(&zip_src, &zip_dst)
        .unwrap_or_else(|e| panic!("failed to copy engine payload from {zip_src}: {e}"));
    println!("cargo:rerun-if-changed={zip_src}");
}
