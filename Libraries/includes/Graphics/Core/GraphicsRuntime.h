#pragma once

#include "Graphics/Core/GraphicsBackend.h"

struct GraphicsRuntimeBindings
{
    GraphicsAPI api = GraphicsAPI::OpenGL;
    const IGraphicsBackend *backend = nullptr;
    const IGraphicsDevice *device = nullptr;
};

void BindGraphicsRuntime(const GraphicsRuntimeBindings &bindings) noexcept;
void ClearGraphicsRuntime() noexcept;

GraphicsAPI GetActiveGraphicsAPI() noexcept;
const IGraphicsBackend *TryGetActiveGraphicsBackend() noexcept;
const IGraphicsDevice *TryGetActiveGraphicsDevice() noexcept;
bool IsGraphicsAPIActive(GraphicsAPI api) noexcept;
