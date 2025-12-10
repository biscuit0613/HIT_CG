#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/fwd.hpp>
#include <iostream>

#include "Camera.hpp"
#include "Shader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <filesystem>

#include "stb_image.h"

namespace fs = std::filesystem;

constexpr unsigned int SCR_WIDTH = 800;
constexpr unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 1.0f, 0.0f));

static float lastX = SCR_WIDTH / 2.0f;
static float lastY = SCR_HEIGHT / 2.0f;
static bool firstMouse = true;
static float deltaTime = 0.0f;  
static float lastFrame = 0.0f;    

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera.processKeyboard(CameraMovement::FORWARD, deltaTime);
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera.processKeyboard(CameraMovement::BACKWARD, deltaTime);
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera.processKeyboard(CameraMovement::LEFT, deltaTime);
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera.processKeyboard(CameraMovement::RIGHT, deltaTime);
    }

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        camera.processKeyboard(CameraMovement::DOWN, deltaTime);
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        camera.processKeyboard(CameraMovement::UP, deltaTime);
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset =
        lastY - ypos;    // 注意这里是相反的，因为y坐标是从底部往顶部依次增大的

    lastX = xpos;
    lastY = ypos;

    camera.processMouseMovement(xoffset, yoffset);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.processMouseScroll(static_cast<float>(yoffset));
}

