#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <GL/glew.h>

#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class CameraMovement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

constexpr float YAW = -90.0f;
constexpr float PITCH = -15.0f;
constexpr float SPEED = 2.5f;
constexpr float SENSITIVITY = 0.1f;
constexpr float ZOOM = 90.0f;

class Camera {
   public:
    // 相机属性
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    // 欧拉角
    float yaw;
    float pitch;
    // 相机选项
    float movementSpeed;
    float mouseSensitivity;
    float zoom;

    // 用向量的构造函数
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW,
           float pitch = PITCH);

    // 用标量的构造函数
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ,
           float yaw, float pitch);

    glm::mat4 getViewMatrix();

    void processKeyboard(CameraMovement direction, float deltaTime);

    void processMouseMovement(float xoffset, float yoffset,
                              GLboolean constrainPitch = true);

    void processMouseScroll(float yoffset);

   private:
    void updateCameraVectors();
};

#endif    // CAMERA_HPP