#include "Mesh.h"

#include <utility>

#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"

Mesh::Mesh(std::vector<float> vertices, std::vector<std::uint32_t> indices, std::vector<std::uint32_t> sizeAttrib)
{
    this->Initialize(std::move(vertices), std::move(indices), std::move(sizeAttrib));
}

Mesh::Mesh(std::vector<float> vertices, std::vector<std::uint32_t> indices, std::vector<std::uint32_t> sizeAttrib,
           std::vector<float> instances, std::vector<std::uint32_t> sizeAttribInstance)
{
    this->Initialize(std::move(vertices), std::move(indices), std::move(sizeAttrib), std::move(instances), std::move(sizeAttribInstance));
}

Mesh::Mesh(const Mesh &mesh)
{
    this->Initialize(mesh.vertices, mesh.indices, mesh.sizeAttrib, mesh.instances, mesh.sizeAttribInstance);
    for (const Texture &texture : mesh.textures)
    {
        this->textures.push_back(texture.Copy());
    }
    this->shaderProgram = ShaderProgram(mesh.shaderProgram);
    this->position = mesh.position;
    this->scale = mesh.scale;
    this->rotation = mesh.rotation;
}

Mesh &Mesh::operator=(const Mesh &mesh)
{
    if (this == &mesh)
    {
        return *this;
    }
    this->Destroy();
    this->Initialize(mesh.vertices, mesh.indices, mesh.sizeAttrib, mesh.instances, mesh.sizeAttribInstance);
    for (const Texture &texture : mesh.textures)
    {
        this->textures.push_back(texture.Copy());
    }
    this->shaderProgram = ShaderProgram(mesh.shaderProgram);
    this->position = mesh.position;
    this->scale = mesh.scale;
    this->rotation = mesh.rotation;
    return *this;
}

Mesh::Mesh(Mesh &&mesh) noexcept
    : vertices(std::move(mesh.vertices)), indices(std::move(mesh.indices)), sizeAttrib(std::move(mesh.sizeAttrib)),
      textures(std::move(mesh.textures)), shaderProgram(std::move(mesh.shaderProgram)), position(std::move(mesh.position)),
      scale(std::move(mesh.scale)), rotation(std::move(mesh.rotation)), instancing(mesh.instancing), instances(std::move(mesh.instances)),
      sizeAttribInstance(std::move(mesh.sizeAttribInstance)), geometry(std::move(mesh.geometry)), modelBuffer(std::move(mesh.modelBuffer)),
      uniformCache(std::move(mesh.uniformCache))
{
    mesh.instancing = 1;
}

Mesh &Mesh::operator=(Mesh &&mesh) noexcept
{
    if (this != &mesh)
    {
        this->Destroy();
        this->Swap(mesh);
    }
    return *this;
}

void Mesh::Swap(Mesh &mesh) noexcept
{
    std::swap(this->vertices, mesh.vertices);
    std::swap(this->indices, mesh.indices);
    std::swap(this->sizeAttrib, mesh.sizeAttrib);
    std::swap(this->instances, mesh.instances);
    std::swap(this->sizeAttribInstance, mesh.sizeAttribInstance);
    std::swap(this->textures, mesh.textures);
    std::swap(this->instancing, mesh.instancing);
    std::swap(this->geometry, mesh.geometry);
    std::swap(this->modelBuffer, mesh.modelBuffer);
    std::swap(this->shaderProgram, mesh.shaderProgram);
    std::swap(this->position, mesh.position);
    std::swap(this->scale, mesh.scale);
    std::swap(this->rotation, mesh.rotation);
}

void Mesh::Initialize(std::vector<float> vertices, std::vector<std::uint32_t> indices, std::vector<std::uint32_t> sizeAttrib)
{
    this->Initialize(std::move(vertices), std::move(indices), std::move(sizeAttrib), {}, {});
}

