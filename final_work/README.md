# Ray Tracer Project

这是一个基于 C++ 的光线追踪渲染器框架，实现了递归光线追踪、菲涅尔反射/折射以及透明材质（水晶球）的模拟。

## 项目结构

- `src/main.cpp`: 主程序入口，包含渲染循环和场景设置。
- `include/`: 头文件目录
    - `vec3.h`: 向量数学库
    - `ray.h`: 光线类
    - `hittable.h`, `hittable_list.h`: 物体接口与列表
    - `sphere.h`: 球体实现
    - `material.h`: 材质系统 (Lambertian, Metal, Dielectric)
    - `camera.h`: 摄像机类
    - `utils.h`: 通用工具函数

## 编译与运行

确保安装了 CMake 和 C++ 编译器。

```bash
mkdir build
cd build
cmake ..
make
./RayTracer
```

运行结束后会生成 `image.ppm` 文件，可以使用支持 PPM 格式的图片查看器打开，或者使用在线工具转换。

## 功能实现

1.  **递归光线追踪**: 在 `ray_color` 函数中实现。
2.  **折射与反射**: `Dielectric` 材质实现了菲涅尔方程近似 (Schlick's approximation) 和全内反射判断。
3.  **场景**: 包含地面、漫反射球、金属球和玻璃球（水晶球）。

## 拓展指南

### 拓展 1: 光子映射 (Photon Mapping)
- 需要在 `main.cpp` 渲染循环前增加光子发射阶段。
- 创建 `Photon` 结构体和 `KDTree` 类来存储光子。
- 在 `ray_color` 中，对于漫反射表面，增加从光子图中查询光照的逻辑来模拟焦散 (Caustics)。

### 拓展 2: 贝塞尔曲面 (Bezier Surface)
- 创建新的类 `BezierSurface` 继承自 `Hittable`。
- 实现旋转贝塞尔曲面的求交算法（通常涉及数值解法，如牛顿迭代法）。
- 在 `main` 函数中将该物体加入场景。
