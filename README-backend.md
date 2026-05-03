# Graphics System Guide

**Status:** ✅ Fully Functional (OpenGL) | 🚧 Planned (Vulkan, Metal)

## Overview

The rendering architecture is split into three layers:

| Layer                                                                                               | Responsibility                              | Location                             |
| --------------------------------------------------------------------------------------------------- | ------------------------------------------- | ------------------------------------ |
| **Renderers** (`Renderer3D`, `Renderer2D`)                                                | Pass management, post-processing, upscaling | `Libraries/includes/Graphics/`     |
| **Resource facades** (`Mesh`, `Texture`, `Buffer`, `RenderTarget`, `ShaderProgram`) | API-agnostic GPU resource wrappers          | `Libraries/includes/Graphics/`     |
| **Backend** (`OpenGLGraphicsBackend`, …)                                                   | Actual `gl*` calls, shader compilation    | `Libraries/src/Graphics/Backends/` |

Gameplay and UI code only touch the top two layers. The backend is never called directly.

**Current state:**

- `OpenGL` is implemented and usable.
- `Vulkan` and `Metal` are recognized by the runtime selection system but are not implemented yet.

---

## Architecture Rules

- High-level code (`Game`, `World`, UI) **must not** call `gl*` directly.
- All GPU state changes go through `GraphicsRenderState::`.
- All GPU resource creation goes through the device: `TryGetActiveGraphicsDevice()->Create*()`.
- Backend-specific code stays under `Libraries/src/Graphics/Backends/`.
- A renderer subclass passes its required API to the `Renderer` base constructor, then implements `OnBeginPass`, `OnClear`, and `GetRenderTarget`. It never touches `gl*`.

---

## Runtime Selection

The active backend is chosen at startup via command-line arguments, parsed in `main.cpp`:

```
ProceduralGeneration                # starts with OpenGL (default)
ProceduralGeneration --api opengl
ProceduralGeneration --api vulkan   # recognized but will fail (not implemented)
ProceduralGeneration --api metal    # macOS only, not implemented
ProceduralGeneration --list-apis    # print availability of every backend
ProceduralGeneration --choose-api   # interactive selection prompt
ProceduralGeneration --help
```

After argument parsing, `main.cpp` initializes the backend and binds it globally:

```cpp
graphicsBackend = CreateGraphicsBackend({selectedApi});
graphicsBackend->Initialize();
graphicsDevice  = graphicsBackend->CreateDevice({});
BindGraphicsRuntime({.api = selectedApi, .backend = graphicsBackend.get(), .device = graphicsDevice.get()});
```

After `BindGraphicsRuntime`, `IsGraphicsAPIActive(GraphicsAPI::OpenGL)` returns `true` and all resource facades and renderers are operational.

---

## Per-frame Pipeline

Every renderer follows the same pipeline each frame:

```
SetOutputResolution(w, h)   ← once on startup, and again whenever the window is resized
SetRenderScale(scale)        ← optional; persists across frames
BeginPass()                  ← if post-processing, upscaling, or frame generation is active:
                               allocates/resizes an offscreen RenderTarget and binds it so
                               all draw calls render into it instead of the screen.
                               Otherwise binds the default framebuffer (screen) directly.
Clear(color)                 ← optional
[draw calls]
EndPass()                    ← runs post-process chain → upscale → OnEndPass cleanup
                               (frame generation is wired but not yet active — see below)
```

`BeginPass` and `EndPass` are non-virtual. They own the full skeleton and call the appropriate protected hooks on the subclass. You never override them.

### Why an offscreen RenderTarget?

Some pipeline stages (post-processing, upscaling) need to read the rendered image as a texture. You cannot read from the screen while writing to it. So when any of those stages are active, the renderer renders into an offscreen texture first, then that texture is processed and finally blitted to the screen in `EndPass`.

---

## 3D Rendering

`Renderer3D` manages the 3D scene pass. It owns an internal `RenderTarget` (`sceneRenderTarget`) that is used automatically when upscaling or post-processing is active.

### Per-frame usage

```cpp
// Call once at startup and again in your window-resize callback:
renderer3D.SetOutputResolution(windowWidth, windowHeight);

renderer3D.BeginPass();
renderer3D.Clear(window.GetClearColor(), /*clearDepth=*/true);
// ... scene draw calls ...
renderer3D.EndPass();   // runs post-process chain then upscale, if active
```

### Drawing

```cpp
renderer3D.SetCamera(camera);       // uploads camera matrices to binding point 0
renderer3D.DrawMesh(mesh, camera);  // binds shader + textures + buffers, draws, unbinds
```

### Render scale and upscaling

```cpp
// Render at a fraction of the output resolution, then upscale back up.
renderer3D.SetRenderScale(0.5f);          // render at 50%; automatically enables upscaling
renderer3D.SetRenderScale(1.0f);          // render at full res; automatically disables upscaling

// Manual toggle (e.g. to temporarily bypass upscaling without changing scale)
renderer3D.SetUpscalingEnabled(false);
renderer3D.SetUpscalingEnabled(true);

// Query
renderer3D.IsUpscalingEnabled();
renderer3D.GetRenderScale();
renderer3D.GetActiveUpscaleMode();        // "bilinear-blit" by default
renderer3D.GetRegisteredUpscaleModes();   // list all registered modes

// Switch mode
renderer3D.SetActiveUpscaleMode("bilinear-blit");
renderer3D.SetActiveUpscaleMode("disabled");  // bypass upscaling even if enabled
```

`SetRenderScale` and `SetUpscalingEnabled` persist across frames.

---

## 2D / UI Rendering

`Renderer2D` handles all 2D content — text, sprites, UI widgets. It owns its own internal `RenderTarget` (`uiRenderTarget`).

When upscaling or post-processing is active, `BeginPass` binds the RT and copies the current screen content into it, so the UI is composited on top of the 3D scene before the final blit.

### Per-frame usage

