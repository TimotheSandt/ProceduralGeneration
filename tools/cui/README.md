# CUI DSL Precompiler

The CUI (Component UI) precompiler now lives in `tools/cui-rs` and builds to a native Rust executable. This legacy Python directory is kept only as historical reference while the build and editor integration use the Rust binary.

## 🔍 How it works

The toolchain operates in two main stages:

### 1. Header Scanning (Metadata Extraction)
The precompiler scans C++ header files for special Doxygen-style annotations (`@cui-*`). It builds a **Registry** of:
- **Components** (`@cui-component`)
- **Modifiers** (`@cui-modifier`)
- **Exposed Functions/Methods** (`@cui-expose`)
- **Enums** (`@cui-enum`)

### 2. View Compilation
It takes `.cui` files and uses the Registry to validate types, components, and modifiers. It then generates:
- `.gen.h` and `.gen.cpp` files containing the generated `UI::View` subclasses.
- A central `registry.json` (found in `Generated/registry.json`).

## 🛠️ Usage

### Commands via Makefile
The build system handles the precompiler automatically during `make`.

### Manual Usage
```bash
make cui-tool
tools/cui-rs/target/release/cui run --headers Libraries/includes --output Generated src/UI/Views/PerformanceView.cui
```

## 📝 Annotations Reference

| Tag | Purpose |
|---|---|
| `@cui-component` | Marks a class as a UI component usable in the DSL. |
| `@cui-modifier` | Exposes a `DoSet*` method as a chained modifier (e.g., `.padding()`). |
| `@cui-expose` | Makes a function or method callable within DSL expressions. |
| `@cui-volatile` | Marks an exposed function as non-pure (re-evaluates every tick). |
| `@cui-enum` | Exposes an enum to the DSL. |

## 📁 Directory Structure
- `lexer.py` / `parser.py`: Handles the `.cui` language syntax.
- `ast_nodes.py`: Defines the internal representation of the UI tree.
- `generator.py`: Generates the C++ output code.
- `scanner.py`: Extracts metadata from C++ headers.
