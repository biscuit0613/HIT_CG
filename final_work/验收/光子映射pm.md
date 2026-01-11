## 光子映射 (Photon Mapping)

Pass 1 (光子发射): 从光源发射光子，让它们在场景里乱跑，停在物体表面上。这些停下来的光子就代表了“光照信息”。

Pass 2 (渲染): 从相机发射视线，看到物体时，不再像路径追踪那样盲目地去寻找光源，而是直接问周围的光子这里有多亮

光子的结构体是`photon`，光子图的结构体是kdtree，因为我对数据结构掌握的不是很好，所以这一块让ai写了，尽量实现高度包装，只在`estimate_radiance`方法里面用了一下search函数。

### Pass1

目标是建立光子图，从光源随机发射大量光子。每个光子携带能量 (Flux/Power)。

关于光子的flux(或者叫power,因为俩单位是一样的)计算：flux= 光源发光强度L * 光源面积A * π / 发射光子数N

```cpp
Color L = std::dynamic_pointer_cast<DiffuseLight>(sphere->mat_ptr)->emit->value(0,0,origin);
double area = 4 * pi * sphere->radius * sphere->radius;
Color photon_power = L * area * pi / num_photons;
```

主要方法是`trace_photon_pm` 函数：当光子打在漫反射 (Diffuse) 表面时，我们把它记录下来，存入光子图 (KD-Tree)。光子图分为全局光子图 (Global Map) 和焦散光子图 (Caustic Map)。全局光子图包含焦散光子图，焦散光子图只包含焦散光子。

```cpp
inline void trace_photon_pm(Ray ray, int dep, Color power, std::vector<Photon>& global_photons, 
    std::vector<Photon>& caustic_photons, const HittableObjList& world, bool in_caustic_path) {
    if (max_in_xyz(power) < 1e-9) return;// 如果辐射通量的最大分量小于1e-9,说明该光子已经被材质所吸收，直接返回
    HitRecord rec;
    if (!world.hit(ray, 0.001, infinity, rec)) return;//如果射到世界world外面了，也返回
    // 1. 判断是否需要存储光子
    // 如果是漫反射表面 (且不是光源)，则存储光子
    std::pair<Refl_t, Color> feature = get_feature(rec.mat_ptr, rec.p);
    bool is_diffuse_light = (std::dynamic_pointer_cast<DiffuseLight>(rec.mat_ptr) != nullptr);
    if (feature.first == DIFF && !is_diffuse_light) {
        if (in_caustic_path) {
            // 路径: L ...S D caustic 存入 Caustic Map
            #pragma omp critical
            caustic_photons.push_back({rec.p, ray.direction(), power});
        } 
        // 路径: L ...D D 存入 Global Map
        #pragma omp critical
        global_photons.push_back({rec.p, ray.direction(), power});
    }
    // 2. 使用材质的 scatter 函数决定光子的下一次反弹
    Ray scattered;
    Color attenuation;
    if (rec.mat_ptr->scatter(ray, rec, attenuation, scattered)) {
        Color new_power = power * attenuation;
        
        // 俄罗斯轮盘赌 (Russian Roulette)
        double p_survive = max_in_xyz(attenuation);
        if (p_survive > 1.0) p_survive = 1.0;
        
        if (++dep > 5) {
            if (random_double() < p_survive) {
                new_power = new_power / p_survive;
            } else {
                return;
            }
        }
        // 如果撞击漫反射表面，后续路径不再属于 "Caustic Path"
        // 如果撞击镜面/折射表面，保持 in_caustic_path 状态
        bool next_in_path = in_caustic_path;
        if (feature.first == DIFF) {
            next_in_path = false;
        }
        trace_photon_pm(scattered, dep, new_power, global_photons, caustic_photons, world, next_in_path);
    }
}
```

### Pass2

目标是渲染图像，得到颜色向量。对于每个像素，发射多条光线 ：

当视线打中物体的$p$点时，渲染方程

$$
L_o(p, \omega_o) = L_e(p, \omega_o) + \int_{\Omega} f_r(p, \omega_i, \omega_o) L_i(p, \omega_i) (\omega_i \cdot n) d\omega_i
$$

根据报告，brdf$f_r$拆成两份，入射辐射度$L_i$可以拆成两部分：直接光照$L_{i,dir}$和间接光照$L_{i,ind}$，间接光照又分为全局光子映射$L_{i,global}$和焦散光子映射$L_{i,caustic}$：