```cpp
// Same rule as 3D: call on startup and on resize only.
rendererUI.SetOutputResolution(windowWidth, windowHeight);

rendererUI.BeginPass();
// ... draw calls ...
rendererUI.EndPass();
```

### Text

```cpp
rendererUI.RenderText(textRenderer, "fps: 60", x, y, scale,
                      glm::vec3(1.0f, 1.0f, 1.0f), UI::TextAnchor::TopLeft);
```

Advanced layout with overflow clipping or scrolling:

```cpp
UI::TextLayoutParams params;
params.maxWidth  = 300.0f;
params.maxHeight = 80.0f;
params.overflow  = UI::TextOverflow::Hidden;
rendererUI.RenderTextAdvanced(textRenderer, "Long text...", x, y, params, color, scale);
```

### Sprites

```cpp
Sprite sprite;
sprite.SetShader("shader/UI/default.vert", "shader/UI/default.frag");

SpriteDrawParams params;
params.offset        = {x, y};           // screen position (top-left corner)
params.scale         = {width, height};  // size in pixels
params.color         = {1.0f, 1.0f, 1.0f, 1.0f};

rendererUI.DrawSprite(sprite, params);             // no texture
rendererUI.DrawSprite(sprite, params, &myTexture); // with texture
```

#### Scrollable content (`scrollOffset` / `contentSize`)

These two fields implement a viewport into a larger virtual canvas — useful for scrollable panels:

- `containerSize` — the visible area on screen (e.g. `{200, 300}`)
- `contentSize`   — the total scrollable area (e.g. `{200, 800}` — taller than the container)
- `scrollOffset`  — how far into the content we've scrolled (e.g. `{0, 150}` = 150 px down)

The shader remaps UVs so only the region `[scrollOffset, scrollOffset + containerSize]` out of `contentSize` is visible. When `contentSize` is `{0, 0}`, scrolling is disabled and the texture fills the container normally.

```cpp
params.containerSize = {200.0f, 300.0f};  // panel visible area
params.contentSize   = {200.0f, 800.0f};  // full scrollable height
params.scrollOffset  = {0.0f, 150.0f};    // scrolled 150 px down
```

### Scissor clipping

Scissor is a GPU-level rectangle mask: any pixel drawn outside the rectangle is discarded before it reaches the framebuffer. Used to clip UI elements to a panel boundary (e.g. text that should not overflow outside a box).

```cpp
rendererUI.PushClipRect(x, y, width, height);
// ... clipped draw calls ...
rendererUI.PopClipRect();
```

### Render scale and upscaling

```cpp
rendererUI.SetRenderScale(0.75f);   // validates range (0, 1]; auto-enables upscaling
rendererUI.SetRenderScale(1.0f);    // auto-disables upscaling

rendererUI.SetUpscalingEnabled(false);  // manual override
rendererUI.IsUpscalingEnabled();
rendererUI.GetRenderScale();
```

### Compositing a RenderTarget

Draw the colour buffer of a `RenderTarget` as a fullscreen quad into the current 2D pass:

```cpp
rendererUI.PresentRenderTarget(myRenderTarget);
```

---

## Post-Processing

Post-process passes are executed in order by `EndPass`, **after all draw calls and before upscaling**. They receive a mutable `RenderTarget` at render resolution.

Any number of passes can be registered on any renderer:

```cpp
renderer3D.AddPostProcessPass(std::make_unique<MyVignettePass>());
renderer3D.AddPostProcessPass(std::make_unique<MyColorGradingPass>());
renderer3D.RemovePostProcessPass("vignette");
```

When at least one post-process pass is registered, the renderer automatically uses an offscreen `RenderTarget` even if upscaling is disabled.

### Implementing a post-process pass

Implement `IPostProcessPass` (`Graphics/Upscaling/IPostProcessPass.h`):

```cpp
// VignettePass.h
#include "Graphics/Upscaling/IPostProcessPass.h"
#include "Graphics/ShaderProgram.h"

class VignettePass final : public IPostProcessPass
{
  public:
    VignettePass();

    std::string GetName() const noexcept override { return "vignette"; }
    void Process(RenderTarget &target, int width, int height) const override;

  private:
    ShaderProgram shader;
};
```

```cpp
// VignettePass.cpp
#include "VignettePass.h"
#include "Graphics/RenderTarget.h"
#include "Graphics/Core/RenderState.h"

VignettePass::VignettePass()
{
    shader.SetShader("shader/post/vignette.vert", "shader/post/vignette.frag");
}

void VignettePass::Process(RenderTarget &target, int width, int height) const
{
    // target contains the scene at render resolution.
    // Render into target using its own colour texture as input.
    // You can modify target in-place via RenderScreenQuad or
    // own a second RenderTarget for ping-pong.

    GraphicsRenderState::PrepareScreenPass(width, height);
    shader.Bind();
    target.GetTexture().texUnit(shader);
    target.GetTexture().Bind();
    target.RenderScreenQuad(width, height);
    target.GetTexture().Unbind();
    shader.Unbind();
}
```

The shader receives the scene colour via `sampler2D screenTexture`. Output `fragColor` replaces the scene content in the render target.

---

## Implementing a New Upscale Mode

### Simple mode — render-target-based

Subclass `RenderTargetUpscaleMode`. It provides name storage and a default `SupportsRenderer` that returns `true`. You only implement `Upscale`.

```cpp
// MySharpUpscaleMode.h
#include "Graphics/Upscaling/RenderTargetUpscaleMode.h"
#include "Graphics/ShaderProgram.h"

class MySharpUpscaleMode final : public RenderTargetUpscaleMode
{
  public:
    explicit MySharpUpscaleMode(std::string name = "my-sharp");

    void Upscale(const RenderTarget &source, int outputWidth, int outputHeight) const override;

  private:
    ShaderProgram upscaleShader;
};
```

