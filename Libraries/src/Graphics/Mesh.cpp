#include "Mesh.h"

Mesh::Mesh(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib) {
    this->Initialize(vertices, indices, sizeAttrib);
}

Mesh::Mesh(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib, std::vector<GLfloat> instances, std::vector<GLuint> SizeAttribInstance) {
    this->Initialize(vertices, indices, sizeAttrib, instances, SizeAttribInstance);
}

Mesh::Mesh(const Mesh& mesh) noexcept {
    this->Initialize(mesh.vertices, mesh.indices, mesh.sizeAttrib, mesh.instances, mesh.SizeAttribInstance);
}

Mesh Mesh::operator=(const Mesh& mesh) noexcept {
    this->Initialize(mesh.vertices, mesh.indices, mesh.sizeAttrib, mesh.instances, mesh.SizeAttribInstance);
    return *this;
}

Mesh::Mesh(Mesh&& mesh) noexcept {
    this->Swap(mesh);
}

Mesh Mesh::operator=(Mesh&& mesh) noexcept {
    if (this != &mesh) {
        this->Destroy();
        this->Swap(mesh);
    }
    return *this;
}

void Mesh::Swap(Mesh& mesh) noexcept {
    std::swap(this->vertices, mesh.vertices);
    std::swap(this->indices, mesh.indices);
    std::swap(this->sizeAttrib, mesh.sizeAttrib);
    std::swap(this->instances, mesh.instances);
    std::swap(this->SizeAttribInstance, mesh.SizeAttribInstance);
    std::swap(this->instancing, mesh.instancing);
    std::swap(this->bVAO, mesh.bVAO);
    std::swap(this->bUBO, mesh.bUBO);
    std::swap(this->shader, mesh.shader);
    std::swap(this->position, mesh.position);
    std::swap(this->scale, mesh.scale);
    std::swap(this->rotation, mesh.rotation);
}

void Mesh::Initialize(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib) {
    this->Initialize(vertices, indices, sizeAttrib, {}, {});
}

void Mesh::Initialize(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib, std::vector<GLfloat> instances, std::vector<GLuint> SizeAttribInstance) {
    this->vertices = vertices;
    this->indices = indices;
    this->sizeAttrib = sizeAttrib;
    this->instances = instances;
    this->SizeAttribInstance = SizeAttribInstance;
    
    if (instances.empty()) {
        this->instancing = 1;
    } else {
        // Calculate instances based on total components per instance
        int componentsPerInstance = 0;
        for (GLuint size : SizeAttribInstance) {
            componentsPerInstance += size;
        }
        this->instancing = (componentsPerInstance > 0) ? instances.size() / componentsPerInstance : 1;
    }
    
    
    this->bVAO.Initialize();
    if (glGetError() != GL_NO_ERROR) {
        LOG_ERROR(1, "VAO initialization failed");
    }
    this->bVAO.Bind();

    VBO bVBO(this->vertices);
    EBO bEBO(this->indices);

    int numComponents = 0;
    for (GLuint i = 0; i < sizeAttrib.size(); i++) {
        numComponents += sizeAttrib[i];
    }

    int offset = 0; 
    GLuint i = 0;
    for (; i < sizeAttrib.size(); i++) {
        this->bVAO.LinkAttrib(bVBO, i, sizeAttrib[i], GL_FLOAT, numComponents * sizeof(GLfloat), (void*)(offset * sizeof(GLfloat)));
        offset += sizeAttrib[i];
    }

    if (!instances.empty()) {
        VBO instanceVBO(instances);
        instanceVBO.Bind();

        numComponents = 0;
        for (GLuint i = 0; i < SizeAttribInstance.size(); i++) {
            numComponents += SizeAttribInstance[i];
        }

        offset = 0;
        i = sizeAttrib.size();
        for (; i < sizeAttrib.size() + SizeAttribInstance.size(); i++) {
            this->bVAO.LinkAttrib(instanceVBO, i, SizeAttribInstance[i - sizeAttrib.size()], GL_FLOAT, numComponents * sizeof(GLfloat), (void*)(offset * sizeof(GLfloat)));
            offset += SizeAttribInstance[i - sizeAttrib.size()];
        }

        i = sizeAttrib.size();
        for (; i < sizeAttrib.size() + SizeAttribInstance.size(); i++) {
            glVertexAttribDivisor(i, 1);
        }
        
        this->bVAO.Unbind();
        bVBO.Unbind();
        instanceVBO.Unbind();
        bEBO.Unbind();
    } else {
        this->bVAO.Unbind();
        bVBO.Unbind();
        bEBO.Unbind();
    }

    this->bUBO.initialize(sizeof(glm::mat4), MESH_MODEL_BINDING_POINT);
}


void Mesh::Destroy() {
    this->bVAO.Destroy();
    this->bUBO.Destroy();
    this->shader.Destroy();
    for (GLuint i = 0; i < this->textures.size(); i++) {
        this->textures[i].Destroy();
    }
    this->textures.clear();
    this->FreeCache();
}

void Mesh::AddTexture(Texture texture) {
    this->textures.push_back(texture.Copy());
}

void Mesh::AddTexture(const char* image, const char* name, GLenum format, GLenum pixelType) {
    GLuint slot = this->textures.size();
    this->textures.push_back(Texture(image, name, slot, format, pixelType));
}


void Mesh::Render(Camera& camera) {
    if (!this->shader.IsCompiled()) {
        LOG_WARNING("Shader not compiled");
        return;
    }
    this->shader.Bind();
    this->bVAO.Bind();
    for (GLuint i = 0; i < this->textures.size(); i++) {
        this->textures[i].texUnit(this->shader);
        
        this->textures[i].Bind();
    }
    this->bUBO.BindToBindingPoint();
    this->Draw();
    if (camera.IsWireframe()) {
        GLint wireframe = GL_TRUE;
        this->InitUniform1i("wireframe", &wireframe);
        this->Draw(true);
        wireframe = GL_FALSE;
        this->InitUniform1i("wireframe", &wireframe);
    }
    
    this->bVAO.Unbind();
    this->shader.Unbind();
    this->bUBO.Unbind();
    for (GLuint i = 0; i < this->textures.size(); i++) {
        this->textures[i].Unbind();
    }
}

void Mesh::Draw(bool wireframe) const {
    if (wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        // Optional: disable depth testing for wireframe to avoid z-fighting
        // glDisable(GL_DEPTH_TEST);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        // glEnable(GL_DEPTH_TEST);
    }

    if (this->instancing > 1) {
        glDrawElementsInstanced(GL_TRIANGLES, this->indices.size(), GL_UNSIGNED_INT, 0, this->instancing);
    } else {
        glDrawElements(GL_TRIANGLES, this->indices.size(), GL_UNSIGNED_INT, 0);
    }

    // Reset to fill mode after drawing
    if (wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
    }
}

void Mesh::UpdateUBO() {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, this->position);
    model = glm::rotate(model, glm::radians(this->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(this->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(this->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, this->scale);

    this->bUBO.uploadData(glm::value_ptr(model), sizeof(glm::mat4));
}
