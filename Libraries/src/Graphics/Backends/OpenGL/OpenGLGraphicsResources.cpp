#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"

#include <GLFW/glfw3.h>

#include "Logger.h"

#include <bit>

namespace
{

bool HasActiveOpenGLContext() noexcept { return glfwGetCurrentContext() != nullptr; }

GLenum ToOpenGLShaderType(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex:
            return GL_VERTEX_SHADER;
        case ShaderStage::Fragment:
            return GL_FRAGMENT_SHADER;
        case ShaderStage::Geometry:
            return GL_GEOMETRY_SHADER;
        case ShaderStage::TessellationControl:
            return GL_TESS_CONTROL_SHADER;
        case ShaderStage::TessellationEvaluation:
            return GL_TESS_EVALUATION_SHADER;
        case ShaderStage::Compute:
            return GL_COMPUTE_SHADER;
        default:
            return 0;
    }
}

GLint ToOpenGLInternalFormat(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::BGRA8:
        case TextureFormat::RGBA8:
            return GL_RGBA8;
        case TextureFormat::Depth24Stencil8:
            return GL_DEPTH24_STENCIL8;
        case TextureFormat::Depth32Float:
            return GL_DEPTH_COMPONENT32F;
        case TextureFormat::R8:
            return GL_R8;
        case TextureFormat::R32UI:
            return GL_R32UI;
        case TextureFormat::RG16F:
            return GL_RG16F;
        default:
            return GL_RGBA8;
    }
}

GLenum ToOpenGLDataFormat(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::BGRA8:
            return GL_BGRA;
        case TextureFormat::RGBA8:
            return GL_RGBA;
        case TextureFormat::Depth24Stencil8:
            return GL_DEPTH_STENCIL;
        case TextureFormat::Depth32Float:
            return GL_DEPTH_COMPONENT;
        case TextureFormat::R8:
            return GL_RED;
        case TextureFormat::R32UI:
            return GL_RED_INTEGER;
        case TextureFormat::RG16F:
            return GL_RG;
        default:
            return GL_RGBA;
    }
}

GLenum ToOpenGLDataType(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::Depth24Stencil8:
            return GL_UNSIGNED_INT_24_8;
        case TextureFormat::Depth32Float:
            return GL_FLOAT;
        case TextureFormat::R32UI:
            return GL_UNSIGNED_INT;
        case TextureFormat::RG16F:
            return GL_HALF_FLOAT;
        case TextureFormat::BGRA8:
        case TextureFormat::RGBA8:
        case TextureFormat::R8:
        default:
            return GL_UNSIGNED_BYTE;
    }
}

GLenum ToOpenGLBufferTarget(BufferUsage usage)
{
    switch (usage)
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

GLenum ToOpenGLBufferUsageHint(const BufferDesc &desc)
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

void *AttributeOffset(std::size_t bytes) { return std::bit_cast<void *>(static_cast<std::uintptr_t>(bytes)); }

bool CheckShaderCompile(GLuint shaderID, ShaderStage stage)
{
    GLint compileStatus = GL_FALSE;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus == GL_TRUE)
    {
        return true;
    }

    char infoLog[1024] = {};
    glGetShaderInfoLog(shaderID, static_cast<GLsizei>(sizeof(infoLog)), nullptr, infoLog);
    LOG_ERROR(1, "OpenGL shader compilation failed for stage ", static_cast<int>(stage), ": ", infoLog);
    return false;
}

bool CheckProgramLink(GLuint programID)
{
    GLint linkStatus = GL_FALSE;
    glGetProgramiv(programID, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_TRUE)
    {
        return true;
    }

    char infoLog[1024] = {};
    glGetProgramInfoLog(programID, static_cast<GLsizei>(sizeof(infoLog)), nullptr, infoLog);
    LOG_ERROR(1, "OpenGL shader program linking failed: ", infoLog);
    return false;
}

} // namespace

