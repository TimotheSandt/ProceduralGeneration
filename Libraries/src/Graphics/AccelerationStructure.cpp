#include "Graphics/AccelerationStructure.h"

#include "Graphics/Core/GraphicsRuntime.h"

#include <utility>

AccelerationStructure::AccelerationStructure(const AccelerationStructureCreateInfo &createInfo) { Initialize(createInfo); }

AccelerationStructure::AccelerationStructure(AccelerationStructure &&other) noexcept { Swap(other); }

AccelerationStructure &AccelerationStructure::operator=(AccelerationStructure &&other) noexcept
{
    if (this != &other)
    {
        Destroy();
        Swap(other);
    }
    return *this;
}

AccelerationStructure::~AccelerationStructure() { Destroy(); }

bool AccelerationStructure::Initialize(const AccelerationStructureCreateInfo &createInfo)
{
    Destroy();

    desc = createInfo.desc;
    debugName = createInfo.debugName;

    const IGraphicsDevice *device = TryGetActiveGraphicsDevice();
    if (device == nullptr || !device->GetCapabilities().supportsAccelerationStructures)
    {
        return false;
    }

    resource = device->CreateAccelerationStructure(createInfo);
    return resource != nullptr;
}

void AccelerationStructure::Destroy()
{
    resource.reset();
    desc = {};
    debugName.clear();
}

GraphicsAPI AccelerationStructure::GetAPI() const noexcept
{
    return resource != nullptr ? resource->GetAPI() : GraphicsAPI::OpenGL;
}

std::string_view AccelerationStructure::GetDebugName() const noexcept
{
    return resource != nullptr ? resource->GetDebugName() : std::string_view(debugName);
}

const AccelerationStructureDesc &AccelerationStructure::GetDescription() const noexcept
{
    return resource != nullptr ? resource->GetDescription() : desc;
}

void AccelerationStructure::Swap(AccelerationStructure &other) noexcept
{
    std::swap(resource, other.resource);
    std::swap(desc, other.desc);
    std::swap(debugName, other.debugName);
}
