### 创建物体：

流程图：

![创建物体流程图.png](创建物体.png)

1. 纹理 (Texture) —— 最底层

    纹理决定了物体表面的“图案”或“颜色来源”。

    SolidColor: 单一颜色（如红色、灰色）。

    ImageTexture: 图片纹理（如maodie的图片）。

2. 材质 (Material) —— 包含纹理

    材质决定了光线如何与物体交互（漫反射、镜面反射、发光等）。材质通常包含一个纹理对象。

    * Lambertian (漫反射): 需要一个 Texture 来决定反照率 (Albedo)。

    * DiffuseLight (光源): 也包含一个 Texture，决定发光的颜色。
  
3. 物体 (HittableObj) —— 包含材质

    物体定义了形状（如球体），每个物体都持有一个材质的指针。

    Sphere: 构造时需要传入一个 Material 指针。
    ![code](创建物体code.png)

4. 世界 (HittableObjList) —— 包含物体

    世界是一个容器，包含了很多物体。

    HittableObjList: 通过 add 方法添加 HittableObj 指针。

## path tracing的过程

### 渲染方程

$$
L_o(p, \omega_o) = L_e(p, \omega_o) + \int_{\Omega} f_r(p, \omega_i, \omega_o) L_i(p, \omega_i) (\omega_i \cdot n) d\omega_i
$$

其中：

* $L_o(p, \omega_o)$：点 $p$ 在方向 $\omega_o$ 上的出射辐射度。

* $L_e(p, \omega_o)$：点 $p$ 在方向 $\omega_o$ 上的自发光辐射度（如果物体是光源）。

* $f_r(p, \omega_i, \omega_o)$：点 $p$ 处的双向反射分布函数 (BRDF)，描述了入射光 $\omega_i$ 如何被反射到出射方向 $\omega_o$。
* $L_i(p, \omega_i)$：点 $p$ 在方向 $\omega_i$ 上的入射辐射度。
* $(\omega_i \cdot n)$：入射方向与表面法线的夹角余弦，表示光线与表面的关系。

### 实现近似：`include/renderer_path.`里的`ray_color`函数

```cpp
// 递归光线追踪函数
inline Color ray_color(const Ray& r, const HittableObj& world, int depth) {
    HitRecord rec;//光线与物体的交点信息
    // 0.001 是为了忽略非常接近零的撞击
    if (world.hit(r, 0.001, infinity, rec)) {//world.hit返回true说明光线击中了物体
        Ray scatteredRay;//与材质交互后的光线
        Color attenuation;//albedo,颜色衰减
        Color emitted = rec.mat_ptr->emitted(0, 0, rec.p);//(忽略这里的uv坐标)获取材质发光颜色

        // 递归步骤：光线与材质交互并累积颜色
        if (rec.mat_ptr->scatter(r, rec, attenuation, scatteredRay)) {//scatter返回true说明有交互
            // 递归达到一定次数，轮盘赌决定是否终止路径
            if (depth < 45) {
                double p = 0.8; // 存活概率
                if (random_double() > p)
                    return emitted; // 终止路径
                attenuation = attenuation / p; // 能量补偿
            }
            // 继续递归追踪和材质交互的光线
            return emitted + attenuation * ray_color(scatteredRay, world, depth-1) ;
        }
        // 增加环境光，调试的时候用，以防光源太暗看不清场景了
        Color ambient(0.1, 0.1, 0.1);
        return emitted+ attenuation * ambient;
    }
    // 环境光，同上
    Vec3 unit_direction = unit_vector(r.direction());
    auto t = 0.5*(unit_direction.y() + 1.0);
    return (1.0-t)*Color(1.0, 1.0, 1.0) + t*Color(0.5, 0.7, 1.0);//只要color返回不是纯黑就不至于特别暗
}
```

解释：

1. 光线与物体交互检测：

    ```cpp
    if (world.hit(r, 0.001, infinity, rec)) { ... }
    ```

   * 使用 `world.hit` 返回bool,检测光线是否与场景中的物体相交。如果相交，获取交点信息 `rec`。其中`rec.p`是交点位置，`rec.mat_ptr`是交点处的材质指针，`rec.normal`是交点处的法线。
   * 如果没有相交，返回环境光颜色。

2. 计算自发光项：

    ```cpp
    Color emitted = rec.mat_ptr->emitted(0, 0, rec.p);
    ```

   * 调用材质的 `emitted` 方法获取交点处的自发光颜色。获取向量`emitted`。对应渲染方程中的 $L_e(p, \omega_o)$。
   * emitted方法在材质类`material`中定义，不同材质实现不同。光源材质（如`DiffuseLight`）会返回非零的发光颜色，其他材质返回黑色。