对于直接光照部分，连接点$p$和光源采样点$s$，如果可见，则计算：

```cpp
if (dot(nl, light_dir) > 0) { // 面向光源
    Ray shadow_ray(x, light_dir);
    HitRecord shadow_rec;
    // 检查可见性 发射shadow Ray，如果能见光源就继续
    if (!world.hit(shadow_ray, 0.001, dist - 0.001, shadow_rec)) {
        // 可见
        if (auto diff_light = std::dynamic_pointer_cast<DiffuseLight>(sphere->mat_ptr)) {
            Color Le = diff_light->emit->value(0,0, point_on_light);
            double cos_theta = dot(nl, light_dir);
            Vec3 light_normal = unit_vector(point_on_light - sphere->center);
            double cos_theta_light = dot(-light_dir, light_normal);  
            if (cos_theta_light > 0) {
                double area = 4 * pi * sphere->radius * sphere->radius;
                // Lo = Le * f_r * cos_theta * (cos_theta_light * Area / dist^2)
                // f_r = albedo / pi
                direct += Le * f * (1.0/pi) * cos_theta * cos_theta_light * area / dist_sq;
            }
        }
    }
}
```

对于间接光照的焦散部分，直接查焦散光子图，即调用`estimate_radiance`函数：传入焦散光子图`caustic_map`，位置`x`，法线`nl`，搜索半径`caustic_radius`：

```cpp
Color caustics = estimate_radiance(caustic_map, x, nl, caustic_radius);
```

对于间接光照的全局部分，也就是漫反射部分，并不是直接查，而是用final gathering的方法：

```cpp
//  间接漫反射不是直接查询光子图，而是使用 Final Gather,将当前点视为一个新的“摄像机”
// 向半球空间发射许多条主光线。当这些光线击中周围环境时，从主光变成了次级光线（gather_only从false变为true）,在那个击中点查询光子图，获取那里的辐射度，
// 然后将其作为入射光计算当前点的颜色。最后对所有次级光线的结果取平均。
//相当于做了一次单反弹的路径追踪
Color indirect(0,0,0);//需要计算的间接漫反射部分
int fg_samples = 512; // Final Gather的 采样数，越多越好但越慢
for (int i = 0; i < fg_samples; ++i) {
    // 半球余弦采样
    double r1 = 2 * pi * random_double();
    double r2 = random_double();
    double r2s = sqrt(r2);
    Vec3 u = unit_vector(cross((fabs(nl.x()) > .1 ? Vec3(0, 1, 0) : Vec3(1, 0, 0)), nl));
    Vec3 v = cross(nl, u);
    Vec3 d = unit_vector(u * cos(r1) * r2s + v * sin(r1) * r2s + nl * sqrt(1 - r2));
    // 发射主光线，设置 gather_only = true
    Color Li = eye_trace_estimate(Ray(x, d), dep + 1, max_depth, world, lights, global_map, 
    caustic_map, global_radius, caustic_radius, true);
    indirect += Li * f;
}
indirect = indirect / fg_samples;

return direct + caustics_reflected + indirect;
```

当 `gather_only=true` 时，递归函数会直接调用 estimate_radiance(global_map, ...) 返回结果，不再继续递归。（见函数开头）

### pass2中的密度估计

光子图里存的是一个个光子，存的是flux(或power)怎么变成颜色（辐射度 Radiance）

理论推导见报告，这里直接上公式

$$
L(x, \omega_o) \approx \frac{1}{\pi r^2} \sum_{i=1}^{N} f_r(x, \omega_i, \omega_o) \Delta \Phi_i
$$

其中，$N$ 是在半径 $r$ 内找到的光子数，$\Delta \Phi_i$ 是第 $i$ 个光子的通量 (Flux（或Power）)，$f_r$ 是材质的 BRDF。

```cpp
inline Color estimate_radiance(const KDTree<Photon>& map, const Point3& p, const Vec3& normal, double radius) {
    Color flux(0,0,0);
    int count = 0;
    map.search(p, radius, [&](Photon* photon, double dist_sq) {
        if (dot(normal, photon->dir) < 0) { // 法线检查
            flux += photon->power;
            count++;
        }
    });
    
    if (count == 0) return Color(0,0,0);
    // 辐射度 = Flux / Area
    return flux / (pi * radius * radius);
}
```

### 细节

在查焦散光子图的时候，半径乘了0.8，比全局光子图小一些，减少噪点：