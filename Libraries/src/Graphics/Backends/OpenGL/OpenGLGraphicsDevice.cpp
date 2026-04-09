#include "Graphics/Backends/OpenGL/OpenGLGraphicsDevice.h"
#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"

namespace
{

constexpr ShaderStageMask OpenGLSupportedStages = ShaderStageBit(ShaderStage::Vertex) | ShaderStageBit(ShaderStage::Fragment) |
                                                  ShaderStageBit(ShaderStage::Geometry) | ShaderStageBit(ShaderStage::TessellationControl) |
                                                  ShaderStageBit(ShaderStage::TessellationEvaluation) |
                                                  ShaderStageBit(ShaderStage::Compute);

} // namespace

OpenGLGraphicsDevice::OpenGLGraphicsDevice(GraphicsCapabilities capabilitiesIn) : capabilities(capabilitiesIn) {}

GraphicsAPI OpenGLGraphicsDevice::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

std::string_view OpenGLGraphicsDevice::GetDeviceName() const noexcept { return "OpenGL Device"; }

const GraphicsCapabilities &OpenGLGraphicsDevice::GetCapabilities() const noexcept { return capabilities; }

bool OpenGLGraphicsDevice::SupportsShaderStages(ShaderStageMask stages) const noexcept
{
    return (stages & OpenGLSupportedStages) == stages;
}

std::unique_ptr<IShaderProgramResource> OpenGLGraphicsDevice::CreateShaderProgram(const ShaderProgramCreateInfo &createInfo) const
{
    if (!SupportsShaderStages(createInfo.desc.stages))
    {
        return nullptr;
    }

    return std::make_unique<OpenGLShaderProgramResource>(createInfo);
}

std::unique_ptr<ITextureResource> OpenGLGraphicsDevice::CreateTexture(const TextureCreateInfo &createInfo) const
{
    return std::make_unique<OpenGLTextureResource>(createInfo);
}

std::unique_ptr<IAccelerationStructureResource> OpenGLGraphicsDevice::CreateAccelerationStructure(
    const AccelerationStructureCreateInfo &createInfo) const
{
    static_cast<void>(createInfo);
    return nullptr;
}
