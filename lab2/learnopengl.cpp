#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "Camera.hpp"
#include "shader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

float lastx = 400, lasty = 300;
bool firstMouse = true;
float deltaTime = 0.0f; // 当前帧与上一帧的时间差
float lastFrame = 0.0f; // 上一帧的时间

//导入纹理
unsigned int loadTexture(const char* path){
    unsigned int textureID;

    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }
    return textureID;
} 

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    // 摄像机控制
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(Camera_Movement::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(Camera_Movement::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(Camera_Movement::LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(Camera_Movement::RIGHT, deltaTime);
}

//鼠标移动回调
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastx = xpos;
        lasty = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastx;
    float yoffset = lasty - ypos; // reversed since y-coordinates go from bottom to top
    lastx = xpos;
    lasty = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

//滚轮回调
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

int main() {
    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // 创建窗口
    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL Demo", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }

    // 设置当前上下文
    glfwMakeContextCurrent(window);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // 初始化 GLEW（必须在上下文创建后）
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: "
                  << glewGetErrorString(err) << "\n";
        return -1;
    }

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GLEW version: " << glewGetString(GLEW_VERSION) << "\n";

    Shader shader_diamond("../../lab2/expr2.vs", "../../lab2/expr2.fs");
    unsigned int texture1 = loadTexture("../../lab2/diamond.jpg");
    shader_diamond.use();
    shader_diamond.setInt("texture1", 0);

    float diamondVertices[] = {
        // 位置           // 纹理坐标
         0.5f ,0.0f, 0.0f,  0.0f, 0.0f, // 顶部
         0.0f ,0.5f ,0.0f,  1.0f, 0.0f, // 右侧
         0.0f,  0.0f, 0.5f,  1.0f, 1.0f, // 顶部
        -0.5f,  0.0f, 0.0f,  0.0f, 0.0f, // 左侧
         0.0f, -0.5f, 0.0f,  1.0f, 0.0f, // 底部
         0.5f,  0.0f, -0.5f,  1.0f, 1.0f  // 右侧
    };
    unsigned int diamondIndices[] = {
            2,1,0,
            2,0,4,
            2,4,3,
            2,3,1,
            4,0,5,
            0,1,5,
            1,3,5,
            0,4,5
    };

    glEnable(GL_DEPTH_TEST);

    unsigned int diamondVAO, diamondVBO, diamondEBO;
    glGenVertexArrays(1, &diamondVAO);
    glGenBuffers(1, &diamondVBO);
    glGenBuffers(1, &diamondEBO);

    glBindVertexArray(diamondVAO);

    glBindBuffer(GL_ARRAY_BUFFER, diamondVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(diamondVertices), diamondVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, diamondEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(diamondIndices), diamondIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);



    // 渲染循环
    while (!glfwWindowShouldClose(window)) {
        // 计算时间差
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        // 处理输入
        processInput(window);
    }
    shader_diamond.use();
    shader_diamond.setMat4("model", glm::mat4(1.0f));
    shader_diamond.setMat4("view", camera.GetViewMatrix());
    shader_diamond.setMat4("projection", glm::perspective(glm::radians(camera.zoom), 800.0f / 600.0f, 0.1f, 100.0f));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture1);
    glBindVertexArray(diamondVAO);
    glDrawElements(GL_TRIANGLES, 24, GL_UNSIGNED_INT, 0);

    glfwTerminate();
    return 0;
}

 