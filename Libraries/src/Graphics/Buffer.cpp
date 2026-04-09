#include "Buffer.h"

#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"
#include "Graphics/Core/GraphicsRuntime.h"

#include <algorithm>
#include <cstring>
#include <vector>

Buffer::Buffer(BufferUsage usage, size_t size, GLuint bindingPoint, bool cpuWritable)
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

GLenum Buffer::GetTarget() const noexcept
{
    switch (desc.usage)
    {
        case BufferUsage::Index:
            return GL_ELEMENT_ARRAY_BUFFER;
        case BufferUsage::Uniform:
            return GL_UNIFORM_BUFFER;
        case BufferUsage::Storage:
            return GL_SHADER_STORAGE_BUFFER;
        case BufferUsage::Staging:
        case BufferUsage::Vertex:
        default:
            return GL_ARRAY_BUFFER;
    }
}

GLenum Buffer::GetUsageHint() const noexcept
{
    if (desc.cpuWritable)
    {
        return GL_DYNAMIC_DRAW;
    }

    switch (desc.usage)
    {
        case BufferUsage::Staging:
            return GL_STREAM_DRAW;
        case BufferUsage::Vertex:
        case BufferUsage::Index:
        case BufferUsage::Uniform:
        case BufferUsage::Storage:
        default:
            return GL_STATIC_DRAW;
    }
}

bool Buffer::Initialize(BufferUsage usage, size_t size, GLuint bindingPoint, bool cpuWritable)
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

    glGenBuffers(1, &this->ID);
    glBindBuffer(GetTarget(), this->ID);
    glBufferData(GetTarget(), static_cast<GLsizeiptr>(this->desc.sizeInBytes), nullptr, GetUsageHint());
    glBindBuffer(GetTarget(), 0);
    return this->ID != 0;
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

    if (ID != 0)
    {
        glDeleteBuffers(1, &ID);
    }
    ID = 0;
    desc.sizeInBytes = 0;
}

void Buffer::Bind() const
{
    if (ID == 0)
    {
        return;
    }
    glBindBuffer(GetTarget(), ID);
}

void Buffer::BindToBindingPoint() const
{
    if (ID == 0)
    {
        return;
    }

    switch (desc.usage)
    {
        case BufferUsage::Uniform:
            glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ID);
            break;
        case BufferUsage::Storage:
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, ID);
            break;
        default:
            Bind();
            break;
    }
}

void Buffer::Unbind() const { glBindBuffer(GetTarget(), 0); }

void Buffer::UploadData(const void *data, size_t size, size_t offset) const
{
    if (ID == 0 || data == nullptr || size == 0)
    {
        return;
    }

    glBindBuffer(GetTarget(), ID);
    glBufferSubData(GetTarget(), static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
    glBindBuffer(GetTarget(), 0);
}

void Buffer::Recreate(size_t newSize, bool preserveData)
{
    if (ID == 0 || newSize == desc.sizeInBytes)
    {
        desc.sizeInBytes = newSize;
        return;
    }

    std::vector<std::byte> previousData;
    if (preserveData && desc.sizeInBytes > 0)
    {
        previousData.resize(std::min(desc.sizeInBytes, newSize));
        glBindBuffer(GetTarget(), ID);
        glGetBufferSubData(GetTarget(), 0, static_cast<GLsizeiptr>(previousData.size()), previousData.data());
        glBindBuffer(GetTarget(), 0);
    }

    const BufferUsage usage = desc.usage;
    const bool cpuWritable = desc.cpuWritable;
    const GLuint currentBindingPoint = bindingPoint;

    this->Destroy();
    this->Initialize(usage, newSize, currentBindingPoint, cpuWritable);

    if (!previousData.empty())
    {
        this->UploadData(previousData.data(), previousData.size());
    }
}

void Buffer::Resize(size_t newSize) { Recreate(newSize, false); }

void Buffer::ResizePreserveData(size_t newSize) { Recreate(newSize, true); }

void *Buffer::MapBuffer(GLenum access) const
{
    if (ID == 0)
    {
        return nullptr;
    }

    glBindBuffer(GetTarget(), ID);
    return glMapBuffer(GetTarget(), access);
}

void Buffer::UnmapBuffer() const
{
    if (ID == 0)
    {
        return;
    }

    glBindBuffer(GetTarget(), ID);
    glUnmapBuffer(GetTarget());
    glBindBuffer(GetTarget(), 0);
}
