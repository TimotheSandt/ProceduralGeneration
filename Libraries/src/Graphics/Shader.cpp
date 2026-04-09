#include "Shader.h"

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

Shader::Shader(const char *vertexFile, const char *fragmentFile) : vertexShaderPath(vertexFile), fragmentShaderPath(fragmentFile)
{
    this->SetShader(vertexFile, fragmentFile);
}

Shader::~Shader() { this->Destroy(); }

Shader::Shader(const Shader &shader) { this->SetShader(shader.vertexShaderPath, shader.fragmentShaderPath); }

Shader &Shader::operator=(const Shader &shader)
{
    if (this != &shader)
    {
        this->SetShader(shader.vertexShaderPath, shader.fragmentShaderPath);
    }
    return *this;
}

Shader::Shader(Shader &&shader) noexcept : ID(0), vertexShaderPath(nullptr), fragmentShaderPath(nullptr) { this->Swap(shader); }

Shader &Shader::operator=(Shader &&shader) noexcept
{
    if (this != &shader)
    {
        this->Destroy();
        this->Swap(shader);
    }
    return *this;
}

void Shader::Swap(Shader &other) noexcept
{
    std::swap(this->ID, other.ID);
    std::swap(this->backendResource, other.backendResource);
    std::swap(this->vertexShaderPath, other.vertexShaderPath);
    std::swap(this->fragmentShaderPath, other.fragmentShaderPath);
    std::swap(this->vertexSource, other.vertexSource);
    std::swap(this->fragmentSource, other.fragmentSource);
}

void Shader::SetShader(const char *vertexPath, const char *fragmentPath)
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

void Shader::SetShaderCode(std::string vertexCode, std::string fragmentCode)
{
    this->vertexShaderPath = nullptr;
    this->fragmentShaderPath = nullptr;

    this->vertexSource = std::move(vertexCode);
    this->fragmentSource = std::move(fragmentCode);

    this->CompileShader();
}

void Shader::CompileShader()
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

void Shader::Bind() const
{
    if (this->backendResource == nullptr)
    {
        return;
    }
    this->backendResource->Bind();
}

void Shader::Unbind() const
{
    if (this->backendResource != nullptr)
    {
        this->backendResource->Unbind();
    }
}

void Shader::Destroy()
{
    if (this->backendResource != nullptr)
    {
        this->backendResource.reset();
        this->ID = 0;
        return;
    }

    this->ID = 0;
}

GLint Shader::GetUniformLocation(const std::string &uniform) const
{
    if (this->backendResource == nullptr)
    {
        return -1;
    }
    return this->backendResource->GetUniformLocation(uniform);
}

void Shader::SetUniformFloats(GLint location, const GLfloat *data, std::size_t componentCount) const
{
    if (this->backendResource == nullptr)
    {
        return;
    }
    this->backendResource->SetFloatUniform(location, data, componentCount);
}

void Shader::SetUniformInts(GLint location, const GLint *data, std::size_t componentCount) const
{
    if (this->backendResource == nullptr)
    {
        return;
    }
    this->backendResource->SetIntUniform(location, data, componentCount);
}

void Shader::SetUniformMatrix4(GLint location, const GLfloat *data) const
{
    if (this->backendResource == nullptr)
    {
        return;
    }
    this->backendResource->SetMatrix4Uniform(location, data);
}