OpenGLShaderProgramResource::OpenGLShaderProgramResource(ShaderProgramCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
{
    if (!HasActiveOpenGLContext() || createInfo.stageSources.empty())
    {
        return;
    }

    programID = glCreateProgram();
    std::vector<GLuint> compiledShaders;
    compiledShaders.reserve(createInfo.stageSources.size());

    for (const ShaderStageSource &stageSource : createInfo.stageSources)
    {
        const GLenum shaderType = ToOpenGLShaderType(stageSource.stage);
        if (shaderType == 0)
        {
            LOG_ERROR(1, "OpenGL backend does not support shader stage ", static_cast<int>(stageSource.stage), " for resource ", debugName);
            continue;
        }

        const GLuint shaderID = glCreateShader(shaderType);
        const char *shaderSource = stageSource.sourceCode.c_str();
        glShaderSource(shaderID, 1, &shaderSource, nullptr);
        glCompileShader(shaderID);
        if (!CheckShaderCompile(shaderID, stageSource.stage))
        {
            glDeleteShader(shaderID);
            continue;
        }

        glAttachShader(programID, shaderID);
        compiledShaders.push_back(shaderID);
    }

    if (!compiledShaders.empty())
    {
        glLinkProgram(programID);
    }

    if (compiledShaders.empty() || !CheckProgramLink(programID))
    {
        glDeleteProgram(programID);
        programID = 0;
    }

    for (const GLuint shaderID : compiledShaders)
    {
        if (programID != 0)
        {
            glDetachShader(programID, shaderID);
        }
        glDeleteShader(shaderID);
    }
}

OpenGLShaderProgramResource::~OpenGLShaderProgramResource()
{
    if (programID != 0 && HasActiveOpenGLContext())
    {
        glDeleteProgram(programID);
    }
}

GraphicsAPI OpenGLShaderProgramResource::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

std::string OpenGLShaderProgramResource::GetDebugName() const noexcept { return debugName; }

const ShaderProgramDesc &OpenGLShaderProgramResource::GetDescription() const noexcept { return desc; }

void OpenGLShaderProgramResource::Bind() const
{
    if (programID != 0)
    {
        glUseProgram(programID);
    }
}

void OpenGLShaderProgramResource::Unbind() const { glUseProgram(0); }

int OpenGLShaderProgramResource::GetUniformLocation(std::string name) const
{
    if (programID == 0)
    {
        return -1;
    }
    return glGetUniformLocation(programID, std::string(name).c_str());
}

void OpenGLShaderProgramResource::SetFloatUniform(int location, const float *data, std::size_t componentCount) const
{
    if (location < 0 || data == nullptr)
    {
        return;
    }

    switch (componentCount)
    {
        case 1:
            glUniform1fv(location, 1, data);
            break;
        case 2:
            glUniform2fv(location, 1, data);
            break;
        case 3:
            glUniform3fv(location, 1, data);
            break;
        case 4:
            glUniform4fv(location, 1, data);
            break;
        default:
            break;
    }
}

void OpenGLShaderProgramResource::SetIntUniform(int location, const int *data, std::size_t componentCount) const
{
    if (location < 0 || data == nullptr)
    {
        return;
    }

    switch (componentCount)
    {
        case 1:
            glUniform1iv(location, 1, data);
            break;
        case 2:
            glUniform2iv(location, 1, data);
            break;
        case 3:
            glUniform3iv(location, 1, data);
            break;
        case 4:
            glUniform4iv(location, 1, data);
            break;
        default:
            break;
    }
}

void OpenGLShaderProgramResource::SetMatrix4Uniform(int location, const float *data) const
{
    if (location >= 0 && data != nullptr)
    {
        glUniformMatrix4fv(location, 1, GL_FALSE, data);
    }
}

bool OpenGLShaderProgramResource::GetBinary(std::vector<std::byte> &dataOut, std::uint32_t &formatOut) const
{
    if (programID == 0)
    {
        return false;
    }
    GLint binaryLength = 0;
    glGetProgramiv(programID, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
    if (binaryLength <= 0)
    {
        return false;
    }
    dataOut.resize(static_cast<std::size_t>(binaryLength));
    GLenum binaryFormat = 0;
    GLsizei written = 0;
    glGetProgramBinary(programID, binaryLength, &written, &binaryFormat, dataOut.data());
    if (written <= 0)
    {
        dataOut.clear();
        return false;
    }
    dataOut.resize(static_cast<std::size_t>(written));
    formatOut = static_cast<std::uint32_t>(binaryFormat);
    return true;
}

bool OpenGLShaderProgramResource::LoadBinary(const std::vector<std::byte> &data, std::uint32_t format)
{
    if (programID == 0 || data.empty())
    {
        return false;
    }
    glProgramBinary(programID, static_cast<GLenum>(format), data.data(), static_cast<GLsizei>(data.size()));
    GLint status = 0;
    glGetProgramiv(programID, GL_LINK_STATUS, &status);
    return status == GL_TRUE;
}

GLuint OpenGLShaderProgramResource::GetProgramID() const noexcept { return programID; }

OpenGLBufferResource::OpenGLBufferResource(BufferCreateInfo createInfo)
    : target(ToOpenGLBufferTarget(createInfo.desc.usage)), desc(createInfo.desc), debugName(std::move(createInfo.debugName))
{
    if (!HasActiveOpenGLContext())
    {
        return;
    }

    glGenBuffers(1, &bufferID);
    glBindBuffer(target, bufferID);
    glBufferData(target, static_cast<GLsizeiptr>(desc.sizeInBytes),
                 createInfo.initialData.empty() ? nullptr : createInfo.initialData.data(), ToOpenGLBufferUsageHint(desc));
    glBindBuffer(target, 0);
}

OpenGLBufferResource::~OpenGLBufferResource()
{
    if (bufferID != 0 && HasActiveOpenGLContext())
    {
        glDeleteBuffers(1, &bufferID);
    }
}

GraphicsAPI OpenGLBufferResource::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

std::string OpenGLBufferResource::GetDebugName() const noexcept { return debugName; }

const BufferDesc &OpenGLBufferResource::GetDescription() const noexcept { return desc; }

void OpenGLBufferResource::Bind() const
{
    if (bufferID != 0)
    {
        glBindBuffer(target, bufferID);
    }
}

void OpenGLBufferResource::BindToBindingPoint(std::uint32_t bindingPoint) const
{
    if (bufferID == 0)
    {
        return;
    }

    switch (desc.usage)
    {
        case BufferUsage::Uniform:
            glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, bufferID);
            break;
        case BufferUsage::Storage:
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, bufferID);
            break;
        default:
            Bind();
            break;
    }
}

