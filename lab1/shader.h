//
//shader.h
//
#ifndef SHADER_H
#define SHADER_H
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <string>

class Shader {
public:
    unsigned int ID;
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    void use();

};

#endif