```cpp
// MySharpUpscaleMode.cpp
#include "MySharpUpscaleMode.h"
#include "Graphics/RenderTarget.h"
#include "Graphics/Core/RenderState.h"

MySharpUpscaleMode::MySharpUpscaleMode(std::string name) : RenderTargetUpscaleMode(name)
{
    upscaleShader.SetShader("shader/upscaling/sharp.vert", "shader/upscaling/sharp.frag");
}

void MySharpUpscaleMode::Upscale(const RenderTarget &source, int outputWidth, int outputHeight) const
{
    // source            = the offscreen RT the scene (and post-process chain) was drawn into
    // outputWidth/Height = the final window resolution to blit into

    GraphicsRenderState::PrepareScreenPass(outputWidth, outputHeight);
    upscaleShader.Bind();
    source.GetTexture().texUnit(upscaleShader);
    source.GetTexture().Bind();
    source.RenderScreenQuad(outputWidth, outputHeight);
    source.GetTexture().Unbind();
    upscaleShader.Unbind();
}
```

Register on a renderer:

```cpp
renderer3D.RegisterUpscaleMode(std::make_unique<MySharpUpscaleMode>());
renderer3D.SetActiveUpscaleMode("my-sharp");
renderer3D.SetRenderScale(0.5f);   // auto-enables upscaling
```

**What the pipeline does around your `Upscale` call:**

1. `BeginPass` — resizes the internal `RenderTarget` to the render resolution, binds it.
2. All draw calls + post-process passes execute inside the RT.
3. `EndPass` — resets scissor and blend state, then calls `Upscale(rt, outputW, outputH)`.

### Full control — implement `IUpscaleMode` directly

Use this when you need to restrict the mode to a specific renderer type or fully control `SupportsRenderer`:

```cpp
class MyFullMode final : public IUpscaleMode
{
  public:
    std::string GetName() const noexcept override { return "my-full-mode"; }

    bool SupportsRenderer(const Renderer &renderer) const noexcept override
    {
        return renderer.IsRuntimeCompatible();
    }

    void Upscale(const RenderTarget &source, int outputWidth, int outputHeight) const override
    {
        source.BlitToScreen(outputWidth, outputHeight);
    }
};
```

### Advanced mode — temporal / AI upscaling

For techniques that need depth, motion vectors, history, or exposure, implement `IAdvancedUpscaleMode`. It inherits `IUpscaleMode`, so it can be used wherever a regular upscale mode is expected.

```cpp
#include "Graphics/Upscaling/IAdvancedUpscaleMode.h"

class MyTAAMode final : public IAdvancedUpscaleMode
{
  public:
    std::string GetName() const noexcept override { return "my-taa"; }

    bool SupportsRenderer(const Renderer &) const noexcept override { return true; }

    const UpscaleModeDesc &GetDescription() const noexcept override { return desc; }

    bool IsSupported(const GraphicsCapabilities &caps) const noexcept override
    {
        return caps.supportsComputeShaders;
    }

    bool Initialize(const IGraphicsDevice &device) override
    {
        // allocate history buffers, temporal accumulation targets, etc.
        return true;
    }

    void Shutdown() override
    {
        // free GPU resources
    }

    // Upscale() does NOT need to be overridden — IAdvancedUpscaleMode provides a default
    // that calls Execute() with colour-only input as a fallback. The renderer calls
    // Execute() directly with the full UpscaleInput when temporal resources are available.

    bool Execute(const UpscaleInput &input, UpscaleOutput &output) override
    {
        // input.color            — current frame colour texture
        // input.depth            — depth buffer
        // input.motionVectors    — per-pixel motion vectors
        // input.jitter           — sub-pixel jitter offset
        // input.sharpness        — post-upscale sharpening (0 = off)
        // input.deltaTimeSeconds — time since last frame
        // input.resetHistory     — true on scene cuts / teleports
        // input.historyColor     — previous upscaled frame

        // Write to:
        // output.output          — the upscaled result texture
        // output.newHistoryColor — history for the next frame

        return true;
    }

  private:
    UpscaleModeDesc desc{
        .name         = "my-taa",
        .quality      = UpscaleQualityMode::Quality,
        .requirements = {
            .needsColor         = true,
            .needsDepth         = true,
            .needsMotionVectors = true,
            .needsHistory       = true,
            .needsJitter        = true,
        },
        .supportsJitter = true,
    };
};
```

**`UpscaleInput` field reference:**

| Field                | Description                                        |
| -------------------- | -------------------------------------------------- |
| `color`            | Current rendered frame                             |
| `depth`            | Depth buffer                                       |
| `motionVectors`    | Screen-space per-pixel motion                      |
| `exposure`         | Optional exposure texture                          |
| `reactiveMask`     | Optional mask for particles / transparent geometry |
| `transparencyMask` | Optional transparency mask                         |
| `historyColor`     | Previous upscaled frame                            |
| `renderResolution` | Input (render) resolution                          |
| `outputResolution` | Output (window) resolution                         |
| `jitter`           | Sub-pixel jitter for TAA                           |
| `sharpness`        | Post-upscale sharpening strength                   |
| `deltaTimeSeconds` | Frame delta for temporal blending                  |
| `resetHistory`     | Signal a scene change / camera cut                 |

---

## Temporal Resource Setup (depth, motion vectors, history)

Before frame generation or temporal upscaling modes work correctly, the renderer needs to be told which extra resources to produce each frame. All of these are opt-in — enable only what your technique actually needs.

```cpp
// Make depth readable as a texture (off by default — renderbuffer is faster when not needed).
renderer3D.SetDepthAsTextureEnabled(true);

// Add a motion vector attachment (RG16F) as color attachment 1.
// Your scene shaders must write screen-space velocity into layout(location = 1) out vec2.
renderer3D.SetMotionVectorsEnabled(true);

// Tell the renderer the frame delta so frame-gen modes can interpolate correctly.
renderer3D.SetDeltaTime(deltaTime);

// Jitter is generated and applied automatically (Halton base-2/3 sequence) whenever
// NeedsTemporalResources() is true. No action needed in most cases.
// To override with a custom value (switches to Manual mode):
renderer3D.SetCameraJitter({jitterX, jitterY});
// To revert to automatic generation:
renderer3D.SetJitterMode(Renderer::JitterMode::Auto);
// Longer sequence = less repetition (default: 16 samples):
renderer3D.SetJitterSequenceLength(32);

// Call once on scene cuts or camera teleports to discard stale history.
renderer3D.ResetTemporalHistory();
```

