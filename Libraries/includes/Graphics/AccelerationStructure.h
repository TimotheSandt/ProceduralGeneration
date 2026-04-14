#pragma once

#include "Graphics/Core/GraphicsResources.h"

#include <memory>
#include <string_view>

class AccelerationStructure
{
  public:
    AccelerationStructure() = default;
    explicit AccelerationStructure(const AccelerationStructureCreateInfo &createInfo);

    AccelerationStructure(const AccelerationStructure &) = delete;
    AccelerationStructure &operator=(const AccelerationStructure &) = delete;

    AccelerationStructure(AccelerationStructure &&) noexcept;
    AccelerationStructure &operator=(AccelerationStructure &&) noexcept;

    ~AccelerationStructure();

    bool Initialize(const AccelerationStructureCreateInfo &createInfo);
    void Destroy();

    bool IsInitialized() const noexcept { return resource != nullptr; }
    GraphicsAPI GetAPI() const noexcept;
    std::string_view GetDebugName() const noexcept;
    const AccelerationStructureDesc &GetDescription() const noexcept;

  private:
    void Swap(AccelerationStructure &other) noexcept;

    std::unique_ptr<IAccelerationStructureResource> resource;
    AccelerationStructureDesc desc{};
    std::string debugName;
};