void OpenGLBufferResource::Unbind() const { glBindBuffer(target, 0); }

void OpenGLBufferResource::UploadData(const void *data, std::size_t size, std::size_t offset)
{
    if (bufferID == 0 || data == nullptr || size == 0)
    {
        return;
    }

    glBindBuffer(target, bufferID);
    glBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
    glBindBuffer(target, 0);
}

void OpenGLBufferResource::Resize(std::size_t newSize, bool preserveData) { Recreate(newSize, preserveData); }

void *OpenGLBufferResource::Map(BufferMapAccess access)
{
    if (bufferID == 0)
    {
        return nullptr;
    }

    GLenum openGLAccess = GL_READ_WRITE;
    switch (access)
    {
        case BufferMapAccess::ReadOnly:
            openGLAccess = GL_READ_ONLY;
            break;
        case BufferMapAccess::WriteOnly:
            openGLAccess = GL_WRITE_ONLY;
            break;
        case BufferMapAccess::ReadWrite:
        default:
            openGLAccess = GL_READ_WRITE;
            break;
    }

    glBindBuffer(target, bufferID);
    return glMapBuffer(target, openGLAccess);
}

void OpenGLBufferResource::Unmap()
{
    if (bufferID == 0)
    {
        return;
    }

    glBindBuffer(target, bufferID);
    glUnmapBuffer(target);
    glBindBuffer(target, 0);
}

GLenum OpenGLBufferResource::UsageHint() const noexcept { return ToOpenGLBufferUsageHint(desc); }

void OpenGLBufferResource::Recreate(std::size_t newSize, bool preserveData)
{
    if (bufferID == 0 || newSize == desc.sizeInBytes)
    {
        desc.sizeInBytes = newSize;
        return;
    }

    std::vector<std::byte> previousData;
    if (preserveData && desc.sizeInBytes > 0)
    {
        previousData.resize(std::min(desc.sizeInBytes, newSize));
        glBindBuffer(target, bufferID);
        glGetBufferSubData(target, 0, static_cast<GLsizeiptr>(previousData.size()), previousData.data());
        glBindBuffer(target, 0);
    }

    glDeleteBuffers(1, &bufferID);
    bufferID = 0;
    desc.sizeInBytes = newSize;

    glGenBuffers(1, &bufferID);
    glBindBuffer(target, bufferID);
    glBufferData(target, static_cast<GLsizeiptr>(desc.sizeInBytes), nullptr, UsageHint());
    glBindBuffer(target, 0);

    if (!previousData.empty())
    {
        UploadData(previousData.data(), previousData.size(), 0);
    }
}

