#include "Mesh.h"

#include <bit>
#include <cstdint>
#include <utility>

#include "Graphics/Core/GraphicsRuntime.h"

namespace
{
void *VertexAttribOffset(std::size_t bytes) { return std::bit_cast<void *>(static_cast<std::uintptr_t>(bytes)); }
} // namespace

Mesh::Mesh(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib)
{
    this->Initialize(std::move(vertices), std::move(indices), std::move(sizeAttrib));
}

Mesh::Mesh(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib, std::vector<GLfloat> instances,
           std::vector<GLuint> SizeAttribInstance)
{
    this->Initialize(std::move(vertices), std::move(indices), std::move(sizeAttrib), std::move(instances), std::move(SizeAttribInstance));
}

Mesh::Mesh(const Mesh &mesh)
{
    this->Initialize(mesh.vertices, mesh.indices, mesh.sizeAttrib, mesh.instances, mesh.SizeAttribInstance);
    for (const Texture &texture : mesh.textures)
    {
        this->textures.push_back(texture.Copy());
    }
    this->shader = Shader(mesh.shader);
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
    this->Initialize(mesh.vertices, mesh.indices, mesh.sizeAttrib, mesh.instances, mesh.SizeAttribInstance);
    for (const Texture &texture : mesh.textures)
    {
        this->textures.push_back(texture.Copy());
    }
    this->shader = Shader(mesh.shader);
    this->position = mesh.position;
    this->scale = mesh.scale;
    this->rotation = mesh.rotation;
    return *this;
}

Mesh::Mesh(Mesh &&mesh) noexcept
    : vertices(std::move(mesh.vertices)), indices(std::move(mesh.indices)), sizeAttrib(std::move(mesh.sizeAttrib)),
      textures(std::move(mesh.textures)), shader(std::move(mesh.shader)), position(std::move(mesh.position)), scale(std::move(mesh.scale)),
      rotation(std::move(mesh.rotation)), instancing(mesh.instancing), instances(std::move(mesh.instances)),
      SizeAttribInstance(std::move(mesh.SizeAttribInstance)), geometry(std::move(mesh.geometry)), modelBuffer(std::move(mesh.modelBuffer)),
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
    std::swap(this->SizeAttribInstance, mesh.SizeAttribInstance);
    std::swap(this->textures, mesh.textures);
    std::swap(this->instancing, mesh.instancing);
    std::swap(this->geometry, mesh.geometry);
    std::swap(this->modelBuffer, mesh.modelBuffer);
    std::swap(this->shader, mesh.shader);
    std::swap(this->position, mesh.position);
    std::swap(this->scale, mesh.scale);
    std::swap(this->rotation, mesh.rotation);
}

void Mesh::Initialize(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib)
{
    this->Initialize(std::move(vertices), std::move(indices), std::move(sizeAttrib), {}, {});
}

void Mesh::Initialize(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib,
                      std::vector<GLfloat> instances, std::vector<GLuint> SizeAttribInstance)
{
    this->vertices = std::move(vertices);
    this->indices = std::move(indices);
    this->sizeAttrib = sizeAttrib;
    this->instances = instances;
    this->SizeAttribInstance = SizeAttribInstance;

    if (instances.empty())
    {
        this->instancing = 1;
    }
    else
    {
        // Calculate instances based on total components per instance
        int componentsPerInstance = 0;
        for (GLuint size : SizeAttribInstance)
        {
            componentsPerInstance += static_cast<int>(size);
        }
        this->instancing = (componentsPerInstance > 0) ? instances.size() / componentsPerInstance : 1;
    }

    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr)
    {
        GeometryCreateInfo createInfo;
        createInfo.layout.vertexAttributes = this->sizeAttrib;
        createInfo.layout.instanceAttributes = this->SizeAttribInstance;
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
    this->shader.Destroy();
    for (GLuint i = 0; i < this->textures.size(); i++)
    {
        this->textures[i].Destroy();
    }
    this->textures.clear();
    this->FreeCache();
}

void Mesh::AddTexture(Texture texture) { this->textures.push_back(texture.Copy()); }

void Mesh::AddTexture(const char *image, const char *name, GLenum format, GLenum pixelType)
{
    GLuint slot = this->textures.size();
    this->textures.push_back(Texture(image, name, slot, format, pixelType));
}

void Mesh::Render(Camera &camera)
{
    if (!this->shader.IsCompiled())
    {
        LOG_WARNING("Shader not compiled");
        return;
    }
    this->shader.Bind();
    if (this->geometry != nullptr)
    {
        this->geometry->Bind();
    }
    for (GLuint i = 0; i < this->textures.size(); i++)
    {
        this->textures[i].texUnit(this->shader);

        this->textures[i].Bind();
    }
    this->modelBuffer.BindToBindingPoint();
    this->Draw();
    if (camera.IsWireframe())
    {
        GLint wireframe = GL_TRUE;
        this->InitUniform1i("wireframe", &wireframe);
        this->Draw(true);
        wireframe = GL_FALSE;
        this->InitUniform1i("wireframe", &wireframe);
    }

    if (this->geometry != nullptr)
    {
        this->geometry->Unbind();
    }
    this->shader.Unbind();
    this->modelBuffer.Unbind();
    for (GLuint i = 0; i < this->textures.size(); i++)
    {
        this->textures[i].Unbind();
    }
}

void Mesh::Draw(bool wireframe) const
{
    if (wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        // Optional: disable depth testing for wireframe to avoid z-fighting
        // glDisable(GL_DEPTH_TEST);
    }
    else
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        // glEnable(GL_DEPTH_TEST);
    }

    if (this->instancing > 1)
    {
        glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(this->indices.size()), GL_UNSIGNED_INT, nullptr,
                                static_cast<GLsizei>(this->instancing));
    }
    else
    {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(this->indices.size()), GL_UNSIGNED_INT, nullptr);
    }

    // Reset to fill mode after drawing
    if (wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
    }
}

void Mesh::UpdateUBO()
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, this->position);
    model = glm::rotate(model, glm::radians(this->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(this->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(this->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, this->scale);

    this->modelBuffer.UploadData(glm::value_ptr(model), sizeof(glm::mat4));
}
