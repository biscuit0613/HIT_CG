# 光线追踪渲染器 (Ray Tracer)

本项目是一个基于 C++ 实现的光线追踪渲染器，主要是**路径追踪 (Path Tracing)**、尝试实现了**光子映射 (Photon Mapping, PM)** 和 **渐进式光子映射 (Progressive Photon Mapping, PPM)**。

## 1. 项目结构

```plaintext
├── CMakeLists.txt          
├── read_ppm.py             # 用于查看 .ppm 格式图片的 Python 脚本
├── src/
│   └── main.cpp            # 程序入口，负责场景构建和渲染循环调度
├── include/
│   ├── camera.h            # 摄像机类
│   ├── hittable_obj.h      # 可求交物体基类 (HittableObj)
│   ├── hittable_list.hpp   # 物体列表 (HittableObjList)
│   ├── material.hpp        # 材质基类及具体实现(Lambertian, Metal, Dielectric, DiffuseLight)
│   ├── ray.h               # 光线类
│   ├── sphere.h            # 球体类
│   ├── renderer_path.h     # 路径追踪算法实现
│   ├── renderer_pm.h       # 光子映射算法实现
│   ├── renderer_ppm.h      # 渐进式光子映射算法实现
│   ├── renderer_common.h   # 渲染通用工具函数
│   ├── utils.h             # 通用数学工具和随机数生成
│   └── vec3.h              # 向量类
└── images/                 # 渲染结果输出目录
```

## 2. 核心类与架构

本项目采用了面向对象的设计，主要类之间的关系如下：

### 2.1 几何体 (Geometry)

* **`HittableObj`**: 所有可被光线击中的物体的抽象基类。定义了纯虚函数 `hit`。
  
* **`Sphere`**: 继承自 `HittableObj`，实现了球体的求交逻辑。
* **`HittableObjList`**: 继承自 `HittableObj`，内部维护一个 `std::vector<shared_ptr<HittableObj>>`，用于存储整个场景的物体。

### 2.2 材质 (Material)

* **`Material`**: 材质基类，定义了 `scatter` (散射) 和 `emitted` (自发光) 接口。

* **`Lambertian`**: 漫反射材质，光线随机散射。
* **`Metal`**: 金属材质，光线发生镜面反射，支持模糊 (Fuzz)。
* **`Dielectric`**: 绝缘体/玻璃材质，支持折射和反射 (菲涅尔效应)。
* **`DiffuseLight`**: 发光材质，不散射光线，只发射颜色。

### 2.3 核心数据结构

* **`Ray`**: 光线，由原点 `origin` 和方向 `direction` 组成。

* **`HitRecord`**: 记录光线与物体相交时的详细信息（交点位置 `p`、法线 `normal`、材质指针 `mat_ptr`、光线参数 `t` 等）。
* **`Camera`**: 负责根据视场角 (FOV) 和宽高比生成从视点出发的光线。
* KD-Tree: 用于加速光子映射中的光子查询。AI生成的。

## 3. 渲染流程详解

本项目实现了三种主要的全局光照算法：

### 3.1 路径追踪 (Path Tracing)

**文件**: `include/renderer_path.h`

路径追踪是一种基于蒙特卡洛积分的无偏渲染算法。

1. **光线生成**: 对每个像素发射多条光线 (Anti-aliasing)，每条光线在像素内随机采样。

2. **递归追踪 (`ray_color`)**:
    * 光线击中物体后，根据材质属性计算**自发光** (`Emitted`) 和**散射** (`Scatter`)。
    * 如果是光源，返回发光颜色。
    * 如果是普通物体，递归计算散射光线的颜色，并乘以衰减系数 (`Attenuation`)。
3. **俄罗斯轮盘赌**: 为了防止无限递归并保证无偏性，引入生存概率 `p`。光线有一定概率终止，若存活则通过 `1/p` 进行能量补偿。

### 3.2 光子映射 (Photon Mapping, PM)

**文件**: `include/renderer_pm.h`

KDtree的部分是AI写的

光子映射分为两个阶段(Pass)：

1. **光子发射 (Photon Pass)**:
    * 从光源发射大量光子。
    * 光子在场景中弹射，当击中**漫反射**表面时，将其位置、入射方向和能量存储在 **KD-Tree** (Photon Map) 中。
    * 区分 **Global Map** (间接光照) 和 **Caustic Map** (焦散，如玻璃球下的聚光)。
2. **渲染 (Render Pass)**:
    * 从相机发射光线。
    * **直接光照**: 通过阴影光线直接计算。
    * **镜面/折射**: 递归追踪。
    * **间接漫反射/焦散**: 在击中点附近搜索最近的 N 个光子，利用光子密度估算辐射度 (Radiance Estimation)。

### 3.3 渐进式光子映射 (Progressive Photon Mapping, PPM)
**文件**: `include/renderer_ppm.h`

PPM 解决了传统 PM 内存受限的问题，通过多轮迭代逐步收敛到精确解。

1. **视线追踪 (Eye Pass)**:
    * 从相机发射光线，记录所有与**漫反射**表面相交的点，保存为 **HitPoint**。
    * 每个 HitPoint 记录了位置、法线、累积权重 (`Throughput`) 和当前的搜索半径。
2. **光子发射 (Photon Pass)**:
    * 每一轮从光源发射一批新光子。
    * 当光子落在 HitPoint 的搜索半径内时，更新该 HitPoint 的光子计数和能量。
3. **半径缩减**:
    * 随着收集到的光子越来越多，逐步缩小 HitPoint 的搜索半径。

    * 最终结果是所有迭代的累积平均。

## 4. 编译与运行

### 编译

确保已安装 CMake 和支持 C++17 的编译器。

```bash
mkdir build
cd build
cmake ..
make
```

### 运行

编译完成后，可执行文件位于 `build` 目录中。

```bash
# 路径追踪 (默认)
./RayTracer -m pt -o output_pt.ppm -s 1000

# 光子映射
./RayTracer -m pm -o output_pm.ppm -s 1000

# 渐进式光子映射
./RayTracer -m ppm -o output_ppm.ppm -s 1000
```

**参数说明**:

* `-m, --mode`: 渲染模式 (`pt`, `pm`, `ppm`)。

* `-o, --out`: 输出文件名。
* `-s, --spp`: 单位是万，采样数 (PT) 或光子发射数 (PM/PPM)。（注意不是ppm一轮的数量）
* `-w, --width`: 图像宽度。

### 查看结果

输出图片为 PPM 格式，可以使用 `read_ppm.py` 转换为常见格式查看，或使用支持 PPM 的看图软件。

```bash
python3 read_ppm.py output.ppm
```
