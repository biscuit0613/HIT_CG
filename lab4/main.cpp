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


// unsigned int loadTexture(fs::path path) {
//     unsigned int textureID;
//     glGenTextures(1, &textureID);

//     int width, height, nrComponents;
//     unsigned char* data =
//         stbi_load(path.string().c_str(), &width, &height, &nrComponents, 0);
//     if (data) {
//         GLenum format;
//         if (nrComponents == 1)
//             format = GL_RED;
//         else if (nrComponents == 3)
//             format = GL_RGB;
//         else if (nrComponents == 4)
//             format = GL_RGBA;

//         glBindTexture(GL_TEXTURE_2D, textureID);
//         glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
//                      GL_UNSIGNED_BYTE, data);
//         glGenerateMipmap(GL_TEXTURE_2D);

//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
//                         GL_REPEAT);    // 设置纹理环绕方式
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
//                         GL_LINEAR_MIPMAP_LINEAR);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

//         stbi_image_free(data);
//     } else {
//         std::cout << "Texture failed to load at path: " << path << std::endl;
//         stbi_image_free(data);
//     }

//     return textureID;
// }

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
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);    
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glViewport(0, 0, width, height);

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GLEW version: " << glewGetString(GLEW_VERSION) << "\n";


    float quadVertices[] = {
        // positions        // texture Coords
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
        1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
    };

    glEnable(GL_DEPTH_TEST);

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)0);
 
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    Shader circle_shader("../res/exp4_circle.vert",
                          "../res/exp4_circle.frag");
    // unsigned int texture = loadTexture("../res/yellow.png");
    // unsigned int texture = loadTexture("../res/diamond.png");
    
    // 帧缓冲（FBO）设置
    // 1) 生成并绑定帧缓冲对象：
    //    生成一个 FBO（Framebuffer Object），后续附件（颜色纹理 / 深度模板 RBO）
    //    都会被附加到当前绑定的 FBO 上，之后渲染到 FBO 相当于渲染到这些附件。
    unsigned int frameBuffer;
    glGenFramebuffers(1, &frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

    // 2) 创建颜色附件：把渲染结果写入一个纹理（随后可以作为采样纹理使用）
    //    这里创建了一个与窗口宽高相同的空纹理（NULL），作为 FBO 的颜色缓冲。
    unsigned int texColorBuffer;
    glGenTextures(1, &texColorBuffer);
    glBindTexture(GL_TEXTURE_2D, texColorBuffer);
    // 分配纹理存储（不提供初始数据），格式为 RGB，尺寸为当前帧缓冲尺寸，其实就是一个空的2D纹理
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, NULL);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // 解绑纹理
    glBindTexture(GL_TEXTURE_2D, 0);

    // 将该纹理附加到当前绑定的 FBO 的颜色附件 0 上（GL_COLOR_ATTACHMENT0）
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texColorBuffer, 0);
    
    // 3) 创建并附加深度和模板渲染缓冲（RBO）
    //    RBO 用于保存深度/模板信息，通常比纹理更高效（但不能被采样）。
    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    // 为 RBO 分配存储（深度 24 + 模板 8）并设置与窗口一致的大小
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    // 将 RBO 附加为 FBO 的深度模板附件
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    // 4) 检查 FBO 是否完整（所有必须的附件是否正确附加）
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

    // 完成设置后解绑 FBO（以后渲染需要写入该 FBO 时再绑定）
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 注意：
    // - 如果窗口大小发生变化，需要重新调整或重新创建颜色纹理和 RBO 的尺寸（与 width/height 保持一致），否则渲染会拉伸或不完整。
    // - 渲染到 FBO 时，视口（glViewport）应与 FBO 大小一致（如果它们不同，需要调用 glViewport 进行调整）。
    // - 若需要从生成的颜色纹理中采样（在屏幕四边形上显示或后处理），记得在绘制时将纹理绑定到合适的纹理单元并设置 shader 的 sampler。

    Shader quad_shader("../res/expr4_quad.vert",
                         "../res/expr4_quad.frag");

    while (!glfwWindowShouldClose(window)) {

        processInput(window);
        glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
        glClearColor(0.1f, 0.1f, 0.1f, 0.1f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        
        circle_shader.use();
        circle_shader.setFloat("radius", 0.5f);
        circle_shader.setFloat("edge", 0.5f);
        circle_shader.setVec3("innerColor", glm::vec3(0.0f, 0.0f, 0.0f));
        circle_shader.setVec3("outerColor", glm::vec3(1.0f, 0.0f, 0.0f));
        circle_shader.setFloat("w_div_h", (float)width / (float)height);
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        quad_shader.use();
        glBindTexture(GL_TEXTURE_2D, texColorBuffer);
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
