cmake_minimum_required(VERSION 3.10)

cmake_minimum_required(VERSION 3.10)
project(HITCGLAB4)

set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(lab3 main.cpp src/Shader.cpp src/Camera.cpp)

target_link_libraries(lab3
    OpenGL::GL
    GLEW::GLEW
    glfw
    glm
)

target_include_directories(lab3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

