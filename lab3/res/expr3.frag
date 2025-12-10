#version 460 core

out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

//  在 main.cpp 中：
//   shader_cube.setInt("texture1", 0);
//   shader_cube.setVec3("lightPos", 1.5f, 1.0f, 1.2f);
//   shader_cube.setVec3("viewPos", camera.position);
//   shader_cube.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
//   shader_cube.setMat4("model", glm::mat4(1.0f)); 
//   shader_cube.setMat4("view", camera.getViewMatrix());
//   shader_cube.setMat4("projection", ... );
//纹理在 main.cpp 中用 loadTexture 加载为 texture，并用 glActiveTexture(GL_TEXTURE0)
// 绑定到 GL_TEXTURE0，其后 shader_cube.setInt("texture1", 0) 将采样器绑定到单元 0
uniform sampler2D texture1; // 纹理采样器
uniform vec3 lightColor;    // 光的颜色
uniform vec3 lightPos;      // 光源位置（世界坐标）
uniform vec3 viewPos;       // 相机/观察者位置（世界坐标）
 
void main()
{
    //  镜面/漫反/环境光参数 
    float specularStrength = 0.5; // 控制高光强度
    float ambientStrength = 0.1;  // 环境光强度

    //  方向向量（都归一化） 
    // 视线方向：从片元指向观察者（相机）
    vec3 viewDir = normalize(viewPos - FragPos);

    // 法线向量：从顶点属性获取，再归一化
    vec3 norm = normalize(Normal);

    // 光照方向：从片元指向光源
    vec3 lightDir = normalize(lightPos - FragPos);

    // 反射向量：反射入射光线（用 -lightDir 表示从光源到片元的入射向量）
    // reflect(I, N) 计算向量 I 关于法线 N 的反射向量
    vec3 reflectDir = reflect(-lightDir, norm);

    //  镜面高光（Phong） 
    // dot(viewDir, reflectDir) 越接近 1，说明视线更接近反射方向 -> 高光越强
    // pow(..., 32) 控制高光聚集程度（高指数 -> 更小更亮的高光斑）
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor; // 镜面分量（通常不乘物体颜色）

    // 漫反射（Lambert）
    // 用法线和光向量的点乘来计算漫反射强度，取 0 到 1
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor; // 漫反射分量（与表面材料的颜色相乘）

    // 环境光（常量项）
    vec3 ambient = ambientStrength * lightColor;

    // 从纹理中采样得到表面颜色 (rgb)
    vec3 objColor = texture(texture1, TexCoords).rgb;

    // ambient 和 diffuse 与物体颜色相乘（表示被物体吸收/反射的光）
    // 而 specular 为高光（通常由光源决定，与物体颜色无关）
    vec3 result = (ambient + diffuse) * objColor + specular;

    FragColor = vec4(result, 1.0);
}