### Motion vector GLSL

When `SetMotionVectorsEnabled(true)`, the scene render target has a second attachment at index 1 (`RG16F`). Your scene fragment shader writes the screen-space velocity of each pixel into it:

```glsl
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragMotion;  // screen-space velocity in [-1, 1]

void main()
{
    fragColor  = computeColor();
    // currentNDC - previousNDC, both in clip space → NDC
    fragMotion = currentNDCPos.xy - previousNDCPos.xy;
}
```

### What the renderer does automatically

Once these flags are set, `BeginPass` reconfigures the scene render target once (only when the attachment layout actually changes) and `EndPass`:

1. Runs post-process passes.
2. Snapshots the color attachment to an internal history buffer (at render resolution).
3. Runs the upscale mode.
4. Calls `frameGenMode->GenerateFrame(input, output)` with all available resources filled in.

---

## Implementing Frame Generation

Frame generation produces an interpolated frame between two rendered frames. Implement `IFrameGenerationMode` and register it on a renderer:

```cpp
renderer3D.SetFrameGenerationMode(std::make_unique<MyFrameGen>());
```

The mode lifecycle (`Initialize` / `Shutdown`) is already handled by the renderer — `SetFrameGenerationMode` calls `Initialize` on the new mode and `Shutdown` on the old one; the destructor calls `Shutdown` on the active mode.

The render call (`GenerateFrame`) is wired in `EndPass` after upscaling but **not yet active** — it requires depth buffer output, motion vectors, and previous-frame history to be plumbed through. The infrastructure is in place; the runtime integration is a future task.

```cpp
class MyFrameGen final : public IFrameGenerationMode
{
  public:
    const FrameGenerationModeDesc &GetDescription() const noexcept override { return desc; }

    bool IsSupported(const GraphicsCapabilities &caps) const noexcept override
    {
        return caps.supportsComputeShaders;
    }

    bool Initialize(const IGraphicsDevice &device) override
    {
        // allocate optical flow / warp buffers
        return true;
    }

    void Shutdown() override {}

    bool GenerateFrame(const FrameGenerationInput &input, FrameGenerationOutput &output) override
    {
        // input.currentColor     — most recently rendered frame
        // input.previousColor    — frame before that
        // input.depth            — depth for the current frame
        // input.motionVectors    — screen-space motion for the current frame
        // input.deltaTimeSeconds
        // input.resetHistory     — camera cut / scene change

        // output.generatedFrame  — write the interpolated frame here

        return true;
    }

  private:
    FrameGenerationModeDesc desc{
        .name               = "my-frame-gen",
        .quality            = FrameGenerationQualityMode::On,
        .needsDepth         = true,
        .needsMotionVectors = true,
        .needsHistory       = true,
    };
};
```

---

## Mesh

`Mesh` is a GPU geometry object with a shader, textures, a transform, and a model matrix buffer.

### Construction

```cpp
// vertices: interleaved float array
// indices:  triangle index list
// sizeAttrib: component count per attribute — e.g. {3, 3, 2} for (position, normal, uv)
Mesh mesh(vertices, indices, {3, 3, 2});

// With per-instance data:
Mesh mesh(vertices, indices, {3, 3, 2}, instanceData, {4, 4, 4, 4}); // 4 rows of mat4
```

Alternatively, default-construct and call `Initialize` later:

```cpp
Mesh mesh;
mesh.Initialize(vertices, indices, {3, 3, 2});
```

### Shader

```cpp
mesh.SetShader("shader/default.vert", "shader/default.frag");
mesh.IsShaderCompiled(); // false until the GPU context is ready
```

### Textures

```cpp
mesh.AddTexture(Texture("resources/albedo.png", "uAlbedo", /*slot=*/0));
mesh.AddTexture("resources/albedo.png", "uAlbedo", TextureFormat::RGBA8, TexturePixelType::UnsignedByte);
```

The texture uniform name must match the `sampler2D` name declared in the shader.

### Transform

```cpp
mesh.SetPosition({x, y, z});
mesh.SetScale({sx, sy, sz});
mesh.SetRotation({rx, ry, rz}); // Euler angles in degrees
```

These are uploaded automatically to the model matrix buffer at binding point `MESH_MODEL_BINDING_POINT` (3) when `Render()` is called. Call `UploadTransform()` explicitly if you need the GPU buffer updated outside of `Render`.

### Uniforms

Set a named uniform that is cached and re-applied automatically each time the shader is rebound:

```cpp
float color[4] = {1.0f, 0.5f, 0.0f, 1.0f};
mesh.SetUniform4f("uColor", color);

int flag[1] = {1};
mesh.SetUniform1i("uEnabled", flag);

float matrix[16] = { /* column-major mat4 */ };
mesh.SetUniformMatrix4f("uTransform", matrix);
```

Available variants: `SetUniform1/2/3/4f`, `SetUniform1/2/3/4i`, `SetUniformMatrix4f`.

### Rendering

```cpp
mesh.Render(camera);   // binds shader + textures + buffers, draws, unbinds everything
mesh.Draw();           // draws without binding anything (shader must already be bound)
mesh.Draw(true);       // wireframe draw
```

For custom pipelines (e.g. UI geometry):

```cpp
mesh.BindShader();
mesh.BindGeometry();
// ... custom draw call ...
mesh.UnbindGeometry();
mesh.UnbindShader();
```

---

## Sprite

`Sprite` wraps a `Mesh` for simple 2D quad rendering. It is the building block for UI elements.

