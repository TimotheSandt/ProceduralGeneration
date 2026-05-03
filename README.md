# Procedural Generation Project

A C++ 20 3D Procedural Generation engine with a custom UI DSL and a multi-backend graphics system (OpenGL, Vulkan, Metal).

## 🚀 Quick Start

### Prerequisites
- **Compiler:** C++20 compatible (GCC 11+, Clang 13+, or MSVC 2022).
- **Build System:** GNU Make.
- **Dependencies:** [vcpkg](https://github.com/microsoft/vcpkg) is used for dependency management.
- **Tools:** Python 3.x (for the CUI DSL precompiler), PowerShell (for formatting on Windows).

### Setup
1. Clone the repository.
2. Install dependencies:
   ```bash
   make install_deps
   ```
3. Build the project:
   ```bash
   make dev
   ```
4. Run:
   ```bash
   make run-dev
   ```

---

## 🏗️ Architecture

The project is divided into several specialized libraries:

### 🎮 Game Engine
- **Game/World:** Core gameplay loop and entity management.
- **ProceduralGeneration:** Terrain generation algorithms, noise functions (`Noise.h`), and grid management.

### 🖼️ Graphics System
A layered architecture designed for cross-API compatibility.
- **Renderers:** High-level pass management (`Renderer3D`, `Renderer2D`).
- **Resource Facades:** API-agnostic wrappers for `Mesh`, `Texture`, `Shader`.
- **Backends:** Low-level implementation (currently OpenGL is fully implemented).
- See [README-backend.md](README-backend.md) for a deep dive into the graphics architecture.

### 🧊 UI System & DSL
A declarative UI system using a custom DSL inspired by SwiftUI.
- **DSL Precompiler:** Compiles `.cui` files into C++ source code.
- **Runtime:** A dirty-tracking component-based UI engine.
- See [Docs/UI_DSL_Spec.md](Docs/UI_DSL_Spec.md) for the language specification.

---

## 🛠️ Development Tools

### CUI DSL Precompiler
Located in `tools/cui/`. It scans C++ headers for `@cui-*` annotations and generates the registry and C++ view classes.
See [tools/cui/README.md](tools/cui/README.md) for usage.

### Formatting & Linting
- **Format:** `make format` (uses `.clang-format`).
- **Lint:** `make lint` (uses `clang-tidy`).
- **Check Syntax:** `make check-syntax`.

---

## 📂 Project Structure
- `Libraries/includes/`: Public headers.
- `Libraries/src/`: Library implementations.
- `src/`: Application entry point and UI views.
- `tools/cui/`: Python source for the UI DSL toolchain.
- `res/`: Shaders, textures, and fonts.
- `tests/`: Unit and integration tests.

---

## 📜 License
*Add license information here*
