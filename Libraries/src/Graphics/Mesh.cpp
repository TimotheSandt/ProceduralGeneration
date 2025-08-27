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
    //this->instancing = (instances.empty()) ? 1 : instances.size();
    /**/
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
    /**/
    
    
    this->VAO.initialize();
    if (glGetError() != GL_NO_ERROR) { 
        std::cout << "VAO initialization failed" << std::endl; 
        throw std::runtime_error("VAO initialization failed");
    }
    this->VAO.Generate();
    this->VAO.Bind();

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


void Mesh::Render(Camera& camera)
{
    this->shader.Bind();
    this->VAO.Bind();
    this->InitUniform3f("camPos", glm::value_ptr(camera.GetPosition()));
    camera.Matrix(this->shader, "camMatrix");
    for (GLuint i = 0; i < this->textures.size(); i++) {
        this->textures[i].texUnit(this->shader);
        
        this->textures[i].Bind();
    }

    this->Draw();
    if (camera.IsWireframe()) {
        GLint wireframe = GL_TRUE;
        this->InitUniform1i("wireframe", &wireframe);
        this->Draw(true);
        wireframe = GL_FALSE;
        this->InitUniform1i("wireframe", &wireframe);
    }
    
    this->VAO.Unbind();
    this->shader.Unbind();
    for (GLuint i = 0; i < this->textures.size(); i++) {
        this->textures[i].Unbind();
    }
}

void Mesh::Draw(bool wireframe) {
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
