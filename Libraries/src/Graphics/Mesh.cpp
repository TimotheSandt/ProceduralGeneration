#include "Mesh.h"

Mesh::Mesh(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib)
{
    this->Initialize(vertices, indices, sizeAttrib);
}

Mesh::Mesh(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib, std::vector<GLfloat> instances, std::vector<GLuint> SizeAttribInstance)
{
    this->Initialize(vertices, indices, sizeAttrib, instances, SizeAttribInstance);
}

void Mesh::Initialize(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib) {
    this->Initialize(vertices, indices, sizeAttrib, {}, {});
}

void Mesh::Initialize(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLuint> sizeAttrib, std::vector<GLfloat> instances, std::vector<GLuint> SizeAttribInstance) {
    this->vertices = vertices;
    this->indices = indices;
    this->instancing = (instances.empty()) ? 1 : instances.size();

    VAO.Generate();
    VAO.Bind();
    VBO bVBO(this->vertices);
    EBO EBO(this->indices);

    int numComponents = 0;
    for (GLuint i = 0; i < sizeAttrib.size(); i++) {
        numComponents += sizeAttrib[i];
    }

    int offset = 0; 
    GLuint i = 0;
    for (; i < sizeAttrib.size(); i++) {
        this->VAO.LinkAttrib(bVBO, i, sizeAttrib[i], GL_FLOAT, numComponents * sizeof(GLfloat), (void*)(offset * sizeof(GLfloat)));
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
            this->VAO.LinkAttrib(instanceVBO, i, SizeAttribInstance[i - sizeAttrib.size()], GL_FLOAT, numComponents * sizeof(GLfloat), (void*)(offset * sizeof(GLfloat)));
            offset += SizeAttribInstance[i - sizeAttrib.size()];
        }

        i = sizeAttrib.size();
        for (; i < sizeAttrib.size() + SizeAttribInstance.size(); i++) {
            glVertexAttribDivisor(i, 1);
        }
        
        this->VAO.Unbind();
        bVBO.Unbind();
        instanceVBO.Unbind();
        EBO.Unbind();
    } else {
        this->VAO.Unbind();
        bVBO.Unbind();
        EBO.Unbind();
    }
}


void Mesh::Destroy() {
    this->VAO.Destroy();
    this->shader.Destroy();
    for (GLuint i = 0; i < this->textures.size(); i++) {
        this->textures[i].Destroy();
    }
}

void Mesh::AddTexture(Texture texture) { 
    this->textures.push_back(texture);
}

void Mesh::AddTexture(const char* image, const char* name, GLenum format, GLenum pixelType) {
    GLuint slot = this->textures.size();
    this->textures.push_back(Texture(image, name, slot, format, pixelType));
}

void Mesh::Draw(Camera& camera)
{
    glGetError(); // Clear any previous errors

    this->shader.Activate();
    if (glGetError() != GL_NO_ERROR) {
        std::cerr << "Error activating shader" << std::endl;
        return;
    }

    this->VAO.Bind();
    if (glGetError() != GL_NO_ERROR) {
        std::cerr << "Error binding VAO" << std::endl;
        return;
    }
    
    this->InitUniform3f("camPos", glm::value_ptr(camera.GetPosition()));
    camera.Matrix(this->shader, "camMatrix");
    if (glGetError() != GL_NO_ERROR) {
        std::cerr << "Error setting camera matrix" << std::endl;
        return;
    }

    for (GLuint i = 0; i < this->textures.size(); i++) {
        this->textures[i].texUnit(this->shader);
        
        this->textures[i].Bind();
    }
    if (glGetError() != GL_NO_ERROR) {
        std::cerr << "Error binding textures" << std::endl;
        return;
    }

    if (this->instancing > 1) {
        glDrawElementsInstanced(GL_TRIANGLES, this->indices.size(), GL_UNSIGNED_INT, 0, this->instancing);
    } else {
        glDrawElements(GL_TRIANGLES, this->indices.size(), GL_UNSIGNED_INT, 0);
    }
    if (glGetError() != GL_NO_ERROR) {
        std::cerr << "Error drawing mesh" << std::endl;
        return;
    }

    this->VAO.Unbind();
    if (glGetError() != GL_NO_ERROR) {
        std::cerr << "Error unbinding VAO" << std::endl;
        return;
    }
}

void Mesh::InitUniform()
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, this->position);
    model = glm::rotate(model, glm::radians(this->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(this->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(this->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, this->scale);

    this->InitUniformMatrix4f("model", glm::value_ptr(model));
}
