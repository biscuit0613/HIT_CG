## 光子映射progressive photon mapping

渐进式光子映射 (Progressive Photon Mapping, PPM) 是对传统光子映射 (PM) 的一次重大升级。传统的 PM 必须把所有光子都存进内存。想要非常细腻、无噪声的画面，可能需要几十亿个光子，内存直接爆炸。

ppm的pass比pm多，而且第一个pass是从摄像机发射光线，记录hitpoint，而不是从光源发射光子。

### Pass 1 (视线追踪 Eye Pass)

代码: trace_eye_path (在 render_ppm 中调用)

找出画面中所有需要计算光照的点。

从相机发射光线。如果遇到镜面/玻璃，就继续反射/折射（递归）。直到光线打在漫反射 (Diffuse) 表面上，停下来。在这个位置创建一个 HitPoint (击中点)。

hITpoint 结构体包含：

* 位置 `p`、
* 法线 `normal`。
* `throughput`: 输出颜色（比如光线经过红色玻璃打到这里就是红色）。
* `r2`: 搜索半径的平方，便于比较距离 (这是 PPM 的核心，每个点都有自己的半径)。
* `n_accum`, `flux_accum`: 累积的光子数和能量。

只需要第一次 pass 记录这些 hitpoint，后续 pass 都不需要再发射视线了。所以kdtree也是在这一步建立的。

### pass2,3,4,5,... (光子发射 Photon Pass)

这就是多次迭代的阶段了代码是`render_ppm` 中的 `for (int iter = 0; ...)` 循环

每一轮迭代做两件事：

1. 光子追踪 (Photon Pass):

    从光源发射一批新光子（总数/iter次数）。光子在场景里乱跑 (trace_photon_ppm)。当光子落在一个 HitPoint 的搜索半径 r2（这里平方了便于比较距离） 内时，不存储光子而是直接把光子的能量加到 `HitPoint` 的 `flux_new` 上，光子计数 `n_new` + 1。

2. 然后半径缩减 :

    关于公式推导见报告。

    ```cpp
    for (auto& hp : hit_points) {
        if (hp.n_new > 0) {
            double N = hp.n_accum;
            double M = hp.n_new;
            // 半径缩减公式
            // R_{i+1}^2 = R_i^2 * (N + alpha * M) / (N + M)
            double ratio = (N + alpha * M) / (N + M);
            
            hp.r2 *= ratio;
            // 累积能量也需要按比例缩放，以保持密度估计的一致性
            // 公式：tau_{i+1} = (tau_i + phi_i) * ratio
            hp.flux_accum = (hp.flux_accum + hp.flux_new) * ratio;
            hp.n_accum = N + alpha * M;
            
            // 重置当前迭代的统计
            hp.n_new = 0;
            hp.flux_new = Color(0,0,0);
        }
    }
    ```

迭代完之后重建图像

最后每个像素的颜色=所有落在该像素的 HitPoint 的累积能量 除以 (π * r^2 * 迭代次数)

```cpp
for (const auto& hp : hit_points) {
    if (hp.r2 > 1e-9) {
        // 辐射度估计公式
        // L = Flux / (Area * Total_Emitted_Photons)
        // 这里的 flux_accum 是多次迭代的累积值，相当于 sum(Flux_i)
        // 而 photon_power 已经除以了单次迭代的光子数 photons_per_iter
        // 所以除以迭代次数 iterations 来取平均
        // 另外，HitPoint 位于漫反射表面，其 BRDF = albedo / pi
        // hp.throughput 中只包含了 albedo，所以还需要除以 pi
        Color radiance = hp.flux_accum / (pi * hp.r2 * iterations);//可以试试换成 photons_per_iter * iterations，会过曝
        final_image[hp.pixel_index] += radiance * hp.throughput / pi;
    }
}
```