3. 计算散射光线方向和颜色衰减：

    ```cpp
    if (rec.mat_ptr->scatter(r, rec, attenuation, scatteredRay)) { ... }
    ```

   * 调用材质类的 `scatter` 方法，计算光线与材质交互后的**新光线**`scatteredRay`。不同材质的`scatter`实现不同，决定了光线如何被反射或折射。对应渲染方程中的入射光的**方向** $\omega_i$。

   * 颜色衰减 `attenuation`或者叫漫反射色，表示对应分量在scatter之后保留的比例。对应渲染方程中的 $f_r(p, \omega_i, \omega_o)$ 和 $(\omega_i \cdot n)$。
   * 这里需要注意，蒙特卡洛积分中  

        $f_r(p, \omega_i, \omega_o) (\omega_i \cdot n) d\omega_i=\frac{f_r\cdot cos\theta}{pdf}$  

        对于`Lambertian`代码，采样方式符合余弦分布，即 $pdf\propto cos\theta$。
   * 如果材质没有散射（如完全吸收的材质），则直接返回自发光颜色。
4. 递归追踪散射光线：

    ```cpp
    return emitted + attenuation * ray_color(scatteredRay, world, depth-1);
    ```

    * 这一步才开始计算入射光的辐射度 $L_i(p, \omega_i)$。通过递归调用 `ray_color` 函数，追踪散射光线 `scatteredRay`，并减少递归深度 `depth-1`。
    * 将自发光颜色 `emitted` 与衰减后的递归结果相加，得到最终的出射辐射度 $L_o(p, \omega_o)$。
5. 终止条件和路径轮盘赌：

    ```cpp
    if (depth < 45) {
        double p = 0.8; // 存活概率
        if (random_double() > p)
            return emitted; // 终止路径
        attenuation = attenuation / p; // 能量补偿
    }
    ```

   * 为了防止递归过深导致性能问题，设置了最大递归深度。当深度小于45时，使用路径轮盘赌方法随机终止路径。
   * 如果路径被终止，直接返回自发光颜色 `emitted`。
   * 如果继续追踪，为了保持能量守恒，需要对 `attenuation` 进行补偿，除以存活概率 `p`。

### 最终到像素颜色累积：

```cpp
//这里只截取了内层遍历宽度，外层还有高度的遍历
for (int i = 0; i < image_width; ++i) {
            Color pixel_color(0, 0, 0);
            for (int s = 0; s < samples_per_pixel; ++s) {
                auto u = (i + random_double()) / (image_width-1);
                auto v = (j + random_double()) / (image_height-1);
                Ray r = cam.get_ray(u, v);
                pixel_color += ray_color(r, world, max_depth);
            }
```

对于每一个像素点，都有一个向量`pixel_color`来累积该像素的颜色值。

通过多次（`samples_per_pixel`次）采样，每次在uv范围内进行微小抖动，生成一条光线 `r`，调用 `ray_color` 函数获取该光线的颜色贡献，并累加到 `pixel_color` 中。

最终，`pixel_color` 存储了该像素点的**总颜色值**，需要在后续步骤中进行平均和伽马校正，得到最终的显示颜色。

### 每个像素的颜色修正

```cpp
 auto scale = 1.0 / samples_per_pixel;
            Vec3 color = pixel_color * scale;//对每个pixel的采样平均
            color = aces_approx(color);

            auto r = sqrt(color.x());//gamma校正，严谨的是2.2，这里简化为2.0，开平方就行。
            auto g = sqrt(color.y());
            auto b = sqrt(color.z());

            int index = ((image_height - 1 - j) * image_width + i) * 3;
            buffer[index] = static_cast<unsigned char>(256 * clamp(r, 0.0, 0.999));
            buffer[index+1] = static_cast<unsigned char>(256 * clamp(g, 0.0, 0.999));
            buffer[index+2] = static_cast<unsigned char>(256 * clamp(b, 0.0, 0.999));
```

关于为什么要进行这些修正：

1. 采样平均（`scale`）：

   * 由于每个像素通过多次采样累积颜色值，因此需要除以采样次数 `samples_per_pixel` 来计算平均颜色。这确保了最终颜色是所有采样的平均值，而不是总和。

2. ACES近似（`aces_approx`）和gamma校正：

    具体原理参考实验报告，这里简单说明：

    ACES:高动态范围成像系统，能够更好地处理亮度范围广的场景，防止过曝和细节丢失。

    Gamma校正: 通过对颜色分量开平方（即gamma=2.0），调整颜色的亮度分布，使其更符合人眼的感知特性，避免图像过暗或过亮。