GLuint OpenGLBufferResource::GetBufferID() const noexcept { return bufferID; }

OpenGLGeometryResource::OpenGLGeometryResource(GeometryCreateInfo createInfo)
    : layout(std::move(createInfo.layout)), indexCount(createInfo.indexData.size()), vertexBufferFloatCount(createInfo.vertexData.size()),
      instanceBufferFloatCount(createInfo.instanceData.size()), dynamicVertexData(createInfo.dynamicVertexData),
      dynamicInstanceData(createInfo.dynamicInstanceData), debugName(std::move(createInfo.debugName))
{
    if (!HasActiveOpenGLContext() || createInfo.vertexData.empty())
    {
        return;
    }

    glGenVertexArrays(1, &vertexArrayID);
    glBindVertexArray(vertexArrayID);

    glGenBuffers(1, &vertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(createInfo.vertexData.size() * sizeof(float)), createInfo.vertexData.data(),
                 dynamicVertexData ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    if (!createInfo.indexData.empty())
    {
        glGenBuffers(1, &indexBufferID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(createInfo.indexData.size() * sizeof(std::uint32_t)),
                     createInfo.indexData.data(), GL_STATIC_DRAW);
    }

    std::uint32_t vertexStride = 0;
    for (const std::uint32_t size : layout.vertexAttributes)
    {
        vertexStride += size;
    }

    std::uint32_t offset = 0;
    for (GLuint attributeIndex = 0; attributeIndex < layout.vertexAttributes.size(); ++attributeIndex)
    {
        glVertexAttribPointer(attributeIndex, static_cast<GLint>(layout.vertexAttributes[attributeIndex]), GL_FLOAT, GL_FALSE,
                              static_cast<GLsizei>(vertexStride * sizeof(float)), AttributeOffset(offset * sizeof(float)));
        glEnableVertexAttribArray(attributeIndex);
        offset += layout.vertexAttributes[attributeIndex];
    }

    if (!createInfo.instanceData.empty() && !layout.instanceAttributes.empty())
    {
        glGenBuffers(1, &instanceBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, instanceBufferID);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(createInfo.instanceData.size() * sizeof(float)),
                     createInfo.instanceData.data(), dynamicInstanceData ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

        std::uint32_t instanceStride = 0;
        for (const std::uint32_t size : layout.instanceAttributes)
        {
            instanceStride += size;
        }

        offset = 0;
        const GLuint firstInstanceAttribute = static_cast<GLuint>(layout.vertexAttributes.size());
        for (GLuint attributeIndex = 0; attributeIndex < layout.instanceAttributes.size(); ++attributeIndex)
        {
            const GLuint layoutIndex = firstInstanceAttribute + attributeIndex;
            glVertexAttribPointer(layoutIndex, static_cast<GLint>(layout.instanceAttributes[attributeIndex]), GL_FLOAT, GL_FALSE,
                                  static_cast<GLsizei>(instanceStride * sizeof(float)), AttributeOffset(offset * sizeof(float)));
            glEnableVertexAttribArray(layoutIndex);
            glVertexAttribDivisor(layoutIndex, 1);
            offset += layout.instanceAttributes[attributeIndex];
        }

        instanceCount = instanceStride == 0 ? 0 : createInfo.instanceData.size() / instanceStride;
    }
    else
    {
        instanceCount = 1;
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

OpenGLGeometryResource::~OpenGLGeometryResource()
{
    if (!HasActiveOpenGLContext())
    {
        return;
    }

    if (instanceBufferID != 0)
    {
        glDeleteBuffers(1, &instanceBufferID);
    }
    if (indexBufferID != 0)
    {
        glDeleteBuffers(1, &indexBufferID);
    }
    if (vertexBufferID != 0)
    {
        glDeleteBuffers(1, &vertexBufferID);
    }
    if (vertexArrayID != 0)
    {
        glDeleteVertexArrays(1, &vertexArrayID);
    }
}

GraphicsAPI OpenGLGeometryResource::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

std::string OpenGLGeometryResource::GetDebugName() const noexcept { return debugName; }

const GeometryLayout &OpenGLGeometryResource::GetLayout() const noexcept { return layout; }

std::size_t OpenGLGeometryResource::GetIndexCount() const noexcept { return indexCount; }

std::size_t OpenGLGeometryResource::GetInstanceCount() const noexcept { return instanceCount; }

void OpenGLGeometryResource::Bind() const
{
    if (vertexArrayID != 0)
    {
        glBindVertexArray(vertexArrayID);
    }
}

void OpenGLGeometryResource::Unbind() const { glBindVertexArray(0); }

void OpenGLGeometryResource::UpdateVertexData(const float *data, std::size_t floatCount, std::size_t offsetFloats)
{
    if (vertexBufferID == 0 || data == nullptr || floatCount == 0 || offsetFloats + floatCount > vertexBufferFloatCount)
    {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offsetFloats * sizeof(float)),
                    static_cast<GLsizeiptr>(floatCount * sizeof(float)), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLGeometryResource::UpdateInstanceData(const float *data, std::size_t floatCount, std::size_t offsetFloats)
{
    if (instanceBufferID == 0 || data == nullptr || floatCount == 0 || offsetFloats + floatCount > instanceBufferFloatCount)
    {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceBufferID);
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offsetFloats * sizeof(float)),
                    static_cast<GLsizeiptr>(floatCount * sizeof(float)), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLGeometryResource::DrawIndexed() const
{
    if (vertexArrayID != 0 && indexCount != 0)
    {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr);
    }
}

void OpenGLGeometryResource::DrawIndexedInstanced() const
{
    if (vertexArrayID != 0 && indexCount != 0)
    {
        glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr,
                                static_cast<GLsizei>(instanceCount));
    }
}

void OpenGLGeometryResource::DrawVertices(std::size_t vertexCount) const
{
    if (vertexArrayID != 0 && vertexCount != 0)
    {
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount));
    }
}

