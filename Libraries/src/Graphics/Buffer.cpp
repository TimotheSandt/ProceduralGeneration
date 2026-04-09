#include "Buffer.h"

#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"
#include "Graphics/Core/GraphicsRuntime.h"

#include <algorithm>
#include <cstring>
#include <vector>

Buffer::Buffer(BufferUsage usage, size_t size, std::uint32_t bindingPoint, bool cpuWritable)
{
    this->Initialize(usage, size, bindingPoint, cpuWritable);
}

Buffer::~Buffer() { this->Destroy(); }

Buffer::Buffer(Buffer &&other) noexcept { this->Swap(other); }

Buffer &Buffer::operator=(Buffer &&other) noexcept
{
    if (this != &other)
    {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

void Buffer::Swap(Buffer &other) noexcept
{
    std::swap(this->ID, other.ID);
    std::swap(this->bindingPoint, other.bindingPoint);
    std::swap(this->desc, other.desc);
    std::swap(this->backendBuffer, other.backendBuffer);
}

bool Buffer::Initialize(BufferUsage usage, size_t size, std::uint32_t bindingPoint, bool cpuWritable)
{
    this->Destroy();

    this->bindingPoint = bindingPoint;
    this->desc = {.usage = usage, .sizeInBytes = size, .cpuWritable = cpuWritable};

    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr && device->GetAPI() == GraphicsAPI::OpenGL)
    {
        std::unique_ptr<IBufferResource> resource = device->CreateBuffer({.desc = this->desc, .debugName = "graphics_buffer"});
        if (auto *openGLResource = dynamic_cast<OpenGLBufferResource *>(resource.get()); openGLResource != nullptr)
        {
            this->ID = openGLResource->GetBufferID();
            this->backendBuffer = std::move(resource);
            if (this->ID != 0)
            {
                return true;
            }
            this->backendBuffer.reset();
        }
    }

    return false;
}

void Buffer::Destroy()
{
    if (backendBuffer != nullptr)
    {
        backendBuffer.reset();
        ID = 0;
        desc.sizeInBytes = 0;
        return;
    }

    ID = 0;
    desc.sizeInBytes = 0;
}

void Buffer::Bind() const
{
    if (backendBuffer == nullptr)
    {
        return;
    }
    backendBuffer->Bind();
}

void Buffer::BindToBindingPoint() const
{
    if (backendBuffer == nullptr)
    {
        return;
    }
    backendBuffer->BindToBindingPoint(bindingPoint);
}

void Buffer::Unbind() const
{
    if (backendBuffer != nullptr)
    {
        backendBuffer->Unbind();
    }
}

void Buffer::UploadData(const void *data, size_t size, size_t offset) const
{
    if (backendBuffer == nullptr || data == nullptr || size == 0)
    {
        return;
    }
    backendBuffer->UploadData(data, size, offset);
}

void Buffer::Resize(size_t newSize)
{
    if (backendBuffer == nullptr)
    {
        return;
    }
    backendBuffer->Resize(newSize, false);
    desc.sizeInBytes = newSize;
}

void Buffer::ResizePreserveData(size_t newSize)
{
    if (backendBuffer == nullptr)
    {
        return;
    }
    backendBuffer->Resize(newSize, true);
    desc.sizeInBytes = newSize;
}

void *Buffer::MapBuffer(BufferMapAccess access)
{
    if (backendBuffer == nullptr)
    {
        return nullptr;
    }
    return backendBuffer->Map(access);
}

void Buffer::UnmapBuffer() const
{
    if (backendBuffer == nullptr)
    {
        return;
    }
    backendBuffer->Unmap();
}