void Mesh::Initialize(std::vector<float> vertices, std::vector<std::uint32_t> indices, std::vector<std::uint32_t> sizeAttrib,
                      std::vector<float> instances, std::vector<std::uint32_t> sizeAttribInstance)
{
    this->vertices = std::move(vertices);
    this->indices = std::move(indices);
    this->sizeAttrib = std::move(sizeAttrib);
    this->instances = std::move(instances);
    this->sizeAttribInstance = std::move(sizeAttribInstance);

    if (this->instances.empty())
    {
        this->instancing = 1;
    }
    else
    {
        // Calculate instances based on total components per instance
        int componentsPerInstance = 0;
        for (std::uint32_t size : this->sizeAttribInstance)
        {
            componentsPerInstance += static_cast<int>(size);
        }
        this->instancing = (componentsPerInstance > 0) ? this->instances.size() / componentsPerInstance : 1;
    }

    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr)
    {
        GeometryCreateInfo createInfo;
        createInfo.layout.vertexAttributes = this->sizeAttrib;
        createInfo.layout.instanceAttributes = this->sizeAttribInstance;
        createInfo.vertexData.assign(this->vertices.begin(), this->vertices.end());
        createInfo.indexData.assign(this->indices.begin(), this->indices.end());
        createInfo.instanceData.assign(this->instances.begin(), this->instances.end());
        createInfo.debugName = "mesh_geometry";
        this->geometry = device->CreateGeometry(createInfo);
    }

    this->modelBuffer.Initialize(BufferUsage::Uniform, sizeof(glm::mat4), MESH_MODEL_BINDING_POINT, true);
}

void Mesh::Destroy()
{
    this->geometry.reset();
    this->modelBuffer.Destroy();
    this->shaderProgram.Destroy();
    for (std::size_t i = 0; i < this->textures.size(); i++)
    {
        this->textures[i].Destroy();
    }
    this->textures.clear();
    this->FreeCache();
}

void Mesh::AddTexture(Texture texture) { this->textures.push_back(texture.Copy()); }

void Mesh::AddTexture(const char *image, const char *name, TextureFormat format, TexturePixelType pixelType)
{
    const std::uint32_t slot = static_cast<std::uint32_t>(this->textures.size());
    this->textures.push_back(Texture(image, name, slot, format, pixelType));
}

void Mesh::Render(Camera &camera)
{
    if (!this->shaderProgram.IsCompiled())
    {
        LOG_WARNING("ShaderProgram not compiled");
        return;
    }
    this->shaderProgram.Bind();
    if (this->geometry != nullptr)
    {
        this->geometry->Bind();
    }
    for (std::size_t i = 0; i < this->textures.size(); i++)
    {
        this->textures[i].texUnit(this->shaderProgram);

        this->textures[i].Bind();
    }
    this->modelBuffer.BindToBindingPoint();
    this->Draw();
    if (camera.IsWireframe())
    {
        int wireframe = 1;
        this->SetUniform1i("wireframe", &wireframe);
        this->Draw(true);
        wireframe = 0;
        this->SetUniform1i("wireframe", &wireframe);
    }

    if (this->geometry != nullptr)
    {
        this->geometry->Unbind();
    }
    this->shaderProgram.Unbind();
    this->modelBuffer.Unbind();
    for (std::size_t i = 0; i < this->textures.size(); i++)
    {
        this->textures[i].Unbind();
    }
}

void Mesh::Draw(bool wireframe) const
{
    if (this->geometry == nullptr)
    {
        return;
    }

    if (wireframe)
    {
        GraphicsRenderState::SetWireframe(true);
    }
    else
    {
        GraphicsRenderState::SetWireframe(false);
    }

    if (this->instancing > 1)
    {
        this->geometry->DrawIndexedInstanced();
    }
    else
    {
        this->geometry->DrawIndexed();
    }

    if (wireframe)
    {
        GraphicsRenderState::SetWireframe(false);
        GraphicsRenderState::SetDepthTest(true);
    }
}

void Mesh::UploadTransform()
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, this->position);
    model = glm::rotate(model, glm::radians(this->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(this->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(this->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, this->scale);

    this->modelBuffer.UploadData(glm::value_ptr(model), sizeof(glm::mat4));
}
