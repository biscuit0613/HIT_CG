#ifndef RENDERER_PPM_H
#define RENDERER_PPM_H

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

// PPM (Progressive Photon Mapping) 的击中点结构体
// 用于存储 Eye Pass 阶段视线与漫反射表面的交点信息
struct HitPoint {
    Point3 p;           // 击中点位置
    Vec3 normal;        // 击中点法线
    Color throughput;   // 路径权重 (Eye Path Throughput)，即从相机到该点的衰减
    int pixel_index;    // 对应的图像像素索引
    
    // PPM 统计数据
    double r2;          // 当前光子搜索半径的平方，这里用平方避免乘的那个系数开根号
    double n_new;       // 当前迭代收集到的光子数量
    Color flux_new;     // 当前迭代收集到的光子能量 (Flux)
    
    double n_accum;     // 累积收集的光子数量 (经过半径缩减修正)
    Color flux_accum;   // 累积收集的光子能量 (经过半径缩减修正)
    
    HitPoint(Point3 p_, Vec3 n_, Color tr_, int idx_, double r2_)
        : p(p_), normal(n_), throughput(tr_), pixel_index(idx_), r2(r2_),
          n_new(0), flux_new(0,0,0), n_accum(0), flux_accum(0,0,0) {}
};

// 第一步：Eye Pass (视线追踪)
// 从相机发射光线，记录与漫反射表面的交点 (HitPoint)
// 改进：增加 max_depth 参数防止无限递归；对玻璃材质使用分支追踪而非俄罗斯轮盘赌
inline void trace_eye_path(Ray ray, int dep, int max_depth, int pixel_index, const HittableObjList& world, Color throughput, std::vector<HitPoint>& hit_points, double initial_radius, std::vector<Color>& direct_buffer, int width) {
    if (dep > max_depth) return;
    if (max_in_xyz(throughput) < 1e-4) return;
    
    std::pair<int, double> intersect_result = nearest_hit(ray, world);
    if (intersect_result.first == -1) return;
    
    HittableObj* obj = world.objects[intersect_result.first].get();
    Point3 x = ray.at(intersect_result.second);
    
    HitRecord rec;
    obj->hit(ray, 0.001, infinity, rec);

    Vec3 n = rec.normal;
    Vec3 nl = dot(n, ray.direction()) < 0 ? n : -n;
    
    // 检查是否击中光源 (直接光照)
    if (auto light = std::dynamic_pointer_cast<DiffuseLight>(rec.mat_ptr)) {
        Color emitted = light->emit->value(0,0,x);
        // 直接将光源贡献写入 direct_buffer
        direct_buffer[pixel_index] += throughput * emitted;
        return; // 光源通常不进行漫反射散射
    }

    std::pair<Refl_t, Color> feature = get_feature(rec.mat_ptr, x);
    Color f = feature.second;
    
    if (feature.first == DIFF) {
        // 第一次直接撞上漫反射表面：记录 HitPoint，停止追踪
        #pragma omp critical
        hit_points.emplace_back(x, nl, throughput * f, pixel_index, initial_radius * initial_radius);
        return;
    } else if (feature.first == SPEC) {
        // 镜面反射：继续递归追踪
        Ray reflray = Ray(x, reflect(ray.direction(), n));
        trace_eye_path(reflray, dep + 1, max_depth, pixel_index, world, throughput * f, hit_points, initial_radius, direct_buffer, width);
    } else if (feature.first == REFR) {
        // 折射/介质：计算菲涅尔项
        double ir ; // 折射率
        Color transmission = Color(1,1,1); // 默认透射颜色
        
        // 获取材质的具体参数
        if (auto diel = std::dynamic_pointer_cast<Dielectric>(rec.mat_ptr)) {
            ir = diel->ir;
            // Beer's Law: 计算介质内部吸收
            if (dot(n, ray.direction()) > 0) { // 如果是从内部射出 (dot > 0)
                 // rec.t 是光线在介质内部传播的距离
                 double r = exp(-diel->absorbance.x() * rec.t);
                 double g = exp(-diel->absorbance.y() * rec.t);
                 double b = exp(-diel->absorbance.z() * rec.t);
                 transmission = Color(r, g, b);
            }
        }

        double refraction_ratio = dot(n, ray.direction()) < 0 ? (1.0/ir) : ir;
        Vec3 unit_dir = unit_vector(ray.direction());
        double cos_theta = fmin(dot(-unit_dir, nl), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta*cos_theta);
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        
        Vec3 d_refracted;
        if (!cannot_refract) d_refracted = refract(unit_dir, nl, refraction_ratio);
        
        // 应用透射衰减
        Color current_throughput = throughput * f * transmission;

        if (cannot_refract) {
            // 全反射
            trace_eye_path(Ray(x, reflect(unit_dir, nl)), dep + 1, max_depth, pixel_index, world, current_throughput, hit_points, initial_radius, direct_buffer, width);
        } else {
            auto r0 = (1-ir)/(1+ir); r0 = r0*r0;
            double Re = r0 + (1-r0)*pow((1 - cos_theta), 5);
            double Tr = 1 - Re;
            
            // 改进：分支追踪 (Branching)
            // 同时追踪反射和折射，按菲涅尔权重分配 throughput
            
            // 反射路径
            if (Re > 0.001) // 优化：权重太小就不追踪
                trace_eye_path(Ray(x, reflect(unit_dir, nl)), dep + 1, max_depth, pixel_index, world, current_throughput * Re, hit_points, initial_radius, direct_buffer, width);
            
            // 折射路径
            if (Tr > 0.001)
                trace_eye_path(Ray(x, d_refracted), dep + 1, max_depth, pixel_index, world, current_throughput * Tr, hit_points, initial_radius, direct_buffer, width);
        }
    }
}

