#include "Graphics/Core/GraphicsRuntime.h"

namespace
{

GraphicsRuntimeBindings runtimeBindings{};

} // namespace

void BindGraphicsRuntime(const GraphicsRuntimeBindings &bindings) noexcept { runtimeBindings = bindings; }

void ClearGraphicsRuntime() noexcept { runtimeBindings = {}; }

GraphicsAPI GetActiveGraphicsAPI() noexcept { return runtimeBindings.api; }

const IGraphicsBackend *TryGetActiveGraphicsBackend() noexcept { return runtimeBindings.backend; }

const IGraphicsDevice *TryGetActiveGraphicsDevice() noexcept { return runtimeBindings.device; }

bool IsGraphicsAPIActive(GraphicsAPI api) noexcept { return runtimeBindings.backend != nullptr && runtimeBindings.api == api; }
