#ifndef RENDERER_PM_H
#define RENDERER_PM_H

#include "utils.h"
#include "renderer_common.h"
#include "hittable_list.hpp"
#include "camera.h"
#include "material.hpp"
#include "sphere.h" 
#include <vector>
#include <cmath>
#include <iostream>
#include <omp.h>

// 光子结构体
struct Photon {
    Point3 p;       // 位置
    Vec3 dir;       // 入射方向
    Color power;    // 通量
};

// 估算辐射度：查找附近的 N 个光子
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

// pass1光子追踪：发射光子并存储在 photon map 中
// 标志in_caustic_path: 标记当前光子是否处于从光源出发的折射/反射路径中 (L S ...)
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

// pass2光线追踪：使用光子图估算辐射度
// 标志gather_only: 如果为 true，表示当前是 Final Gather 的次级光线，击中漫反射表面时直接查询光子图
inline Color eye_trace_estimate(Ray ray, int dep, int max_depth, const HittableObjList& world, const std::vector<shared_ptr<HittableObj>>& lights, const KDTree<Photon>& global_map, const KDTree<Photon>& caustic_map, double global_radius, double caustic_radius, bool gather_only = false) {
    HitRecord rec;
    if (!world.hit(ray, 0.001, infinity, rec)) return Color(0,0,0); // 背景色
    //photon 信息：
    Point3 x = rec.p;//photon的位置=光线撞到的点
    Vec3 n = rec.normal;//photon撞到的表面的法线=光线撞到的表面的法线
    Vec3 nl = dot(n, ray.direction()) < 0 ? n : -n;//调整法线方向，使其指向入射光线的一侧
    
    //用了一个pair存储材质1. 类型和2. 颜色
    std::pair<Refl_t, Color> feature = get_feature(rec.mat_ptr, x);
    Color f = feature.second;
    
    if (feature.first == DIFF) {
        if (gather_only) {// 如果是次级射线击中漫反射表面，直接查询 Global Map 估计辐射度
            Color emitted(0,0,0);
            if (auto diff_light = std::dynamic_pointer_cast<DiffuseLight>(rec.mat_ptr)) {
                emitted = diff_light->emit->value(0,0, x);
            }
            // 查询全局光子图
            Color irradiance = estimate_radiance(global_map, x, nl, global_radius);
            return emitted + f * irradiance * (1.0/pi);
        }
        //直接光照，交点和光源相连，检查可见性，实现阴影
        Color direct(0,0,0);
        for (const auto& light : lights) {
            // 假设光源是球体 (Sphere)
            if (auto sphere = std::dynamic_pointer_cast<Sphere>(light)) {
                // 在光源上采样一点
                Vec3 point_on_light = sphere->center + random_unit_vector() * sphere->radius;
                Vec3 to_light = point_on_light - x;
                double dist_sq = to_light.length_squared();
                double dist = sqrt(dist_sq);
                Vec3 light_dir = to_light / dist;
                if (dot(nl, light_dir) > 0) { // 面向光源
                    Ray shadow_ray(x, light_dir);
                    HitRecord shadow_rec;
                    // 检查可见性 (Shadow Ray)
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
            }
        }
        // 焦散使用 Caustic Map 估算
        Color caustics = estimate_radiance(caustic_map, x, nl, caustic_radius);
        Color caustics_reflected = f * caustics * (1.0/pi);
        //  间接漫反射不是直接查询光子图，而是使用 Final Gather,将当前点视为一个新的“摄像机”
        // 向半球空间发射许多条主光线。当这些光线击中周围环境时，从主光变成了次级光线,在那个击中点查询光子图，获取那里的辐射度，
        // 然后将其作为入射光计算当前点的颜色。最后对所有次级光线的结果取平均。
        //相当于做了一次单反弹的路径追踪
        Color indirect(0,0,0);
        int fg_samples = 512; // Final Gather 采样数，越多越好但越慢
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

    } else if (feature.first == SPEC) {
        if (dep > max_depth) return Color(0,0,0);//超过最大递归深度就返回黑色
        Ray reflray = Ray(x, reflect(ray.direction(), n));
        // 镜面反射继续递归，保持 gather_only 状态
        return f * eye_trace_estimate(reflray, dep + 1, max_depth, world, lights, global_map, caustic_map, global_radius, caustic_radius, gather_only);
    } else if (feature.first == REFR) {
        if (dep > max_depth) return Color(0,0,0);//超过最大递归深度就返回黑色   
        double refraction_ratio = dot(n, ray.direction()) < 0 ? (1.0/1.5) : 1.5;
        Vec3 unit_dir = unit_vector(ray.direction());
        double cos_theta = fmin(dot(-unit_dir, nl), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta*cos_theta);
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        
        Vec3 d_refracted;
        if (!cannot_refract) d_refracted = refract(unit_dir, nl, refraction_ratio);
        
        if (cannot_refract) {
            return f * eye_trace_estimate(Ray(x, reflect(unit_dir, nl)), dep + 1, max_depth, world, lights, global_map, caustic_map, global_radius, caustic_radius, gather_only);
        } else {
            auto r0 = (1-1.5)/(1+1.5); r0 = r0*r0;
            double Re = r0 + (1-r0)*pow((1 - cos_theta), 5);
            double Tr = 1 - Re;
            double P = .25 + .5 * Re;
            
            if (dep < 3) {
                Color reflection = eye_trace_estimate(Ray(x, reflect(unit_dir, nl)), dep + 1, max_depth, world, lights, global_map, caustic_map, global_radius, caustic_radius, gather_only);
                Color refraction = eye_trace_estimate(Ray(x, d_refracted), dep + 1, max_depth, world, lights, global_map, caustic_map, global_radius, caustic_radius, gather_only);
                return f * (Re * reflection + Tr * refraction);
            } else {
                if (random_double() < P) {
                    return f * (Re/P) * eye_trace_estimate(Ray(x, reflect(unit_dir, nl)), dep + 1, max_depth, world, lights, global_map, caustic_map, global_radius, caustic_radius, gather_only);
                } else {
                    return f * (Tr/(1-P)) * eye_trace_estimate(Ray(x, d_refracted), dep + 1, max_depth, world, lights, global_map, caustic_map, global_radius, caustic_radius, gather_only);
                }
            }
        }
    }
    return Color(0,0,0);
}

// PM 主渲染函数
inline void render_pm(
    const HittableObjList& world, 
    const std::vector<shared_ptr<HittableObj>>& lights,
    const Camera& cam, 
    int image_width, 
    int image_height, 
    int num_photons, 
    int max_depth,
    double radius,
    std::vector<unsigned char>& buffer
) {
    std::cout << "pm渲染中" << std::endl;
    std::cout << "光子总数: " << num_photons << ", 查询半径: " << radius << std::endl;

    // 1. Photon Pass
    std::cout << "Pass1:光子图的构建..." << std::endl;
    std::vector<Photon> global_photons;
    std::vector<Photon> caustic_photons;
    global_photons.reserve(num_photons);
    caustic_photons.reserve(num_photons / 4); // 预估焦散光子较少
    
    #pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < num_photons; ++i) {
        if (lights.empty()) continue;
        int light_idx = static_cast<int>(random_double(0, lights.size()-0.01));
        auto light = lights[light_idx];
        if (auto sphere = std::dynamic_pointer_cast<Sphere>(light)) {
            Point3 origin = sphere->center + random_unit_vector() * sphere->radius;
            Vec3 dir = random_unit_vector();
            if (dot(dir, origin - sphere->center) < 0) dir = -dir;
            
            Color L = std::dynamic_pointer_cast<DiffuseLight>(sphere->mat_ptr)->emit->value(0,0,origin);
            double area = 4 * pi * sphere->radius * sphere->radius;
            Color photon_power = L * area * pi / num_photons;
            
            // 初始 in_caustic_path = true，因为从光源出来
            trace_photon_pm(Ray(origin, dir), 0, photon_power, global_photons, caustic_photons, world, true);
        }
    }
    std::cout << "全局光照的光子数量: " << global_photons.size() << std::endl;
    std::cout << "焦散的光子数量: " << caustic_photons.size() << std::endl;

    // 2. 构建光子图
    std::cout << "构建光子图中" << std::endl;
    // Global Map 
    double global_radius = radius; 
    KDTree<Photon> global_map(global_photons);

    // Caustic Map, cell_size可以小一点，让焦散图更精细
    double caustic_radius = radius*0.8;
    KDTree<Photon> caustic_map(caustic_photons);

    //Pass2
    std::cout << "pass2: 渲染图像中" << std::endl;
    std::vector<Color> final_image(image_width * image_height);
    
    // #pragma omp parallel for schedule(dynamic, 1)
    for (int j = image_height-1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            auto u = (i + random_double()) / (image_width-1);
            auto v = (j + random_double()) / (image_height-1);
            Ray r = cam.get_ray(u, v);
            
            Color pixel_color = eye_trace_estimate(r, 0, max_depth, world, lights, global_map, caustic_map, global_radius, caustic_radius);
            final_image[(image_height - 1 - j) * image_width + i] = pixel_color;
        }
    }

    buffer.assign(image_width * image_height * 3, 0);
    for (int i = 0; i < image_width * image_height; ++i) {
        Color c = final_image[i];
        c = aces_approx(c);
        auto r = sqrt(c.x());
        auto g = sqrt(c.y());
        auto b = sqrt(c.z());
        buffer[i*3] = static_cast<unsigned char>(256 * clamp(r, 0.0, 0.999));
        buffer[i*3+1] = static_cast<unsigned char>(256 * clamp(g, 0.0, 0.999));
        buffer[i*3+2] = static_cast<unsigned char>(256 * clamp(b, 0.0, 0.999));
    }
    std::cout << "完成" << std::endl;
}

#endif