// 第二步：Photon Pass (光子追踪)
// 从光源发射光子，当光子击中漫反射表面时，更新附近的 HitPoint
// 使用 Material里的scatter 进行重要性采样，统一光照传输逻辑
inline void trace_photon_ppm(Ray ray, int dep, Color power, KDTree<HitPoint>& tree, const HittableObjList& world, double max_dist_sq) {
    if (max_in_xyz(power) < 1e-8) return;
    
    std::pair<int, double> intersect_result = nearest_hit(ray, world);
    if (intersect_result.first == -1) return;
    
    HittableObj* obj = world.objects[intersect_result.first].get();
    Point3 x = ray.at(intersect_result.second);
    
    HitRecord rec;
    obj->hit(ray, 0.001, infinity, rec);
    
    // 是漫反射表面，存储光子
    if (std::dynamic_pointer_cast<DiffuseLight>(rec.mat_ptr) == nullptr) {
        std::pair<Refl_t, Color> feature = get_feature(rec.mat_ptr, x);
        if (feature.first == DIFF) {
            tree.search(x, sqrt(max_dist_sq), [&](HitPoint* hp, double dist_sq) {
                if (dist_sq <= hp->r2) {
                    if (dot(hp->normal, ray.direction()) < 0) {
                        #pragma omp atomic
                        hp->n_new += 1;
                        #pragma omp atomic
                        hp->flux_new.e[0] += power.e[0];
                        #pragma omp atomic
                        hp->flux_new.e[1] += power.e[1];
                        #pragma omp atomic
                        hp->flux_new.e[2] += power.e[2];
                    }
                }
            });
        }
    }

    // 使用材质的 scatter 函数决定光子的下一次反弹，材质里面按理说是不符合物理规律的，但如果严格按照物理规律来玻璃球的噪点非常多。
    Ray scattered;
    Color attenuation;
    if (rec.mat_ptr->scatter(ray, rec, attenuation, scattered)) {
        // 更新光子能量
        // attenuation 包含了 BRDF * Cosine / PDF
        Color new_power = power * attenuation;
        // 俄罗斯轮盘赌决定光子是否存活，使用衰减系数的最大分量作为存活概率
        double p_survive = max_in_xyz(attenuation);
        if (p_survive > 1.0) p_survive = 1.0; // 概率不能超过 1
        
        if (++dep > 5) {
            if (random_double() < p_survive) {// 存活：能量需要除以存活概率进行补偿
                new_power = new_power / p_survive;
            } else {// 死亡：停止追踪
                return;
            }
        }
        
        trace_photon_ppm(scattered, dep, new_power, tree, world, max_dist_sq);
    }
}

