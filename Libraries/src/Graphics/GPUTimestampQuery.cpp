#include "Graphics/GPUTimestampQuery.h"

#include "Graphics/Core/GraphicsRuntime.h"

#include <utility>

GPUTimestampQuery::GPUTimestampQuery(const GPUTimestampQueryCreateInfo &createInfo) { Initialize(createInfo); }

GPUTimestampQuery::GPUTimestampQuery(GPUTimestampQuery &&other) noexcept { Swap(other); }

GPUTimestampQuery &GPUTimestampQuery::operator=(GPUTimestampQuery &&other) noexcept
{
    if (this != &other)
    {
        Destroy();
        Swap(other);
    }
    return *this;
}

GPUTimestampQuery::~GPUTimestampQuery() { Destroy(); }

bool GPUTimestampQuery::Initialize(const GPUTimestampQueryCreateInfo &createInfo)
{
    Destroy();

    debugName = createInfo.debugName;

    const IGraphicsDevice *device = TryGetActiveGraphicsDevice();
    if (device == nullptr)
    {
        return false;
    }

    resource = device->CreateTimestampQuery(createInfo);
    return resource != nullptr;
}

void GPUTimestampQuery::Destroy()
{
    resource.reset();
    debugName.clear();
}

GraphicsAPI GPUTimestampQuery::GetAPI() const noexcept
{
    return resource != nullptr ? resource->GetAPI() : GraphicsAPI::OpenGL;
}

std::string_view GPUTimestampQuery::GetDebugName() const noexcept
{
    return resource != nullptr ? resource->GetDebugName() : std::string_view(debugName);
}

void GPUTimestampQuery::Begin()
{
    if (resource != nullptr)
    {
        resource->Begin();
    }
}

void GPUTimestampQuery::End()
{
    if (resource != nullptr)
    {
        resource->End();
    }
}

bool GPUTimestampQuery::IsReady() const
{
    return resource != nullptr && resource->IsReady();
}

std::chrono::nanoseconds GPUTimestampQuery::GetElapsedTime() const
{
    return resource != nullptr ? resource->GetElapsedTime() : std::chrono::nanoseconds::zero();
}

void GPUTimestampQuery::Swap(GPUTimestampQuery &other) noexcept
{
    std::swap(resource, other.resource);
    std::swap(debugName, other.debugName);
}