```cpp
Sprite sprite;
sprite.SetShader("shader/UI/default.vert", "shader/UI/default.frag");

SpriteDrawParams params;
params.offset        = {100.0f, 50.0f};    // screen position (top-left corner)
params.scale         = {200.0f, 100.0f};   // width, height in pixels
params.containerSize = {800.0f, 600.0f};   // visible area of the parent container
params.color         = {1.0f, 1.0f, 1.0f, 1.0f};

sprite.Draw(params);             // no texture
sprite.Draw(params, &myTexture); // with texture
```

`SpriteDrawParams` does not currently have a rotation field. If per-sprite rotation is needed, the transform can be applied by passing a pre-rotated model matrix via a shader uniform, or by adding a `float rotation` (radians) field to `SpriteDrawParams` and building a 2D rotation matrix in the sprite's vertex shader.

`Renderer2D::DrawSprite` wraps `BeginCanvasPass` / `sprite.Draw` / `EndCanvasPass` for you:

```cpp
rendererUI.DrawSprite(sprite, params, &myTexture);
```

See the [Scrollable content](#scrollable-content-scrolloffset--contentsize) section above for `scrollOffset` and `contentSize`.

---

## Texture

`Texture` is a move-only GPU texture handle. It cannot be copied — use `Copy()` for an explicit duplicate.

### From file

```cpp
Texture tex("resources/images/albedo.png", "uAlbedo", /*slot=*/0);
Texture tex("resources/images/normal.png", "uNormal", 1,
            TextureFormat::RGBA8, TexturePixelType::UnsignedByte, TextureFilterMode::Linear);
```

### From raw data

```cpp
Texture tex(pixelData, width, height, "uAlbedo", /*slot=*/0,
            TextureFormat::RGBA8, TexturePixelType::UnsignedByte);
```

### Framebuffer colour attachment

```cpp
texture.SetFramebufferTexture("uTexture", /*slot=*/0, width, height, framebufferHandle);
texture.ResizeFramebufferTexture(newWidth, newHeight);
```

### Binding

```cpp
texture.texUnit(shaderProgram); // writes the sampler uniform to point at this texture's slot
texture.Bind();
texture.Unbind();
```

### Querying

```cpp
texture.GetWidth(); texture.GetHeight();
texture.GetFormat();       // TextureFormat::RGBA8, Depth24Stencil8, etc.
texture.GetID();           // raw GPU texture handle
texture.GetSlot();         // texture unit slot
texture.GetUniformName();  // the name string given at construction
```

---

## ShaderProgram

### Compilation

```cpp
ShaderProgram shader("shader/default.vert", "shader/default.frag");
```

Or lazily:

```cpp
ShaderProgram shader;
shader.SetShader("shader/default.vert", "shader/default.frag");
// CompileShader() is called automatically on the first Bind()
```

From source strings (no files):

```cpp
shader.SetShaderCode(vertexSourceString, fragmentSourceString);
shader.CompileShader();
```

`IsCompiled()` returns `true` once the program is linked on the GPU.

### Uniforms

```cpp
shader.Bind();

int loc = shader.GetUniformLocation("uTime");
float t = 1.23f;
shader.SetUniformFloats(loc, &t, 1);

int loc2 = shader.GetUniformLocation("uColor");
float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
shader.SetUniformFloats(loc2, color, 4);

int loc3 = shader.GetUniformLocation("uModel");
float mat[16] = { /* column-major mat4 */ };
shader.SetUniformMatrix4(loc3, mat);

shader.Unbind();
```

### Binary cache

`ShaderProgram` can serialize the compiled GPU program to a binary blob and reload it next run, skipping source compilation. This is managed automatically by `ShaderLibrary` (see below), but you can also drive it manually:

```cpp
// Save after compilation:
std::ofstream out("shader.bin", std::ios::binary);
shader.SaveBinary(out);

// Load on next run (skips compilation):
ShaderProgram shader;
std::ifstream in("shader.bin", std::ios::binary);
if (!shader.LoadBinary(in))
{
    shader.SetShader("shader/default.vert", "shader/default.frag"); // fallback
}
```

`SaveBinary` / `LoadBinary` return `false` when the active backend does not support program binaries (non-OpenGL) or when no program is compiled yet. Always have a compilation fallback.

---

## ShaderLibrary

`ShaderLibrary` is a singleton that compiles each unique shader pair once and returns a shared reference on every subsequent call. Multiple objects holding the same shader never allocate a second GPU program.

### Basic usage

```cpp
// Compile all shaders up front at startup:
ShaderLibrary::Instance().PreWarm({
    {"shader/default.vert",  "shader/default.frag"},
    {"shader/terrain.vert",  "shader/terrain.frag"},
});

// Fetch a shader anywhere — returns immediately if already cached:
const ShaderProgram &shader = ShaderLibrary::Instance().Get(
    "shader/default.vert", "shader/default.frag");

shader.Bind();
// ... uniforms + draw ...
shader.Unbind();
```

`Get` compiles the shader on the first call. Every subsequent call for the same path pair returns a reference to the same compiled program (the underlying GPU object is shared via `shared_ptr`).

### Disk binary cache

Enable to skip GLSL compilation entirely on subsequent runs. The library computes a hash of the combined shader sources, stores the compiled binary under `res/cache/shaders/<hash>.shbc`, and validates the hash on load — so stale cache files are detected and recompiled automatically.

```cpp
ShaderLibrary::Instance().SetBinaryCacheEnabled(true);
ShaderLibrary::Instance().SetBinaryCacheDirectory("res/cache/shaders"); // default

// Now Get() will:
//   1. Read the source files, compute the hash.
//   2. Look for res/cache/shaders/<hash>.shbc.
//   3. Load from cache if found and valid; compile and save otherwise.
```

Binary cache is only effective for OpenGL (the only backend that supports `GL_ARB_get_program_binary`). On other backends `Get` falls back to standard compilation and `SetBinaryCacheEnabled` has no effect.

### Clearing the cache

```cpp
ShaderLibrary::Instance().Clear(); // releases all in-memory entries
                                   // GPU programs are freed when the last ShaderProgram
                                   // holding a reference is destroyed
```

### Writing shader source

Shaders live in `resources/shader/`. The standard buffer binding points are:

| Binding point | Macro                        | Content                                      |
| ------------- | ---------------------------- | -------------------------------------------- |
| 0             | `CAMERA_BINDING_POINT`     | Camera matrices (combined, view, projection) |
| 1             | `LIGHT_BINDING_POINT`      | Light storage buffer (via `LightManager`)  |
| 2             | `SKYBOX_BINDING_POINT`     | Skybox uniform buffer                        |
| 3             | `MESH_MODEL_BINDING_POINT` | Per-mesh model matrix buffer                 |

A typical 3D vertex shader:

```glsl
#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(std140, binding = 0) uniform CameraBlock {
    mat4 camMatrix;
    mat4 view;
    mat4 projection;
};

layout(std140, binding = 3) uniform ModelBlock {
    mat4 model;
};

out vec3 fragNormal;
out vec2 fragTexCoord;

void main()
{
    fragNormal   = mat3(transpose(inverse(model))) * aNormal;
    fragTexCoord = aTexCoord;
    gl_Position  = camMatrix * model * vec4(aPos, 1.0);
}
```

A typical 3D fragment shader:

```glsl
#version 430 core

in vec3 fragNormal;
in vec2 fragTexCoord;

uniform sampler2D uAlbedo;   // slot 0 — matches Texture constructor name arg
uniform vec4      uColor;    // set via Mesh::SetUniform4f

out vec4 fragColor;

void main()
{
    vec4 albedo = texture(uAlbedo, fragTexCoord);
    fragColor   = albedo * uColor;
}
```

A post-process / upscale fragment shader receives the scene via `screenTexture`:

```glsl
#version 430 core

in vec2 texCoords;

uniform sampler2D screenTexture;

out vec4 fragColor;

void main()
{
    vec3 color = texture(screenTexture, texCoords).rgb;
    // ... effect ...
    fragColor = vec4(color, 1.0);
}
```

---

## Buffer

`Buffer` is a move-only GPU buffer (uniform, storage, vertex staging). It is not copyable.

### Creation

```cpp
Buffer ubo(BufferUsage::Uniform, sizeof(MyData), CAMERA_BINDING_POINT);
Buffer ssbo(BufferUsage::Storage, sizeof(LightBlock) * MAX_LIGHTS, LIGHT_BINDING_POINT, /*cpuWritable=*/true);
Buffer staging(BufferUsage::Staging, dataSize);
```

Or lazily:

```cpp
Buffer buf;
buf.Initialize(BufferUsage::Uniform, sizeof(MyData), MY_BINDING_POINT);
```

### Uniform vs Storage buffers

- **`BufferUsage::Uniform`** — fast, read-only in shaders, max ~64 KB per block. Use for per-frame constants (camera, time, per-object settings).
- **`BufferUsage::Storage`** — large (gigabytes), read-write from shaders. Use for arrays of per-object data (lights, particles, instances). OpenGL guarantees at least 8 simultaneous storage buffers per shader stage.

### Uploading data

```cpp
buf.UploadData(&myData, sizeof(myData));                         // from offset 0
buf.UploadData(&partialData, sizeof(partialData), byteOffset);   // from a byte offset
```

### Binding

```cpp
buf.Bind();                 // bind to the buffer target
buf.BindToBindingPoint();   // bind to the binding point given at construction
buf.Unbind();
```

### Resize

```cpp
buf.Resize(newSize);               // discard existing content
buf.ResizePreserveData(newSize);   // keep existing bytes
```

### Mapped writes

```cpp
void *ptr = buf.MapBuffer(BufferMapAccess::WriteOnly);
std::memcpy(ptr, &newData, sizeof(newData));
buf.UnmapBuffer();
```

---

## RenderTarget

`RenderTarget` is a move-only offscreen framebuffer (colour + depth buffer). It is not copyable.

**When to use it**: when you need to render a scene and then use the result as a texture — shadow maps, reflections, in-world camera screens, portals, or any effect that reads "what was just rendered". The Renderer classes manage their own internal `RenderTarget` automatically; only create one yourself for custom multi-pass techniques.

`Resize(w, h)` handles both the initial allocation and any subsequent dimension changes — call it whenever you need a render target at a specific size without checking `IsInitialized` first.

### Creation

```cpp
RenderTarget rt;
rt.Resize(1920, 1080);   // allocates on first call, resizes on subsequent calls

// Or at construction:
RenderTarget rt(1920, 1080);
```

### Rendering into it

```cpp
rt.Bind();
// ... draw calls render into rt instead of the screen ...
rt.Unbind();
```

### Compositing back to the screen

```cpp
rt.BlitToScreen(outputWidth, outputHeight);    // blit colour buffer to default framebuffer
rt.BlitToRenderTarget(otherRt);               // blit to another RenderTarget
rt.CopyFromScreen(srcWidth, srcHeight);        // capture the default framebuffer into this RT

rt.RenderScreenQuad();                         // draw as fullscreen quad (built-in shader)
rt.RenderScreenQuad(frameWidth, frameHeight);  // same, with explicit viewport size
```

### Querying

```cpp
rt.IsInitialized();              // false until the first Resize() / Init() call
rt.GetWidth(); rt.GetHeight();
rt.GetTexture();                 // Texture& for attachment 0 — shorthand for GetTexture(0)
rt.GetTexture(index);            // Texture& for a specific attachment by index
rt.GetColorAttachmentCount();    // number of color attachments
rt.GetTextureID();               // raw GPU handle for attachment 0
```

---

## Multiple Render Targets (MRT)

MRT lets you write to several textures simultaneously from a single draw pass. Common uses: G-buffer (colour + normals + object IDs), shadow maps, post-process ping-pong, readable depth for SSAO or selection.

### Creating an MRT

```cpp
RenderTargetDesc desc;
desc.extent           = {1920, 1080};
desc.colorAttachments = {
    TextureFormat::RGBA8,   // attachment 0 — standard colour
    TextureFormat::R32UI,   // attachment 1 — per-pixel object ID (32-bit uint)
};
desc.hasDepthBuffer  = true;
desc.depthAsTexture  = true;   // depth readable in shaders (e.g. for SSAO)

RenderTarget rt(desc);
// or: rt.Init(desc);
```

### Accessing attachments

```cpp
rt.GetTexture(0);            // RGBA8 colour
rt.GetTexture(1);            // R32UI object IDs
rt.TryGetDepthTexture();     // Texture* — nullptr when depthAsTexture == false
rt.GetColorAttachmentCount(); // 2
```

### Writing from a shader

In GLSL, declare one `out` per attachment (location matches attachment index):

```glsl
layout(location = 0) out vec4  fragColor;    // → attachment 0 (RGBA8)
layout(location = 1) out uint  fragObjectId; // → attachment 1 (R32UI)

void main()
{
    fragColor    = computeColor();
    fragObjectId = objectId;  // a flat uint passed from the vertex shader
}
```

### Sampling in a later pass

```cpp
// Bind the RT's attachments as inputs to a post-process shader:
rt.GetTexture(0).texUnit(shader);   // writes sampler uniform for attachment 0
rt.GetTexture(0).Bind();

rt.GetTexture(1).texUnit(shader);   // writes sampler uniform for attachment 1
rt.GetTexture(1).Bind();

if (Texture *depth = rt.TryGetDepthTexture())
{
    depth->texUnit(shader);
    depth->Bind();
}
```

```glsl
uniform sampler2D attachment0;   // colour
uniform usampler2D attachment1;  // object IDs — note usampler2D for R32UI
uniform sampler2D depthTexture;

void main()
{
    vec4  color    = texture(attachment0, texCoords);
    uint  objectId = texture(attachment1, texCoords).r;
    float depth    = texture(depthTexture, texCoords).r;
    // ...
}
```

### Object picking

Read back the object ID at a specific pixel (e.g. under the mouse cursor) on the CPU:

```cpp
// After rendering, read one pixel from attachment 1 (R32UI):
int mouseX = ..., mouseY = ...;
uint32_t objectId = rt.ReadPixelUInt(/*attachmentIndex=*/1, mouseX, mouseY);
```

`ReadPixelUInt` accepts screen-space coordinates (top-left origin) and handles the OpenGL Y-flip internally. Returns 0 when the render target is not initialized or the backend does not support pixel readback.

For standard RGBA8 colour attachments:
```cpp
glm::uvec4 color = rt.ReadPixelRGBA8(/*attachmentIndex=*/0, mouseX, mouseY);
// color.r, .g, .b, .a in [0, 255]
```

### Available texture formats

| `TextureFormat` | Use case |
|---|---|
| `RGBA8` | Standard colour (default) |
| `R8` | Single-channel mask or AO |
| `R32UI` | Object ID, draw call ID — integer, requires `usampler2D` in GLSL |
| `RG16F` | Two float16 components — screen-space motion vectors (velocity XY) |
| `Depth32Float` | Readable depth — for SSAO, shadow lookup, depth-of-field |
| `Depth24Stencil8` | Depth + stencil — when stencil masking is needed |

---

## Camera

```cpp
Camera camera;
camera.Initialize(window.GetWidthptr(), window.GetHeightptr(), glm::vec3(0.0f, 1.0f, 0.0f));
camera.SetFOV(75.0f);
camera.SetNearPlane(0.1f);
camera.SetFarPlane(1000.0f);
```

Per frame:

```cpp
camera.Inputs(window.GetWindow(), deltaTime);  // WASD movement + mouse look (dev / fly camera)
camera.UpdateMatrix();                          // recomputes view and projection matrices

renderer3D.SetCamera(camera);  // applies current jitter, recomputes matrix, uploads to binding point 0
```

The camera can also be driven programmatically without `Inputs`:

```cpp
camera.SetPosition({x, y, z});
camera.SetOrientation({dx, dy, dz});  // normalized look direction
camera.UpdateMatrix();
```

Direct buffer upload (when not using `Renderer3D::SetCamera`):

```cpp
camera.Bind(); // uploads combined/view/projection matrices to CAMERA_BINDING_POINT (0)
```

When a temporal upscaling or frame generation mode is active, `SetCamera` automatically injects the renderer's current jitter into the projection matrix via `camera.SetJitter(...)` + `camera.UpdateMatrix()`. No manual jitter handling is needed. To disable jitter for a specific frame: `renderer3D.SetCameraJitter({0, 0})`.

---

## Lighting

```cpp
LightManager lights;
lights.Initialize(); // allocates the GPU buffer — call once after the graphics runtime is bound

lights.SetAmbientLight({1.0f, 1.0f, 1.0f}, /*strength=*/0.1f);
lights.AddLight(lght::DirectionalLight({0.0f, -1.0f, -0.5f}, {1, 1, 1}, /*strength=*/1.0f));
lights.AddLight(lght::PointLight({5, 3, 0}, {1, 0.8f, 0.6f}, /*strength=*/2.0f, /*a=*/0.05f, /*b=*/0.01f));
lights.AddLight(lght::SpotLight(pos, dir, color, strength, outerCone, innerCone, a, b));
```

Per frame (call before any mesh that uses lighting):

```cpp
lights.UploadChanges();  // only uploads lights that changed since last call
lights.Bind();           // binds the buffer to LIGHT_BINDING_POINT (1)
```

Modifying a light after creation:

```cpp
lights[0].color = {1.0f, 0.0f, 0.0f};
lights.SetLight(0, lights[0]); // marks it dirty so UploadChanges uploads it
```

---

## Render State Utilities

`GraphicsRenderState` provides backend-agnostic wrappers for common GPU state changes. Use these instead of calling `gl*` directly:

| Function | What it does |
|---|---|
| `BindDefaultFramebuffer()` | Switch rendering back to the window surface (screen) |
| `BindFramebuffer(id)` | Bind a specific offscreen framebuffer by its GPU handle |
| `SetViewport(x,y,w,h)` | Set the drawing region within the current framebuffer |
| `ClearColor(color)` | Set the color used by the next clear operation |
| `ClearColorBuffer()` | Erase the color attachment to the current clear color |
| `ClearBuffers(color, depth)` | Erase color and/or depth attachments in one call |
| `ClearTransparentColorBuffer()` | Clear to `(0,0,0,0)` — transparent black, used for UI render targets |
| `SetDepthTest(bool)` | Enable/disable depth testing (fragments behind geometry are discarded) |
| `SetWireframe(bool)` | Toggle polygon fill mode (solid vs wireframe lines) |
| `SetBlend(bool)` | Enable/disable alpha blending (required for transparent UI, particles) |
| `SetAlphaBlend()` | Standard alpha blend: `srcAlpha * src + (1 - srcAlpha) * dst` |
| `SetPremultipliedAlphaBlend()` | Pre-multiplied alpha: `src + (1 - srcAlpha) * dst` — common for text rendering |
| `SetScissorTest(bool)` | Enable/disable the scissor rectangle |
| `SetScissor(x,y,w,h)` | Define the scissor rectangle (pixels outside are discarded) |
| `CaptureFramebufferState()` | Snapshot current framebuffer, viewport, blend, depth, scissor state |
| `RestoreFramebufferState(state)` | Restore a previously captured state |
| `PrepareScreenPass(w,h)` | Shorthand: bind default framebuffer + set viewport + disable depth + enable alpha blend |

```cpp
GraphicsRenderState::BindDefaultFramebuffer();
GraphicsRenderState::SetViewport(0, 0, width, height);
GraphicsRenderState::SetDepthTest(true);
GraphicsRenderState::SetBlend(true);
GraphicsRenderState::SetAlphaBlend();
GraphicsRenderState::SetScissorTest(true);
GraphicsRenderState::SetScissor(x, y, w, h);
GraphicsRenderState::PrepareScreenPass(width, height);
```

Save and restore state around a nested pass:

```cpp
auto saved = GraphicsRenderState::CaptureFramebufferState();
// ... nested rendering ...
GraphicsRenderState::RestoreFramebufferState(saved);
```

### Error checking

In `DEBUG` builds, log any pending backend errors after a draw sequence:

```cpp
GRAPHICS_CHECK_ERRORS();                 // no context label
GRAPHICS_CHECK_ERRORS_M("after blit");  // with a label for log readability
```

Both macros are no-ops in Release builds.

---

## Implementing a Custom Renderer

To add a new renderer (e.g. `RendererVR`), subclass `Renderer`, pass the required API to the base constructor, and implement four methods:

```cpp
class RendererVR final : public Renderer
{
  public:
    // Pass the required backend API to the base constructor.
    RendererVR() : Renderer(GraphicsAPI::Vulkan) {}

  protected:
    // Called by BeginPass() after the RT is bound (or the default FB if no RT is needed).
    // Set up render state your renderer needs (viewport, depth test, blend, etc.).
    void OnBeginPass() override { /* ... */ }

    // Called by EndPass() after upscaling. Reset any renderer-specific state.
    void OnEndPass() override { /* ... */ }

    // Called by Clear() after the runtime/extent guard. Renderer-specific clear only.
    void OnClear(const glm::vec4 &clearColor, bool clearDepth) const override { /* ... */ }

    // Return the RenderTarget owned by this renderer.
    // Used by BeginPass (bind), EndPass (post-process + upscale).
    RenderTarget &GetRenderTarget() override            { return renderTarget; }
    const RenderTarget &GetRenderTarget() const override { return renderTarget; }

  private:
    RenderTarget renderTarget;
};
```

The base constructor automatically registers `BilinearBlitUpscaleMode` and sets it as the active mode. There is nothing else to do in the subclass constructor unless you want to register additional upscale modes.

---

## Skeletal Animation — Design Plan

Not yet implemented. The planned approach when it is added:

| Type | Role |
|---|---|
| `BoneTransform` | TRS (translation, rotation, scale) for one bone |
| `Skeleton` | Named bone hierarchy + bind-pose palette |
| `AnimationClip` | Per-bone keyframe tracks (position, rotation, scale) sampled at runtime |
| `SkinnedMesh` | Extends `Mesh`; adds joint index / weight vertex attributes + `BONE_PALETTE_BINDING_POINT` (4) |

The CPU evaluates the clip at the current time, computes final bone matrices (animated TRS × inverse-bind-pose), and uploads them to a `Buffer` at binding point `4`. The vertex shader reads the palette and blends up to 4 bones per vertex using weights from the vertex data:

```glsl
layout(std430, binding = 4) readonly buffer BonePalette {
    mat4 bones[];
};

// in vertex shader:
mat4 skin = weights.x * bones[joints.x]
           + weights.y * bones[joints.y]
           + weights.z * bones[joints.z]
           + weights.w * bones[joints.w];
gl_Position = camMatrix * model * skin * vec4(aPos, 1.0);
```

---

## Not Implemented Yet

- Vulkan backend
- Metal backend
- DLSS / FSR / XeSS (`IAdvancedUpscaleMode` interface is ready)
- Frame generation algorithm implementation (`IFrameGenerationMode` interface, lifecycle, resource plumbing, and `GenerateFrame` call are all wired; only the actual interpolation algorithm inside `GenerateFrame` is missing)
- Acceleration structures (interfaces exist, no backend)
- Skeletal animation (`SkinnedMesh`, `Skeleton`, `AnimationClip` — design plan above)
- Float / multi-component pixel readback (`ReadPixelUInt` handles R32UI; other formats are not yet exposed through the façade)