// PPM 主渲染函数
inline void render_ppm(
    const HittableObjList& world, 
    const std::vector<shared_ptr<HittableObj>>& lights,
    const Camera& cam, 
    int image_width, 
    int image_height, 
    int total_photon_num, // 总光子数
    int max_depth,
    double initial_radius,
    std::vector<unsigned char>& buffer
) {
    std::cout << "开始渐进式光子映射 (PPM)" << std::endl;
    
    // PPM 参数
    int iterations = 100; // 迭代次数
    int photons_per_iter = total_photon_num / iterations; // 每次迭代发射的光子数
    double alpha = 0.85; // 半径缩减参数
    
    std::cout << "ppm迭代次数： " << iterations << ", 每次迭代光子数: " << photons_per_iter << std::endl;

    // 1. 视线追踪阶段
    std::cout << "视线追踪阶段" << std::endl;
    std::vector<HitPoint> hit_points;
    std::vector<Color> direct_buffer(image_width * image_height, Color(0,0,0)); // 暂未使用
    
    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = image_height-1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            auto u = (i + random_double()) / (image_width-1);
            auto v = (j + random_double()) / (image_height-1);
            Ray r = cam.get_ray(u, v);
            
            int pixel_index = ((image_height - 1 - j) * image_width + i);
            trace_eye_path(r, 0, max_depth, pixel_index, world, Color(1,1,1), hit_points, initial_radius, direct_buffer, image_width);
        }
    }
    std::cout << "得到的可见点数： " << hit_points.size() << std::endl;
    
    // 构建 KD-Tree (只需构建一次)
    KDTree<HitPoint> tree(hit_points);

    // 2. 迭代阶段
    for (int iter = 0; iter < iterations; ++iter) {
        std::cout << "\r迭代第" << iter + 1 << "次 " << iter + 1 << "/" << iterations << std::flush;
        
        // 计算当前最大的搜索半径平方，用于 KD-Tree 剪枝
        double max_r2 = 0;
        for (const auto& hp : hit_points) {
            if (hp.r2 > max_r2) max_r2 = hp.r2;
        }

        // 光子追踪阶段
        #pragma omp parallel for schedule(dynamic, 1)
        for (int i = 0; i < photons_per_iter; ++i) {
            if (lights.empty()) continue;//可以试试多个光源，目前场景里面就一个。
            int light_idx = static_cast<int>(random_double(0, lights.size()-0.01));
            auto light = lights[light_idx];
            if (auto sphere = std::dynamic_pointer_cast<Sphere>(light)) {
                // 从光源表面随机发射光子
                Point3 origin = sphere->center + random_unit_vector() * sphere->radius;
                Vec3 dir = random_unit_vector();
                if (dot(dir, origin - sphere->center) < 0) dir = -dir;
                Color L = std::dynamic_pointer_cast<DiffuseLight>(sphere->mat_ptr)->emit->value(0,0,origin);
                double area = 4 * pi * sphere->radius * sphere->radius;
                Color photon_power = L * area * pi / photons_per_iter; // 单个光子的能量，需要除以每次迭代的光子数    
                trace_photon_ppm(Ray(origin, dir), 0, photon_power, tree, world, max_r2);
            }
        }
        // 更新 HitPoint 统计数据并缩减半径
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
    }
    
    // 3.重建最终图像
    std::vector<Color> final_image(image_width * image_height, Color(0,0,0));
    
    // 添加直接光照贡献 (来自 Eye Pass)
    for (int i = 0; i < image_width * image_height; ++i) {
        final_image[i] += direct_buffer[i];
    }

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
    
    // 写入缓冲区 
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
    std::cout << "渲染完成" << std::endl;
}

#endif
