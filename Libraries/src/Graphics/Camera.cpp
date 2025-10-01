#include "Camera.h"

struct CameraUBO {
    glm::vec3 position;
    float padding;
    glm::mat4 matrix;
};

Camera::Camera(int *width, int *height, glm::vec3 position)
    : position(position), width(width), height(height) {
        this->Initialize(width, height, position);
    }

Camera::Camera(const Camera& other) noexcept {
    this->CopyFrom(other);
}

Camera& Camera::operator=(const Camera& other) noexcept {
    if (this != &other) {
        this->Destroy();
        this->CopyFrom(other);
    }
    return *this;
}

Camera::Camera(Camera&& other) noexcept {
    this->Swap(other);
};

Camera& Camera::operator=(Camera&& other) noexcept {
    if (this != &other) {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}


Camera::~Camera() {
    this->Destroy();
}

void Camera::CopyFrom(const Camera& other) {
    this->position = other.position;
    this->Orientation = other.Orientation;
    this->up = other.up;
    this->camMatrix = other.camMatrix;
    this->speed = other.speed;
    this->sensitivity = other.sensitivity;
    this->isWireframe = other.isWireframe;
    this->InitializeUBO();
    this->UpdateUBO();
}

void Camera::Swap(Camera& other) noexcept {
    std::swap(this->position, other.position);
    std::swap(this->Orientation, other.Orientation);
    std::swap(this->up, other.up);
    std::swap(this->camMatrix, other.camMatrix);
    std::swap(this->width, other.width);
    std::swap(this->height, other.height);
    std::swap(this->speed, other.speed);
    std::swap(this->sensitivity, other.sensitivity);
    std::swap(this->isWireframe, other.isWireframe);
    std::swap(this->bUBO, other.bUBO);
}

void Camera::Destroy() {
    this->bUBO.Destroy();
}


void Camera::Initialize(int *width, int *height, glm::vec3 position) {
    this->position = position;
    this->width = width;
    this->height = height;
    this->InitializeUBO();
}

void Camera::UpdateMatrix() {
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);

    view = glm::lookAt(this->position, this->position + this->Orientation, this->up);
    projection = glm::perspective(glm::radians(this->FOV), (float)(*this->width) / *this->height, this->nearPlane, this->farPlane);

    this->camMatrix = projection * view;
    
    this->UpdateUBO();
}

void Camera::UpdateMatrix(float FOVdeg, float nearPlane, float farPlane) {
    this->FOV = FOVdeg;
    this->nearPlane = nearPlane;
    this->farPlane = farPlane;
    this->UpdateMatrix();
}

void Camera::Inputs(GLFWwindow* window, float ElapseTime) {
    float speed = this->speed * ElapseTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        this->position += speed * this->Orientation;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        this->position -= speed * this->Orientation;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        this->position -= glm::normalize(glm::cross(this->Orientation, this->up)) * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        this->position += glm::normalize(glm::cross(this->Orientation, this->up)) * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        this->position += this->up * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        this->position -= this->up * speed;
    }

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        this->speed = 1.0f;
    } else if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_RELEASE) {
        this->speed = 6.0f;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        this->speed = 25.0f;
    } else if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_RELEASE) {
        this->speed = 6.0f;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

        if (firstClick) {
            glfwSetCursorPos(window, (*this->width / 2), (*this->height / 2));
            firstClick = false;
        }

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        float rotX = this->sensitivity * (float)(mouseX - (*this->width / 2)) / (float)(*this->width);
        float rotY = this->sensitivity * (float)(mouseY - (*this->height / 2)) / (float)(*this->height);

        glm::vec3 newOrientation = glm::rotate(this->Orientation, glm::radians(-rotY), glm::normalize(glm::cross(this->Orientation, this->up)));

        if (!(glm::angle(newOrientation, this->up) <= glm::radians(10.0f)) or !(glm::angle(newOrientation, -this->up) <= glm::radians(10.0f))) {
            this->Orientation = newOrientation;
        }

        this->Orientation = glm::rotate(this->Orientation, glm::radians(-rotX), this->up);

        glfwSetCursorPos(window, (*this->width / 2), (*this->height / 2));
    }
    else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstClick = true;
    }

#if defined(_DEBUG) || defined(DEBUG)
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        SetWireframe(true);
    } else if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_RELEASE) {
        SetWireframe(false);
    }
#endif
}

void Camera::InitializeUBO() {
    this->bUBO.initialize(sizeof(CameraUBO), CAMERA_BINDING_POINT, GL_DYNAMIC_DRAW);
    this->UpdateUBO();
}

void Camera::UpdateUBO() {
    CameraUBO data = { this->position, 0, this->camMatrix };
    this->bUBO.uploadData(&data, sizeof(CameraUBO));
}

void Camera::BindUBO() {
    this->bUBO.BindToBindingPoint();
}




void Camera::ToggleWireframe() {
    SetWireframe(!this->isWireframe);
}