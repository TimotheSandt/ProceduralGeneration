#include "ShaderProgram.h"

#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"
#include "Graphics/Core/GraphicsRuntime.h"

#include <cstring>
#include <istream>
#include <ostream>
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

ShaderProgram::ShaderProgram(const char *vertexFile, const char *fragmentFile)
    : vertexShaderPath(vertexFile), fragmentShaderPath(fragmentFile)
{
    this->SetShader(vertexFile, fragmentFile);
}

ShaderProgram::~ShaderProgram() { this->Destroy(); }

ShaderProgram::ShaderProgram(const ShaderProgram &other)
    : ID(other.ID),
      backendResource(other.backendResource),  // shared — no recompile
      vertexShaderPath(other.vertexShaderPath),
      fragmentShaderPath(other.fragmentShaderPath),
      vertexSource(other.vertexSource),
      fragmentSource(other.fragmentSource)
{
}

ShaderProgram &ShaderProgram::operator=(const ShaderProgram &other)
{
    if (this != &other)
    {
        this->ID = other.ID;
        this->backendResource = other.backendResource;  // shared — no recompile
        this->vertexShaderPath = other.vertexShaderPath;
        this->fragmentShaderPath = other.fragmentShaderPath;
        this->vertexSource = other.vertexSource;
        this->fragmentSource = other.fragmentSource;
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

        std::shared_ptr<IShaderProgramResource> resource = device->CreateShaderProgram(createInfo);
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

bool ShaderProgram::SaveBinary(std::ostream &out) const
{
    if (backendResource == nullptr)
    {
        return false;
    }

    std::vector<std::byte> data;
    std::uint32_t format = 0;
    if (!backendResource->GetBinary(data, format))
    {
        return false;
    }

    const std::uint32_t binaryLen = static_cast<std::uint32_t>(data.size());
    out.write(reinterpret_cast<const char *>(&format), sizeof(format));
    out.write(reinterpret_cast<const char *>(&binaryLen), sizeof(binaryLen));
    out.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    return out.good();
}

bool ShaderProgram::LoadBinary(std::istream &in)
{
    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device == nullptr || device->GetAPI() != GraphicsAPI::OpenGL)
    {
        return false;
    }

    std::uint32_t format = 0;
    std::uint32_t binaryLen = 0;
    in.read(reinterpret_cast<char *>(&format), sizeof(format));
    in.read(reinterpret_cast<char *>(&binaryLen), sizeof(binaryLen));
    if (!in || binaryLen == 0)
    {
        return false;
    }

    std::vector<std::byte> data(binaryLen);
    in.read(reinterpret_cast<char *>(data.data()), binaryLen);
    if (!in)
    {
        return false;
    }

    // Create a blank resource and load the binary into it.
    ShaderProgramCreateInfo createInfo;
    createInfo.desc.stages = ShaderStageBit(ShaderStage::Vertex) | ShaderStageBit(ShaderStage::Fragment);
    createInfo.debugName = this->vertexShaderPath != nullptr ? this->vertexShaderPath : "cached_shader";

    const IGraphicsDevice *device = TryGetActiveGraphicsDevice();
    std::shared_ptr<IShaderProgramResource> resource = device->CreateShaderProgram(createInfo);
    if (resource == nullptr)
    {
        return false;
    }

    if (!resource->LoadBinary(data, format))
    {
        return false;
    }

    if (auto *openGLResource = dynamic_cast<OpenGLShaderProgramResource *>(resource.get()))
    {
        this->ID = openGLResource->GetProgramID();
    }

    if (this->ID == 0)
    {
        return false;
    }

    this->backendResource = std::move(resource);
    return true;
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
