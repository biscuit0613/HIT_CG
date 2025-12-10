#version 460 core

// 顶点输入：
// aPos: 顶点位置（模型局部坐标）
// aTexCoords: 纹理坐标（UV）
// aNormal: 顶点法线（模型局部空间）
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec3 aNormal;

// 传给片元着色器的输出：
out vec2 TexCoords; // 纹理坐标
out vec3 Normal;    // 法线（当前示例中未做法线矩阵变换）
out vec3 FragPos;   // 片元位置（世界坐标）

// 变换矩阵：
uniform mat4 model;      // 模型矩阵（模型->世界）
uniform mat4 view;       // 视图矩阵（世界->视图）
uniform mat4 projection; // 投影矩阵（视图->裁剪空间）

void main()
{
    TexCoords = aTexCoords; // 传递纹理坐标给片元着色器
    // 注意：如果 model 矩阵包含非均匀缩放，应使用法线矩阵变换法线：
    // Normal = mat3(transpose(inverse(model))) * aNormal;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    // Normal = aNormal; 

    // 将局部坐标变换到世界坐标供 fragment 使用（例如光照计算）
    FragPos = vec3(model * vec4(aPos, 1.0));

    // 输出裁剪空间坐标（MVP 变换顺序：projection * view * model）
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}