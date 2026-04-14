#pragma once

#include "Graphics/Core/GraphicsResources.h"

#include <chrono>
#include <memory>
#include <string_view>

class GPUTimestampQuery
{
  public:
    GPUTimestampQuery() = default;
    explicit GPUTimestampQuery(const GPUTimestampQueryCreateInfo &createInfo);

    GPUTimestampQuery(const GPUTimestampQuery &) = delete;
    GPUTimestampQuery &operator=(const GPUTimestampQuery &) = delete;

    GPUTimestampQuery(GPUTimestampQuery &&) noexcept;
    GPUTimestampQuery &operator=(GPUTimestampQuery &&) noexcept;

    ~GPUTimestampQuery();

    bool Initialize(const GPUTimestampQueryCreateInfo &createInfo);
    void Destroy();

    bool IsInitialized() const noexcept { return resource != nullptr; }
    GraphicsAPI GetAPI() const noexcept;
    std::string_view GetDebugName() const noexcept;

    void Begin();
    void End();
    bool IsReady() const;
    std::chrono::nanoseconds GetElapsedTime() const;

  private:
    void Swap(GPUTimestampQuery &other) noexcept;

    std::unique_ptr<IGPUTimestampQueryResource> resource;
    std::string debugName;
};
