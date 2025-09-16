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

Camera::~Camera() {
    this->Destroy();
}

void Camera::Destroy() {
    this->UBO.Destroy();
}


void Camera::Initialize(int *width, int *height, glm::vec3 position) {
    this->position = position;
    this->width = width;
    this->height = height;
    this->UBO.initialize(sizeof(CameraUBO), CAMERA_BINDING_POINT, GL_DYNAMIC_DRAW);
}


void Camera::UpdateMatrix(float FOVdeg, float nearPlane, float farPlane) {
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);

    view = glm::lookAt(this->position, this->position + this->Orientation, this->up);
    projection = glm::perspective(glm::radians(FOVdeg), (float)(*this->width) / *this->height, nearPlane, farPlane);

    this->camMatrix = projection * view;
    
    this->UpdateUBO();
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

void Camera::UpdateUBO() {
    CameraUBO data = { this->position, 0, this->camMatrix };
    this->UBO.uploadData(&data, sizeof(CameraUBO));
}

void Camera::BindUBO() {
    this->UBO.BindToBindingPoint();
}



void Camera::SetWireframe(bool enabled) {
    this->isWireframe = enabled;
}

bool Camera::GetWireframe() const {
    return this->isWireframe;
}

void Camera::ToggleWireframe() {
    SetWireframe(!this->isWireframe);
}