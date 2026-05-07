# CUI Rust Precompiler

Native implementation of the `.cui` DSL precompiler. It replaces the legacy Python entry point used by the project build and VS Code diagnostics.

## Build

```sh
make cui-tool
```

or directly:

```sh
cargo build --release --manifest-path tools/cui-rs/Cargo.toml
```

The resulting binary is `tools/cui-rs/target/release/cui` (`cui.exe` on Windows).

## Usage

```sh
tools/cui-rs/target/release/cui scan --headers Libraries/includes --output Generated/registry.json
tools/cui-rs/target/release/cui run --headers Libraries/includes --output Generated src/UI/Views/PerformanceView.cui
tools/cui-rs/target/release/cui check --registry Generated/registry.json src/UI/Views/PerformanceView.cui
```
