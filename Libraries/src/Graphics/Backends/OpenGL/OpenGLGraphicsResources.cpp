#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"

OpenGLShaderProgramResource::OpenGLShaderProgramResource(ShaderProgramCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
{
}

GraphicsAPI OpenGLShaderProgramResource::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

std::string_view OpenGLShaderProgramResource::GetDebugName() const noexcept { return debugName; }

const ShaderProgramDesc &OpenGLShaderProgramResource::GetDescription() const noexcept { return desc; }

OpenGLTextureResource::OpenGLTextureResource(TextureCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
{
}

GraphicsAPI OpenGLTextureResource::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

std::string_view OpenGLTextureResource::GetDebugName() const noexcept { return debugName; }

const TextureDesc &OpenGLTextureResource::GetDescription() const noexcept { return desc; }
