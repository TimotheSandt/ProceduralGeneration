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

std::unique_ptr<IBufferResource> OpenGLGraphicsDevice::CreateBuffer(const BufferCreateInfo &createInfo) const
{
    return std::make_unique<OpenGLBufferResource>(createInfo);
}

std::unique_ptr<IGeometryResource> OpenGLGraphicsDevice::CreateGeometry(const GeometryCreateInfo &createInfo) const
{
    return std::make_unique<OpenGLGeometryResource>(createInfo);
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

std::unique_ptr<IRenderTargetResource> OpenGLGraphicsDevice::CreateRenderTarget(const RenderTargetCreateInfo &createInfo) const
{
    return std::make_unique<OpenGLRenderTargetResource>(createInfo);
}

std::unique_ptr<IAccelerationStructureResource> OpenGLGraphicsDevice::CreateAccelerationStructure(
    const AccelerationStructureCreateInfo &createInfo) const
{
    static_cast<void>(createInfo);
    return nullptr;
}

std::unique_ptr<IGPUTimestampQueryResource> OpenGLGraphicsDevice::CreateTimestampQuery(const GPUTimestampQueryCreateInfo &createInfo) const
{
    return std::make_unique<OpenGLTimestampQueryResource>(createInfo);
}
