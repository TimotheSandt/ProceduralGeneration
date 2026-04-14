#include "Camera.h"
#include "InputManager.h"

namespace
{

glm::vec3 NormalizeOrFallback(const glm::vec3 &vector, const glm::vec3 &fallback)
{
    const float length = glm::length(vector);
    if (length <= 1e-6f)
    {
        return fallback;
    }

    return vector / length;
}

} // namespace

struct CameraUBO
{
    glm::vec3 position;
    float padding;
    glm::mat4 matrix;
};

Camera::Camera(int *width, int *height, glm::vec3 position) : position(position), width(width), height(height)
{
    this->Initialize(width, height, position);
}

Camera::Camera(const Camera &other) noexcept { this->Copy(other); }

Camera &Camera::operator=(const Camera &other) noexcept
{
    if (this != &other)
    {
        this->Destroy();
        this->Copy(other);
    }
    return *this;
}

Camera::Camera(Camera &&other) noexcept { this->Swap(other); };

Camera &Camera::operator=(Camera &&other) noexcept
{
    if (this != &other)
    {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

Camera::~Camera() { this->Destroy(); }

void Camera::Copy(const Camera &other)
{
    this->position = other.position;
    this->Orientation = other.Orientation;
    this->up = other.up;
    this->camMatrix = other.camMatrix;
    this->width = other.width;
    this->height = other.height;
    this->FOV = other.FOV;
    this->nearPlane = other.nearPlane;
    this->farPlane = other.farPlane;
    this->speed = other.speed;
    this->sensitivity = other.sensitivity;
    this->firstClick = other.firstClick;
    this->isWireframe = other.isWireframe;
    this->InitializeUBO();
    this->UploadCameraData();
}

void Camera::Swap(Camera &other) noexcept
{
    std::swap(this->position, other.position);
    std::swap(this->Orientation, other.Orientation);
    std::swap(this->up, other.up);
    std::swap(this->camMatrix, other.camMatrix);
    std::swap(this->width, other.width);
    std::swap(this->height, other.height);
    std::swap(this->speed, other.speed);
    std::swap(this->sensitivity, other.sensitivity);
    std::swap(this->isWireframe, other.isWireframe);
    std::swap(this->cameraBuffer, other.cameraBuffer);
}

void Camera::Destroy() { this->cameraBuffer.Destroy(); }

void Camera::Initialize(int *width, int *height, glm::vec3 position)
{
    this->position = position;
    this->width = width;
    this->height = height;
    this->InitializeUBO();
    this->InitializeInputs();
}

void Camera::InitializeInputs()
{
    InputManager::BindActionToInput("Camera::MoveForward", KeyButton::Z);
    InputManager::BindActionToInput("Camera::MoveBackward", KeyButton::S);
    InputManager::BindActionToInput("Camera::MoveLeft", KeyButton::Q);
    InputManager::BindActionToInput("Camera::MoveRight", KeyButton::D);
    InputManager::BindActionToInput("Camera::MoveUp", KeyButton::SPACE);
    InputManager::BindActionToInput("Camera::MoveDown", KeyButton::LEFT_SHIFT);
    InputManager::BindActionToInput("Camera::SpeedDown", KeyButton::E);
    InputManager::BindActionToInput("Camera::SpeedUp", KeyButton::A);
    InputManager::BindActionToInput("Camera::Rotate", MouseButton::RIGHT);
    InputManager::BindActionToInput("Camera::ToggleWireframe", KeyButton::W);
}

void Camera::UpdateMatrix()
{
    const glm::mat4 view = glm::lookAt(this->position, this->position + this->Orientation, this->up);
    glm::mat4 projection = glm::perspective(glm::radians(this->FOV),
                                            static_cast<float>(*this->width) / static_cast<float>(*this->height),
                                            this->nearPlane, this->farPlane);

    // Apply sub-pixel jitter to the projection matrix when a temporal mode is active.
    // projection[2] is the third column (NDC translation); [0] and [1] are X and Y.
    if (this->jitter.x != 0.0f || this->jitter.y != 0.0f)
    {
        projection[2][0] += this->jitter.x;
        projection[2][1] += this->jitter.y;
    }

    this->camMatrix = projection * view;

    this->UploadCameraData();
}

void Camera::UpdateMatrix(float FOVdeg, float nearPlane, float farPlane)
{
    this->FOV = FOVdeg;
    this->nearPlane = nearPlane;
    this->farPlane = farPlane;
    this->UpdateMatrix();
}

void Camera::Move(const glm::vec3 &delta) { this->position += delta; }

void Camera::MoveForward(float distance) { this->Move(this->Orientation * distance); }

void Camera::MoveRight(float distance) { this->Move(this->GetRight() * distance); }

void Camera::MoveUp(float distance) { this->Move(this->up * distance); }

void Camera::Rotate(const glm::vec3 &eulerDegrees)
{
    this->RotatePitch(eulerDegrees.x);
    this->RotateYaw(eulerDegrees.y);
    this->RotateRoll(eulerDegrees.z);
}

void Camera::RotateYaw(float degrees)
{
    this->Orientation = NormalizeOrFallback(glm::rotate(this->Orientation, glm::radians(degrees), this->up), this->Orientation);
}

void Camera::RotatePitch(float degrees)
{
    const glm::vec3 right = this->GetRight();
    const glm::vec3 rotatedOrientation = NormalizeOrFallback(glm::rotate(this->Orientation, glm::radians(degrees), right), this->Orientation);
    constexpr float minimumPitchAngleDegrees = 10.0f;
    const float minimumPitchAngleRadians = glm::radians(minimumPitchAngleDegrees);

    if (glm::angle(rotatedOrientation, this->up) > minimumPitchAngleRadians &&
        glm::angle(rotatedOrientation, -this->up) > minimumPitchAngleRadians)
    {
        this->Orientation = rotatedOrientation;
    }
}

void Camera::RotateRoll(float degrees)
{
    this->up = NormalizeOrFallback(glm::rotate(this->up, glm::radians(degrees), this->Orientation), this->up);
}

void Camera::Inputs(GLFWwindow *window, float ElapseTime)
{
    float speed = this->speed * ElapseTime;

    InputManager &inputManager = InputManager::GetInstance(window);

    if (inputManager.IsActionActive("Camera::MoveForward"))
    {
        this->MoveForward(speed);
    }
    if (inputManager.IsActionActive("Camera::MoveBackward"))
    {
        this->MoveForward(-speed);
    }
    if (inputManager.IsActionActive("Camera::MoveLeft"))
    {
        this->MoveRight(-speed);
    }
    if (inputManager.IsActionActive("Camera::MoveRight"))
    {
        this->MoveRight(speed);
    }
    if (inputManager.IsActionActive("Camera::MoveUp"))
    {
        this->MoveUp(speed);
    }
    if (inputManager.IsActionActive("Camera::MoveDown"))
    {
        this->MoveUp(-speed);
    }

    if (inputManager.IsActionActive("Camera::SpeedDown"))
    {
        this->speed = 1.0f;
    }
    else if (inputManager.IsActionActive("Camera::SpeedUp"))
    {
        this->speed = 25.0f;
    }
    else
    {
        this->speed = 6.0f;
    }

    if (inputManager.IsActionActive("Camera::Rotate"))
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

        if (firstClick)
        {
            const double centerX = static_cast<double>(*this->width) / 2.0;
            const double centerY = static_cast<double>(*this->height) / 2.0;
            glfwSetCursorPos(window, centerX, centerY);
            firstClick = false;
        }

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        const double centerX = static_cast<double>(*this->width) / 2.0;
        const double centerY = static_cast<double>(*this->height) / 2.0;
        float rotX = this->sensitivity * static_cast<float>(mouseX - centerX) / static_cast<float>(*this->width);
        float rotY = this->sensitivity * static_cast<float>(mouseY - centerY) / static_cast<float>(*this->height);

        this->RotatePitch(-rotY);
        this->RotateYaw(-rotX);

        glfwSetCursorPos(window, centerX, centerY);
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstClick = true;
    }

#if defined(_DEBUG) || defined(DEBUG)
    if (inputManager.IsActionActive("Camera::ToggleWireframe"))
    {
        SetWireframe(true);
    }
    else
    {
        SetWireframe(false);
    }
#endif
}

void Camera::InitializeUBO()
{
    this->cameraBuffer.Initialize(BufferUsage::Uniform, sizeof(CameraUBO), CAMERA_BINDING_POINT, true);
    this->UploadCameraData();
}

void Camera::UploadCameraData()
{
    CameraUBO data = {this->position, 0, this->camMatrix};
    this->cameraBuffer.UploadData(&data, sizeof(CameraUBO));
}

void Camera::Bind() const { this->cameraBuffer.BindToBindingPoint(); }

void Camera::ToggleWireframe() { SetWireframe(!this->isWireframe); }
