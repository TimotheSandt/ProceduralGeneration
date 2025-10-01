#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "Shader.h"
#include "UBO.h"

class Camera
{
public:
    Camera() = default;
    Camera(int *width, int *height, glm::vec3 position);

    Camera(const Camera&) noexcept;
    Camera& operator=(const Camera&) noexcept;

    Camera(Camera&&) noexcept;
    Camera& operator=(Camera&&) noexcept;

    ~Camera();
    void Destroy();
    
    void Initialize(int *width, int *height, glm::vec3 position);
    void UpdateMatrix();
    void UpdateMatrix(float FOVdeg, float nearPlane, float farPlane);
    void Inputs(GLFWwindow* window, float ElapseTime);

    void BindUBO() const;
    
public:
    void SetPosition(glm::vec3 position) { this->position = position; }
    void SetOrientation(glm::vec3 orientation) { this->Orientation = orientation; }
    void SetUp(glm::vec3 up) { this->up = up; }
    void SetWireframe(bool enabled) { this->isWireframe = enabled; }
    void SetFOV(float fov) { this->FOV = fov; }
    void SetNearPlane(float nearPlane) { this->nearPlane = nearPlane; }
    void SetFarPlane(float farPlane) { this->farPlane = farPlane; }

    glm::vec3 GetPosition() const { return this->position; }
    glm::vec3 GetOrientation() const { return this->Orientation; }
    glm::vec3 GetUp() const { return this->up; }
    glm::mat4 GetMatrix() const { return this->camMatrix; }
    glm::mat4 GetViewMatrix() const { return glm::lookAt(this->position, this->position + this->Orientation, this->up); }
    bool IsWireframe() const { return this->isWireframe; }
    float GetFOV() const { return this->FOV; }
    float GetNearPlane() const { return this->nearPlane; }
    float GetFarPlane() const { return this->farPlane; }


private:
    void Copy(const Camera& other);
    void Swap(Camera& other) noexcept;

private:
    glm::vec3 position;
    glm::vec3 Orientation = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 camMatrix = glm::mat4(1.0f);

    int *width, *height;

    float FOV = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    float speed = 6.0f;
    float sensitivity = 100.0f;

    bool firstClick = true;

    UBO bUBO;
    bool isWireframe = false;

private:
    void ToggleWireframe();

    void InitializeUBO();
    void UpdateUBO();

};