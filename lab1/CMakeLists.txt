cmake_minimum_required(VERSION 3.10)

add_executable(lab1 main.cpp shader.cpp)

target_link_libraries(lab1
    OpenGL::GL
    GLEW::GLEW
    glfw
    glm::glm
)