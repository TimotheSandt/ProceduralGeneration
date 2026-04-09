#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"

#include <GLFW/glfw3.h>

#include "Logger.h"

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

OpenGLTextureResource::OpenGLTextureResource(TextureCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
{
}

GraphicsAPI OpenGLTextureResource::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

std::string_view OpenGLTextureResource::GetDebugName() const noexcept { return debugName; }

const TextureDesc &OpenGLTextureResource::GetDescription() const noexcept { return desc; }
