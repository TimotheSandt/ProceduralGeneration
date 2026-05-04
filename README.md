# C++ Game Engine

A C++23 game engine in development, with a multi-backend graphics system (OpenGL, Vulkan, Metal), a component-based UI framework with a custom SwiftUI-inspired DSL, and a static library build system designed for reuse across projects.

> **Work in progress** — architecture and APIs are subject to change.

## 🚀 Quick Start

### Prerequisites
- **Compiler:** C++23 compatible (GCC 13+, Clang 16+, or MSVC 2022).
- **Build System:** GNU Make.
- **Dependencies:** [vcpkg](https://github.com/microsoft/vcpkg) for dependency management.
- **Tools:** Python 3.x (for the CUI DSL precompiler), PowerShell (for formatting on Windows).

### Setup
1. Clone the repository.
2. Install dependencies:
   ```bash
   make install_deps
   ```
3. Build:
   ```bash
   make dev
   ```
4. Run:
   ```bash
   make run-dev
   ```

---

## 🏗️ Architecture

### 🖼️ Graphics System
A layered architecture designed for cross-API compatibility.
- **Renderers:** High-level pass management (`Renderer3D`, `Renderer2D`).
- **Resource Facades:** API-agnostic wrappers for `Mesh`, `Texture`, `Shader`.
- **Backends:** Pluggable low-level implementations (OpenGL, Vulkan, Metal).
- See [README-backend.md](README-backend.md) for a deep dive.

### 🧊 UI System & DSL
A declarative UI system inspired by SwiftUI.
- **DSL Precompiler:** Compiles `.cui` files into C++ source — see [Docs/UI_DSL_Spec.md](Docs/UI_DSL_Spec.md).
- **Runtime:** Dirty-tracking, component-based UI with reactive bindings and layout engine.

### 📊 Profiler
Lightweight performance monitoring with per-frame timing and FPS tracking.

---

## 📦 Using as a Library

The engine compiles to a static archive in `lib/`:

```
lib/
├── debug/libcorelibs.a
└── release/libcorelibs.a
```

To use the engine in another project:
1. Build: `make release`
2. Link against `lib/release/libcorelibs.a`
3. Add `Libraries/includes/` to your include path

---

## 🛠️ Build Commands

| Command | Effect |
|---|---|
| `make debug` | Debug build |
| `make release` | Optimized release build |
| `make clean` | Remove app objects, keep engine archive |
| `make clean-libs` | Remove engine archives only |
| `make clean-exec` | Remove compiled executables |
| `make fclean` | Remove everything |
| `make format` | Auto-format with clang-format |
| `make lint` | Run clang-tidy |
| `make test` | Run test suite |

---

## 📂 Project Structure
- `Libraries/includes/` — Public engine headers
- `Libraries/src/` — Engine implementations
- `lib/` — Compiled engine archives (generated, not committed)
- `src/` — Game application code and UI views
- `tools/cui/` — CUI DSL precompiler (Python)
- `res/` — Shaders, fonts, textures
- `tests/` — Test suite

---

## 📜 License
*Add license information here*
