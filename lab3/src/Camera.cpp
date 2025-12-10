#include "Camera.hpp"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : front(glm::vec3(0.0f, 0.0f, -1.0f)),
      movementSpeed(SPEED),
      mouseSensitivity(SENSITIVITY),
      zoom(ZOOM),
      position(position),
      worldUp(up),
      yaw(yaw),
      pitch(pitch) {
    updateCameraVectors();
}

Camera::Camera(float posX, float posY, float posZ, float upX, float upY,
               float upZ, float yaw, float pitch)
    : front(glm::vec3(0.0f, 0.0f, -1.0f)),
      movementSpeed(SPEED),
      mouseSensitivity(SENSITIVITY),
      zoom(ZOOM),
      position(glm::vec3(0.0f, 0.0f, 0.0f)),
      worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
      yaw(YAW),
      pitch(PITCH) {
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() {
    return glm::lookAt(this->position, this->position + this->front, this->up);
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime) {
    float velocity = this->movementSpeed * deltaTime;
    if (direction == CameraMovement::FORWARD)
        this->position += this->front * velocity;
    if (direction == CameraMovement::BACKWARD)
        this->position -= this->front * velocity;
    if (direction == CameraMovement::LEFT)
        this->position -= this->right * velocity;
    if (direction == CameraMovement::RIGHT)
        this->position += this->right * velocity;
    if (direction == CameraMovement::UP)
        this->position += this->up * velocity;
    if (direction == CameraMovement::DOWN)
        this->position -= this->up * velocity;
}

void Camera::processMouseMovement(float xoffset, float yoffset,
                                  GLboolean constrainPitch) {
    xoffset *= this->mouseSensitivity;
    yoffset *= this->mouseSensitivity;

    this->yaw += xoffset;
    this->pitch += yoffset;

    // 限制俯仰角
    if (constrainPitch) {
        if (this->pitch > 89.0f) this->pitch = 89.0f;
        if (this->pitch < -89.0f) this->pitch = -89.0f;
    }

    // 更新前向、右向和上向量
    updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset) {
    this->zoom -= (float)yoffset;
    if (this->zoom < 1.0f) this->zoom = 1.0f;
    if (this->zoom > 45.0f) this->zoom = 45.0f;
}

void Camera::updateCameraVectors() {
    // 计算新的前向向量
    glm::vec3 front;
    front.x = cos(glm::radians(this->yaw)) * cos(glm::radians(this->pitch));
    front.y = sin(glm::radians(this->pitch));
    front.z = sin(glm::radians(this->yaw)) * cos(glm::radians(this->pitch));
    this->front = glm::normalize(front);
    // 重新计算右向和上向量
    this->right = glm::normalize(glm::cross(this->front, this->worldUp));
    this->up = glm::normalize(glm::cross(this->right, this->front));
}
