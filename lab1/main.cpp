#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "shader.h"

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
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

    // 初始化 GLEW（必须在上下文创建后）
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: "
                  << glewGetErrorString(err) << "\n";
        return -1;
    }

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GLEW version: " << glewGetString(GLEW_VERSION) << "\n";


    float vertices[] = {
        -0.5f, -0.5f, 0.0f, // 左下角
         0.5f, -0.5f, 0.0f, // 右下角
         0.0f,  0.5f, 0.0f  // 顶部
    };

    // 1. 创建并绑定 VAO (必须最先做，这样后续的 VBO 配置都会被记录到这个 VAO 中)
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // 2. 创建并绑定 VBO，传输数据
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);


    // 3. 设置顶点属性指针 (告诉 OpenGL 如何解析数据)（这一步会把指向 VBO 的信息记录到 VAO 里）
    // 第一个参数 0：属性索引（location），要和顶点着色器里的 layout(location = 0) 对应。
    // 3：每个顶点有 3 个分量 (x,y,z)。
    // GL_FLOAT：数据类型。
    // GL_FALSE：是否归一化。
    // 3 * sizeof(float)：stride（每个顶点占用的字节数）。
    // (void*)0：属性在数组中的偏移（这里顶点位置从第 0 个 float 开始）。 启用属性后，VAO 会记录这项配置
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 4. 解绑 VBO (可选，但 VAO 此时已记录了 VBO，所以解绑 VBO 不影响 VAO)
    // glBindBuffer(GL_ARRAY_BUFFER, 0); 

    // 5. 解绑 VAO (防止意外修改)
    glBindVertexArray(0);

    Shader shader_triangle("../exprl.vs", "../exprl.fs");
    shader_triangle.use();

    // 主循环
    while (!glfwWindowShouldClose(window)) {
        // 处理输入esc键
        processInput(window);
        // 设置清屏颜色（RGBA）
        glClearColor(0.2f, 0.2f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader_triangle.use();
        // 绘制三角形
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 交换缓冲区
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 清理资源
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}