#include "Graphics/Backends/OpenGL/OpenGLGraphicsDevice.h"

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