unsigned int loadTexture(fs::path path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data =
        stbi_load(path.string().c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        GL_REPEAT);    // 设置纹理环绕方式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

int main() {
    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // 创建窗口
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Learn OpenGL",
                                          nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }

    // 设置当前上下文
    glfwMakeContextCurrent(window);

    // 初始化 GLEW
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err)
                  << "\n";
        return -1;
    }

    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GLEW version: " << glewGetString(GLEW_VERSION) << "\n";

    /*
    float cubeVertices[] = {
        // positions // texture Coords // normals
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,    //
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f,     //
        0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f,      //
        0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f,      //
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f,     //
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,    //
                                                               //
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,      //
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,       //
        0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,        //
        0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,        //
        -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,       //
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,      //
                                                               //
        -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,      //
        -0.5f, 0.5f, -0.5f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f,     //
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,    //
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,    //
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,     //
        -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,      //
                                                               //
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,        //
        0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,       //
        0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,      //
        0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,      //
        0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,       //
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,        //
                                                               //
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,    //
        0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f,     //
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f,      //
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f,      //
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,     //
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,    //
                                                               //
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,      //
        0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,       //
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,        //
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,        //
        -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,       //
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f       //
    };
    */

    // 正八面体顶点数据 (position.xyz, texCoord.xy, normal.xyz)
    // 8 个三角形面，每个面 3 个顶点 -> 24 个顶点
    float diamondVertices[] = {
        // Top Front Right (normal: 1,1,1)
        0.0f,  0.5f,  0.0f,   0.5f, 1.0f,   1.0f, 1.0f, 1.0f,
        0.0f,  0.0f,  0.5f,   0.5f, 0.5f,   1.0f, 1.0f, 1.0f,
        0.5f,  0.0f,  0.0f,   1.0f, 0.5f,   1.0f, 1.0f, 1.0f,
        // Top Right Back (normal: 1,1,-1)
        0.0f,  0.5f,  0.0f,   0.5f, 1.0f,   1.0f, 1.0f, -1.0f,
        0.5f,  0.0f,  0.0f,   1.0f, 0.5f,   1.0f, 1.0f, -1.0f,
        0.0f,  0.0f, -0.5f,   0.5f, 0.5f,   1.0f, 1.0f, -1.0f,
        // Top Back Left (normal: -1,1,-1)
        0.0f,  0.5f,  0.0f,   0.5f, 1.0f,  -1.0f, 1.0f, -1.0f,
        0.0f,  0.0f, -0.5f,   0.5f, 0.5f,  -1.0f, 1.0f, -1.0f,
       -0.5f,  0.0f,  0.0f,   0.0f, 0.5f,  -1.0f, 1.0f, -1.0f,
        // Top Left Front (normal: -1,1,1)
        0.0f,  0.5f,  0.0f,   0.5f, 1.0f,  -1.0f, 1.0f,  1.0f,
       -0.5f,  0.0f,  0.0f,   0.0f, 0.5f,  -1.0f, 1.0f,  1.0f,
        0.0f,  0.0f,  0.5f,   0.5f, 0.5f,  -1.0f, 1.0f,  1.0f,
        // Bottom Front Left (normal: -1,-1,1)
        0.0f, -0.5f,  0.0f,   0.5f, 0.0f,  -1.0f, -1.0f, 1.0f,
        0.0f,  0.0f,  0.5f,   0.5f, 0.5f,  -1.0f, -1.0f, 1.0f,
       -0.5f,  0.0f,  0.0f,   0.0f, 0.5f,  -1.0f, -1.0f, 1.0f,
        // Bottom Left Back (normal: -1,-1,-1)
        0.0f, -0.5f,  0.0f,   0.5f, 0.0f,  -1.0f, -1.0f, -1.0f,
       -0.5f,  0.0f,  0.0f,   0.0f, 0.5f,  -1.0f, -1.0f, -1.0f,
        0.0f,  0.0f, -0.5f,   0.5f, 0.5f,  -1.0f, -1.0f, -1.0f,
        // Bottom Back Right (normal: 1,-1,-1)
        0.0f, -0.5f,  0.0f,   0.5f, 0.0f,   1.0f, -1.0f, -1.0f,
        0.0f,  0.0f, -0.5f,   0.5f, 0.5f,   1.0f, -1.0f, -1.0f,
        0.5f,  0.0f,  0.0f,   1.0f, 0.5f,   1.0f, -1.0f, -1.0f,
        // Bottom Right Front (normal: 1,-1,1)
        0.0f, -0.5f,  0.0f,   0.5f, 0.0f,   1.0f, -1.0f, 1.0f,
        0.5f,  0.0f,  0.0f,   1.0f, 0.5f,   1.0f, -1.0f, 1.0f,
        0.0f,  0.0f,  0.5f,   0.5f, 0.5f,   1.0f, -1.0f, 1.0f
    };
    // unsigned int cubeIndices[] = {
        
    // };


    glEnable(GL_DEPTH_TEST);

    unsigned int VBO, VAO;
    // EBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    // glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    //glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices,GL_STATIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, sizeof(diamondVertices), &diamondVertices,
                 GL_STATIC_DRAW);
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    // glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices),
    //              cubeIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)0);
 
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(5 * sizeof(float)));

    Shader shader_cube("../res/expr3.vert",
                          "../res/expr3.frag");
    // unsigned int texture = loadTexture("../res/yellow.png");
    unsigned int texture = loadTexture("../res/diamond.png");
    

    shader_cube.use();
    shader_cube.setInt("texture1", 0);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        shader_cube.use();
        //Model矩阵：
        shader_cube.setMat4("model", glm::mat4(1.0f));
        //View矩阵：
        shader_cube.setMat4("view", camera.getViewMatrix());
        //Projection矩阵：
        //最后在着色器里面相乘，实现透视投影
        shader_cube.setMat4(
            "projection", glm::perspective(glm::radians(camera.zoom),
                                           (float)SCR_WIDTH / (float)SCR_HEIGHT,
                                           0.1f, 100.0f));
        //lightPos:光源在世界坐标系里面的位置
        shader_cube.setVec3("lightPos", 1.0f, 1.0f, 1.0f);
        //把摄像机位置传给片段着色器
        shader_cube.setVec3("viewPos", camera.position);
        //rgb
        shader_cube.setVec3("lightColor", 0.5f, 1.0f, 0.3f);

        glBindVertexArray(VAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
       
        // glDrawElements(GL_TRIANGLES,36, GL_UNSIGNED_INT, 0);

        // 使用正八面体顶点数 24
        glDrawArrays(GL_TRIANGLES, 0, 24);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