OpenGLTextureResource::OpenGLTextureResource(TextureCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
{
    if (!HasActiveOpenGLContext() || desc.extent.width == 0 || desc.extent.height == 0)
    {
        return;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    const GLint internalFormat = ToOpenGLInternalFormat(desc.format);
    const GLenum dataFormat = ToOpenGLDataFormat(desc.format);
    const GLenum dataType = ToOpenGLDataType(desc.format);
    const void *initialData = createInfo.initialData.empty() ? nullptr : createInfo.initialData.data();
    const bool isSingleChannelTexture = (desc.format == TextureFormat::R8 || desc.format == TextureFormat::R32UI || desc.format == TextureFormat::RG16F);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    (desc.renderTarget || !createInfo.generateMipmaps) ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (desc.renderTarget || isSingleChannelTexture) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (desc.renderTarget || isSingleChannelTexture) ? GL_CLAMP_TO_EDGE : GL_REPEAT);

    GLint previousUnpackAlignment = 4;
    if (isSingleChannelTexture)
    {
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, static_cast<GLsizei>(desc.extent.width), static_cast<GLsizei>(desc.extent.height), 0,
                 dataFormat, dataType, initialData);

    if (isSingleChannelTexture)
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    }

    if (createInfo.generateMipmaps && !desc.renderTarget && initialData != nullptr)
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

OpenGLTextureResource::~OpenGLTextureResource()
{
    if (textureID != 0 && HasActiveOpenGLContext())
    {
        glDeleteTextures(1, &textureID);
    }
}

GraphicsAPI OpenGLTextureResource::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

std::string OpenGLTextureResource::GetDebugName() const noexcept { return debugName; }

const TextureDesc &OpenGLTextureResource::GetDescription() const noexcept { return desc; }

void OpenGLTextureResource::Bind(std::uint32_t slot) const
{
    if (textureID == 0)
    {
        return;
    }

    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void OpenGLTextureResource::Unbind() const { glBindTexture(GL_TEXTURE_2D, 0); }

void OpenGLTextureResource::Readback(std::vector<std::byte> &output) const
{
    if (textureID == 0)
    {
        output.clear();
        return;
    }

    const GLenum format = ToOpenGLDataFormat(desc.format);
    const GLenum dataType = ToOpenGLDataType(desc.format);
    std::size_t bytesPerPixel = 4;
    switch (desc.format)
    {
        case TextureFormat::R8:
            bytesPerPixel = 1;
            break;
        case TextureFormat::Depth32Float:
        case TextureFormat::R32UI:
            bytesPerPixel = sizeof(float);
            break;
        case TextureFormat::RG16F:
            bytesPerPixel = 2 * sizeof(std::uint16_t);  // 2 × float16
            break;
        case TextureFormat::Depth24Stencil8:
        case TextureFormat::BGRA8:
        case TextureFormat::RGBA8:
        default:
            bytesPerPixel = 4;
            break;
    }

    output.resize(static_cast<std::size_t>(desc.extent.width) * static_cast<std::size_t>(desc.extent.height) * bytesPerPixel);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glGetTexImage(GL_TEXTURE_2D, 0, format, dataType, output.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLTextureResource::Resize(std::uint32_t width, std::uint32_t height)
{
    if (textureID == 0)
    {
        return;
    }

    desc.extent = {width, height};
    const GLint internalFormat = ToOpenGLInternalFormat(desc.format);
    const GLenum dataFormat = ToOpenGLDataFormat(desc.format);
    const GLenum dataType = ToOpenGLDataType(desc.format);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, dataFormat, dataType,
                 nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLTextureResource::AttachToFramebuffer(std::uint32_t framebufferHandle, std::uint32_t colorIndex) const
{
    if (textureID == 0)
    {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebufferHandle));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorIndex, GL_TEXTURE_2D, textureID, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLTextureResource::AttachAsDepthToFramebuffer(std::uint32_t framebufferHandle) const
{
    if (textureID == 0)
    {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebufferHandle));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textureID, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint OpenGLTextureResource::GetTextureID() const noexcept { return textureID; }

OpenGLRenderTargetResource::OpenGLRenderTargetResource(RenderTargetCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
{
    if (!HasActiveOpenGLContext() || desc.extent.width == 0 || desc.extent.height == 0)
    {
        return;
    }

    glGenFramebuffers(1, &framebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);

    // Allocate depth as a renderbuffer only when hasDepthBuffer is true and depth is NOT
    // a texture (depthAsTexture == false). When depthAsTexture is true, RenderTarget
    // creates a Texture with a depth format and attaches it via AttachAsDepthToFramebuffer.
    if (desc.hasDepthBuffer && !desc.depthAsTexture)
    {
        glGenRenderbuffers(1, &depthBufferID);
        glBindRenderbuffer(GL_RENDERBUFFER, depthBufferID);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(desc.extent.width),
                              static_cast<GLsizei>(desc.extent.height));
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBufferID);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

OpenGLRenderTargetResource::~OpenGLRenderTargetResource()
{
    if (!HasActiveOpenGLContext())
    {
        return;
    }

    if (depthBufferID != 0)
    {
        glDeleteRenderbuffers(1, &depthBufferID);
    }
    if (framebufferID != 0)
    {
        glDeleteFramebuffers(1, &framebufferID);
    }
}

GraphicsAPI OpenGLRenderTargetResource::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

std::string OpenGLRenderTargetResource::GetDebugName() const noexcept { return debugName; }

const RenderTargetDesc &OpenGLRenderTargetResource::GetDescription() const noexcept { return desc; }

void OpenGLRenderTargetResource::Bind() const { glBindFramebuffer(GL_FRAMEBUFFER, framebufferID); }

void OpenGLRenderTargetResource::Unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void OpenGLRenderTargetResource::Resize(std::uint32_t width, std::uint32_t height)
{
    desc.extent = {width, height};

    if (depthBufferID != 0)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, depthBufferID);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
}

bool OpenGLRenderTargetResource::IsComplete() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return status == GL_FRAMEBUFFER_COMPLETE;
}

void OpenGLRenderTargetResource::BlitFromDefault(std::uint32_t srcWidth, std::uint32_t srcHeight, std::uint32_t dstWidth,
                                                 std::uint32_t dstHeight) const
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferID);
    glBlitFramebuffer(0, 0, static_cast<GLint>(srcWidth), static_cast<GLint>(srcHeight), 0, 0, static_cast<GLint>(dstWidth),
                      static_cast<GLint>(dstHeight), GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderTargetResource::BlitTo(const IRenderTargetResource &destination, std::uint32_t srcWidth, std::uint32_t srcHeight,
                                        std::uint32_t dstWidth, std::uint32_t dstHeight) const
{
    const auto *openGLDestination = dynamic_cast<const OpenGLRenderTargetResource *>(&destination);
    if (openGLDestination == nullptr)
    {
        return;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferID);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, openGLDestination->framebufferID);
    glBlitFramebuffer(0, 0, static_cast<GLint>(srcWidth), static_cast<GLint>(srcHeight), 0, 0, static_cast<GLint>(dstWidth),
                      static_cast<GLint>(dstHeight), GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBlitFramebuffer(0, 0, static_cast<GLint>(srcWidth), static_cast<GLint>(srcHeight), 0, 0, static_cast<GLint>(dstWidth),
                      static_cast<GLint>(dstHeight), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderTargetResource::BlitToDefault(std::uint32_t srcWidth, std::uint32_t srcHeight, std::uint32_t dstWidth,
                                               std::uint32_t dstHeight) const
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferID);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, static_cast<GLint>(srcWidth), static_cast<GLint>(srcHeight), 0, 0, static_cast<GLint>(dstWidth),
                      static_cast<GLint>(dstHeight), GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBlitFramebuffer(0, 0, static_cast<GLint>(srcWidth), static_cast<GLint>(srcHeight), 0, 0, static_cast<GLint>(dstWidth),
                      static_cast<GLint>(dstHeight), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

std::uint32_t OpenGLRenderTargetResource::ReadPixelUInt(std::uint32_t attachmentIndex, int x, int y, int framebufferHeight) const
{
    if (framebufferID == 0)
    {
        return 0;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferID);
    glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);

    const int flippedY = framebufferHeight - 1 - y;
    std::uint32_t value = 0;
    glReadPixels(x, flippedY, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &value);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return value;
}

glm::uvec4 OpenGLRenderTargetResource::ReadPixelRGBA8(std::uint32_t attachmentIndex, int x, int y, int framebufferHeight) const
{
    if (framebufferID == 0)
    {
        return glm::uvec4(0);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferID);
    glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);

    const int flippedY = framebufferHeight - 1 - y;
    // Read into a tightly-packed 4-byte array — glm::uvec4 is 16 bytes, not 4.
    std::uint8_t pixel[4] = {0, 0, 0, 0};
    glReadPixels(x, flippedY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return glm::uvec4(pixel[0], pixel[1], pixel[2], pixel[3]);
}

void OpenGLRenderTargetResource::SetDrawBuffers(std::uint32_t count)
{
    if (framebufferID == 0 || count == 0)
    {
        return;
    }

    constexpr std::uint32_t maxAttachments = 8;
    count = count < maxAttachments ? count : maxAttachments;

    GLenum buffers[maxAttachments];
    for (std::uint32_t i = 0; i < count; ++i)
    {
        buffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    glDrawBuffers(static_cast<GLsizei>(count), buffers);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint OpenGLRenderTargetResource::GetFramebufferID() const noexcept { return framebufferID; }

GLuint OpenGLRenderTargetResource::GetDepthBufferID() const noexcept { return depthBufferID; }

OpenGLTimestampQueryResource::OpenGLTimestampQueryResource(GPUTimestampQueryCreateInfo createInfo)
    : debugName(std::move(createInfo.debugName))
{
    if (HasActiveOpenGLContext())
    {
        glGenQueries(2, queryIDs.data());
    }
}

OpenGLTimestampQueryResource::~OpenGLTimestampQueryResource()
{
    if (HasActiveOpenGLContext() && queryIDs[0] != 0)
    {
        glDeleteQueries(2, queryIDs.data());
    }
}

GraphicsAPI OpenGLTimestampQueryResource::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

std::string OpenGLTimestampQueryResource::GetDebugName() const noexcept { return debugName; }

void OpenGLTimestampQueryResource::Begin()
{
    if (queryIDs[0] != 0)
    {
        glQueryCounter(queryIDs[0], GL_TIMESTAMP);
    }
}

void OpenGLTimestampQueryResource::End()
{
    if (queryIDs[1] != 0)
    {
        glQueryCounter(queryIDs[1], GL_TIMESTAMP);
    }
}

bool OpenGLTimestampQueryResource::IsReady() const
{
    if (queryIDs[1] == 0)
    {
        return false;
    }

    GLint available = GL_FALSE;
    glGetQueryObjectiv(queryIDs[1], GL_QUERY_RESULT_AVAILABLE, &available);
    return available == GL_TRUE;
}

std::chrono::nanoseconds OpenGLTimestampQueryResource::GetElapsedTime() const
{
    if (!IsReady())
    {
        return std::chrono::nanoseconds::zero();
    }

    GLuint64 startTime = 0;
    GLuint64 endTime = 0;
    glGetQueryObjectui64v(queryIDs[0], GL_QUERY_RESULT, &startTime);
    glGetQueryObjectui64v(queryIDs[1], GL_QUERY_RESULT, &endTime);
    return std::chrono::nanoseconds(endTime - startTime);
}
