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

void *AttributeOffset(std::size_t bytes)
{
    return std::bit_cast<void *>(static_cast<std::uintptr_t>(bytes));
}

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

std::string_view OpenGLShaderProgramResource::GetDebugName() const noexcept { return debugName; }

const ShaderProgramDesc &OpenGLShaderProgramResource::GetDescription() const noexcept { return desc; }

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

std::string_view OpenGLBufferResource::GetDebugName() const noexcept { return debugName; }

const BufferDesc &OpenGLBufferResource::GetDescription() const noexcept { return desc; }

GLuint OpenGLBufferResource::GetBufferID() const noexcept { return bufferID; }

OpenGLGeometryResource::OpenGLGeometryResource(GeometryCreateInfo createInfo)
    : layout(std::move(createInfo.layout)), indexCount(createInfo.indexData.size()), debugName(std::move(createInfo.debugName))
{
    if (!HasActiveOpenGLContext() || createInfo.vertexData.empty() || createInfo.indexData.empty())
    {
        return;
    }

    glGenVertexArrays(1, &vertexArrayID);
    glBindVertexArray(vertexArrayID);

    glGenBuffers(1, &vertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(createInfo.vertexData.size() * sizeof(float)), createInfo.vertexData.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &indexBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(createInfo.indexData.size() * sizeof(std::uint32_t)),
                 createInfo.indexData.data(), GL_STATIC_DRAW);

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
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(createInfo.instanceData.size() * sizeof(float)), createInfo.instanceData.data(),
                     GL_STATIC_DRAW);

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

std::string_view OpenGLGeometryResource::GetDebugName() const noexcept { return debugName; }

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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc.renderTarget ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, desc.renderTarget ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, desc.renderTarget ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, static_cast<GLsizei>(desc.extent.width), static_cast<GLsizei>(desc.extent.height), 0,
                 dataFormat, dataType, initialData);

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

std::string_view OpenGLTextureResource::GetDebugName() const noexcept { return debugName; }

const TextureDesc &OpenGLTextureResource::GetDescription() const noexcept { return desc; }

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

    if (desc.hasDepthBuffer)
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

std::string_view OpenGLRenderTargetResource::GetDebugName() const noexcept { return debugName; }

const RenderTargetDesc &OpenGLRenderTargetResource::GetDescription() const noexcept { return desc; }

GLuint OpenGLRenderTargetResource::GetFramebufferID() const noexcept { return framebufferID; }

GLuint OpenGLRenderTargetResource::GetDepthBufferID() const noexcept { return depthBufferID; }
