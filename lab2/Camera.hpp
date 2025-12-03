#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

enum class Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

constexpr float YAW         = -90.0f;
constexpr float PITCH       =  0.0f;
constexpr float SPEED       =  2.5f;
constexpr float SENSITIVITY =  0.1f;
constexpr float ZOOM        =  45.0f;

class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    // Euler Angles
    float yaw;
    float pitch;
    // Camera options
    float movementSpeed;
    float mouseSensitivity;
    float zoom;
    //向量初始化
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = YAW,
           float pitch = PITCH)
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
    //用标量 初始化
    Camera(float posX, float posY, float posZ,
           float upX, float upY, float upZ,
           float yaw, float pitch)
           : front(glm::vec3(0.0f, 0.0f, -1.0f)),
             movementSpeed(SPEED),
             mouseSensitivity(SENSITIVITY),
             zoom(ZOOM) {
        position = glm::vec3(posX, posY, posZ);
        worldUp = glm::vec3(upX, upY, upZ);
        this->yaw = yaw;
        this->pitch = pitch;
        updateCameraVectors();
    }

    /** 
    @brief: 计算视图矩阵
    @return: 视图矩阵
    */
    glm::mat4 GetViewMatrix() {
        return glm::lookAt(position, position + front, up);
    }

    void ProcessKeyboard(Camera_Movement direction, float deltaTime) {
        float velocity = movementSpeed * deltaTime;
        if (direction == Camera_Movement::FORWARD)
            position += front * velocity;
        if (direction == Camera_Movement::BACKWARD)
            position -= front * velocity;
        if (direction == Camera_Movement::LEFT)
            position -= right * velocity;
        if (direction == Camera_Movement::RIGHT)
            position += right * velocity;
    }

   void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true) {
       xoffset *= mouseSensitivity;
       yoffset *= mouseSensitivity;

       yaw   += xoffset;
       pitch += yoffset;

       // Make sure that when pitch is out of bounds, screen doesn't get flipped
       if (constrainPitch) {
           if (pitch > 89.0f)
               pitch = 89.0f;
           if (pitch < -89.0f)
               pitch = -89.0f;
       }

       // Update Front, Right and Up Vectors using the updated Euler angles
       updateCameraVectors();
   }

   void ProcessMouseScroll(float yoffset) {
       zoom -= (float)yoffset;
       if (zoom < 1.0f)
           zoom = 1.0f;
       if (zoom > 45.0f)
           zoom = 45.0f;
   }

private:
    void updateCameraVectors() {
        // Calculate the new Front vector
        glm::vec3 frontVec;
        frontVec.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        frontVec.y = std::sin(glm::radians(pitch));
        frontVec.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        front = glm::normalize(frontVec);
        // Also re-calculate the Right and Up vector
        right = glm::normalize(glm::cross(front, worldUp));  // Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        up    = glm::normalize(glm::cross(right, front));
    }

};

#endif // CAMERA_HPP