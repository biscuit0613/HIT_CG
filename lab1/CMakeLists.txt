cmake_minimum_required(VERSION 3.10)
project(CGLAB)

set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(demo main.cpp shader.cpp)

target_link_libraries(demo
    OpenGL::GL
    GLEW::GLEW
    glfw
    glm::glm
)