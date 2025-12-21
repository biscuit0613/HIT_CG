#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

// 宽高比，保证在非正方形窗口下圆仍然是圆
uniform float w_div_h;
// 控制圆的半径（归一化坐标），边缘平滑宽度，以及颜色
uniform float radius; //  0.9
uniform float edge;   // 0.02
uniform vec3 innerColor; // 中心颜色
uniform vec3 outerColor; // 边缘颜色
// 计算给定距离 d 的径向渐变颜色（从 innerColor 到 outerColor）
vec3 radialGradient(float d) {
    float r = radius;
    if (r <= 0.0) r = 1.0; // 容错：若未设置 radius，则使用 1.0
    float t = clamp(d / r, 0.0, 1.0);

    vec3 inC = innerColor;
    vec3 outC = outerColor;

    if (length(inC) == 0.0 && length(outC) == 0.0) {
        inC = vec3(1.0, 1.0, 0.0); 
        outC = vec3(1.0, 0.0, 0.0); 
    }

    return mix(inC, outC, t);
}
// 原来的 step 函数现在返回 vec3（颜色），内部调用 radialGradient
vec3 step(float distance) {
    float e = edge;
    if (distance < radius - e/2) return vec3(0.0); // 圆内为黑色
    else if (distance > radius + e) return vec3(0.0); // 圆外为黑色
    else {
    float mask = 1.0 - smoothstep(radius - e, radius + e, distance);
    vec3 grad = radialGradient(distance);
    return grad * mask;
    }
    return vec3(0.0);
}
void main()
{
    vec2 uv = TexCoords * 2.0 - 1.0; // 把 [0,1] 映射到 [-1,1]
    uv.x *= w_div_h; // 宽高比调整
    float d = length(uv); // 当前片元到中心的距离
    vec3 color = step(d); // 调用 step 返回渐变颜色
    FragColor = vec4(color, 1.0f);
}