use std::{env, fs, path::PathBuf};

fn main() {
    let zip_src = env::var("ENGINE_PAYLOAD_ZIP")
        .expect("ENGINE_PAYLOAD_ZIP must point to the engine dist zip (set by Libraries/Makefile)");

    let out_dir = env::var("OUT_DIR").unwrap();
    let dest = PathBuf::from(&out_dir).join("engine_payload.zip");

    fs::copy(&zip_src, &dest)
        .unwrap_or_else(|e| panic!("failed to copy engine payload from {zip_src}: {e}"));

    println!("cargo:rerun-if-changed={zip_src}");
}
