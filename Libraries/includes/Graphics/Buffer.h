#pragma once

#include <glad/glad.h>

#include "Graphics/Core/GraphicsResources.h"

#include <memory>

#define CAMERA_BINDING_POINT 0
#define LIGHT_BINDING_POINT 1
#define SKYBOX_BINDING_POINT 2
#define MESH_MODEL_BINDING_POINT 3

class Buffer
{
  public:
    Buffer() = default;
    Buffer(BufferUsage usage, size_t size, GLuint bindingPoint = 0, bool cpuWritable = true);
    ~Buffer();

    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;

    Buffer(Buffer &&) noexcept;
    Buffer &operator=(Buffer &&) noexcept;

    bool Initialize(BufferUsage usage, size_t size, GLuint bindingPoint = 0, bool cpuWritable = true);
    void Destroy();

    void Bind() const;
    void BindToBindingPoint() const;
    void Unbind() const;

    void UploadData(const void *data, size_t size, size_t offset = 0) const;
    void Resize(size_t newSize);
    void ResizePreserveData(size_t newSize);

    void *MapBuffer(BufferMapAccess access = BufferMapAccess::ReadWrite);
    void UnmapBuffer() const;

    bool IsInitialized() const { return ID != 0; }
    GLuint GetID() const { return ID; }
    size_t GetSize() const { return desc.sizeInBytes; }
    GLuint GetBindingPoint() const { return bindingPoint; }
    BufferUsage GetUsage() const { return desc.usage; }

  private:
    void Swap(Buffer &other) noexcept;

  private:
    GLuint ID = 0;
    GLuint bindingPoint = 0;
    BufferDesc desc{};
    std::unique_ptr<IBufferResource> backendBuffer;
};
