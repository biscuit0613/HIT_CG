cmake_minimum_required(VERSION 3.10)

add_executable(lab2 learnopengl.cpp shader.cpp)

target_link_libraries(lab2
    OpenGL::GL
    GLEW::GLEW
    glfw
    glm::glm
)

target_link_directories(lab2 PRIVATE ${CMAKE_SOURCE_DIR})
