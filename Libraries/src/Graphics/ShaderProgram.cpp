#include "ShaderProgram.h"

#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"
#include "Graphics/Core/GraphicsRuntime.h"

#include <cstring>
#include <utility>

#define COMPILE_SUCCESS 0
#define COMPILE_ERRORS 1

// Reads a text file and outputs a string with everything in the text file
std::string get_file_contents(const char *filename)
{
    std::ifstream in(filename, std::ios::binary);
    if (in)
    {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], static_cast<std::streamsize>(contents.size()));
        in.close();
        return (contents);
    }
    throw(errno);
}

ShaderProgram::ShaderProgram(const char *vertexFile, const char *fragmentFile) : vertexShaderPath(vertexFile), fragmentShaderPath(fragmentFile)
{
    this->SetShader(vertexFile, fragmentFile);
}

ShaderProgram::~ShaderProgram() { this->Destroy(); }

ShaderProgram::ShaderProgram(const ShaderProgram &shaderProgram)
{
    this->SetShader(shaderProgram.vertexShaderPath, shaderProgram.fragmentShaderPath);
}

ShaderProgram &ShaderProgram::operator=(const ShaderProgram &shaderProgram)
{
    if (this != &shaderProgram)
    {
        this->SetShader(shaderProgram.vertexShaderPath, shaderProgram.fragmentShaderPath);
    }
    return *this;
}

ShaderProgram::ShaderProgram(ShaderProgram &&shaderProgram) noexcept : ID(0), vertexShaderPath(nullptr), fragmentShaderPath(nullptr)
{
    this->Swap(shaderProgram);
}

ShaderProgram &ShaderProgram::operator=(ShaderProgram &&shaderProgram) noexcept
{
    if (this != &shaderProgram)
    {
        this->Destroy();
        this->Swap(shaderProgram);
    }
    return *this;
}

void ShaderProgram::Swap(ShaderProgram &other) noexcept
{
    std::swap(this->ID, other.ID);
    std::swap(this->backendResource, other.backendResource);
    std::swap(this->vertexShaderPath, other.vertexShaderPath);
    std::swap(this->fragmentShaderPath, other.fragmentShaderPath);
    std::swap(this->vertexSource, other.vertexSource);
    std::swap(this->fragmentSource, other.fragmentSource);
}

void ShaderProgram::SetShader(const char *vertexPath, const char *fragmentPath)
{
    this->vertexShaderPath = vertexPath;
    this->fragmentShaderPath = fragmentPath;

    try
    {
        vertexSource = get_file_contents(this->vertexShaderPath);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(1, "Failed to load vertex shader from ", this->vertexShaderPath, ": ", e.what());
        vertexSource = "#version 330 core\nvoid main() { gl_Position = vec4(0.0); }";
    }
    catch (int e)
    {
        LOG_ERROR(1, "Failed to load vertex shader from ", this->vertexShaderPath, ": ", strerror(e));
        vertexSource = "#version 330 core\nvoid main() { gl_Position = vec4(0.0); }";
    }

    try
    {
        fragmentSource = get_file_contents(this->fragmentShaderPath);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(1, "Failed to load fragment shader from ", this->fragmentShaderPath, ": ", e.what());
        fragmentSource = "#version 330 core\nout vec4 FragColor; void main() { FragColor = vec4(1.0); }";
    }
    catch (int e)
    {
        LOG_ERROR(1, "Failed to load fragment shader from ", this->fragmentShaderPath, ": ", strerror(e));
        fragmentSource = "#version 330 core\nout vec4 FragColor; void main() { FragColor = vec4(1.0); }";
    }

    this->CompileShader();
}

void ShaderProgram::SetShaderCode(std::string vertexCode, std::string fragmentCode)
{
    this->vertexShaderPath = nullptr;
    this->fragmentShaderPath = nullptr;

    this->vertexSource = std::move(vertexCode);
    this->fragmentSource = std::move(fragmentCode);

    this->CompileShader();
}

void ShaderProgram::CompileShader()
{
    this->Destroy();

    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr && device->GetAPI() == GraphicsAPI::OpenGL)
    {
        ShaderProgramCreateInfo createInfo;
        createInfo.desc.stages = ShaderStageBit(ShaderStage::Vertex) | ShaderStageBit(ShaderStage::Fragment);
        createInfo.debugName = this->vertexShaderPath != nullptr ? this->vertexShaderPath : "runtime_opengl_shader";
        createInfo.stageSources.push_back({.stage = ShaderStage::Vertex, .sourceCode = this->vertexSource});
        createInfo.stageSources.push_back({.stage = ShaderStage::Fragment, .sourceCode = this->fragmentSource});

        std::unique_ptr<IShaderProgramResource> resource = device->CreateShaderProgram(createInfo);
        if (auto *openGLResource = dynamic_cast<OpenGLShaderProgramResource *>(resource.get()); openGLResource != nullptr)
        {
            this->ID = openGLResource->GetProgramID();
            this->backendResource = std::move(resource);
            if (this->ID != 0)
            {
                return;
            }
            this->backendResource.reset();
        }
    }
}

void ShaderProgram::Bind() const
{
    if (this->backendResource == nullptr)
    {
        return;
    }
    this->backendResource->Bind();
}

void ShaderProgram::Unbind() const
{
    if (this->backendResource != nullptr)
    {
        this->backendResource->Unbind();
    }
}

void ShaderProgram::Destroy()
{
    if (this->backendResource != nullptr)
    {
        this->backendResource.reset();
        this->ID = 0;
        return;
    }

    this->ID = 0;
}

int ShaderProgram::GetUniformLocation(const std::string &uniform) const
{
    if (this->backendResource == nullptr)
    {
        return -1;
    }
    return this->backendResource->GetUniformLocation(uniform);
}

void ShaderProgram::SetUniformFloats(int location, const float *data, std::size_t componentCount) const
{
    if (this->backendResource == nullptr)
    {
        return;
    }
    this->backendResource->SetFloatUniform(location, data, componentCount);
}

void ShaderProgram::SetUniformInts(int location, const int *data, std::size_t componentCount) const
{
    if (this->backendResource == nullptr)
    {
        return;
    }
    this->backendResource->SetIntUniform(location, data, componentCount);
}

void ShaderProgram::SetUniformMatrix4(int location, const float *data) const
{
    if (this->backendResource == nullptr)
    {
        return;
    }
    this->backendResource->SetMatrix4Uniform(location, data);
}
