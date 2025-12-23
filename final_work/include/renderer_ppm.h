#ifndef RENDERER_PPM_H
#define RENDERER_PPM_H

#include "utils.h"
#include "renderer_common.h"
#include "hittable_list.hpp"
#include "camera.h"
#include "material.hpp"
#include "sphere.h" 
#include <vector>
#include <list>
#include <cmath>
#include <iostream>
#include <omp.h>
#include <algorithm>

// PPM (Progressive Photon Mapping) 的击中点结构体
// 用于存储 Eye Pass 阶段视线与漫反射表面的交点信息
struct HitPoint {
    Point3 p;           // 击中点位置
    Vec3 normal;        // 击中点法线
    Color throughput;   // 路径权重 (Eye Path Throughput)，即从相机到该点的衰减
    int pixel_index;    // 对应的图像像素索引
    
    // PPM 统计数据
    double r2;          // 当前光子搜索半径的平方
    double n_new;       // 当前迭代收集到的光子数量
    Color flux_new;     // 当前迭代收集到的光子能量 (Flux)
    
    double n_accum;     // 累积收集的光子数量 (经过半径缩减修正)
    Color flux_accum;   // 累积收集的光子能量 (经过半径缩减修正)
    
    HitPoint(Point3 p_, Vec3 n_, Color tr_, int idx_, double r2_)
        : p(p_), normal(n_), throughput(tr_), pixel_index(idx_), r2(r2_),
          n_new(0), flux_new(0,0,0), n_accum(0), flux_accum(0,0,0) {}
};

// 哈希网格：用于加速光子对 HitPoint 的查找
class HashGrid {
public:
    HashGrid(double cell_size, int size) : cell_size(cell_size), size(size) {
        table.resize(size);
    }

    // 构建网格：将所有 HitPoint 放入对应的网格单元
    void build(std::vector<HitPoint*>& hit_points) {
        for (auto& list : table) list.clear();
        for (auto hp : hit_points) {
            int idx = hash(hp->p);
            table[idx].push_back(hp);
        }
    }

    // 更新 HitPoint：当光子击中漫反射表面时，查找附近的 HitPoint 并更新其统计数据
    void update(const Point3& p, const Vec3& dir, const Color& power) {
        int cx = static_cast<int>(std::floor(p.x() / cell_size));
        int cy = static_cast<int>(std::floor(p.y() / cell_size));
        int cz = static_cast<int>(std::floor(p.z() / cell_size));

        // 搜索 3x3x3 的邻域网格
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int idx = hash_coords(cx + dx, cy + dy, cz + dz);
                    for (auto hp : table[idx]) {
                        double dist_sq = (hp->p - p).length_squared();
                        // 检查光子是否在 HitPoint 的搜索半径内
                        if (dist_sq <= hp->r2) {
                            // 检查法线方向，避免漏光 (Light Leaking)
                            // 只有当光子入射方向与 HitPoint 法线相反（即从外部射入）时才统计
                            if (dot(hp->normal, dir) < 0) { 
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
                    }
                }
            }
        }
    }
    
    int hash(const Point3& p) const {
        int x = static_cast<int>(std::floor(p.x() / cell_size));
        int y = static_cast<int>(std::floor(p.y() / cell_size));
        int z = static_cast<int>(std::floor(p.z() / cell_size));
        return hash_coords(x, y, z);
    }

    int hash_coords(int x, int y, int z) const {
        unsigned int h = (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
        return h % size;
    }

    std::vector<std::list<HitPoint*>> table;
    double cell_size;
    int size;
};

// 第一步：Eye Pass (视线追踪)
// 从相机发射光线，记录与漫反射表面的交点 (HitPoint)
inline void trace_eye_path(Ray ray, int dep, int pixel_index, const HittableObjList& world, Color throughput, std::vector<HitPoint>& hit_points, double initial_radius, std::vector<Color>& direct_buffer, int width) {
    if (max_in_xyz(throughput) < 1e-4) return;
    
    std::pair<int, double> intersect_result = nearest_hit(ray, world);//返回最近的碰撞物体的索引和距离
    if (intersect_result.first == -1) return;//如果索引是-1，说明没有碰撞，直接返回
    
    HittableObj* obj = world.objects[intersect_result.first].get();//通过索引获取碰撞物体的指针
    Point3 x = ray.at(intersect_result.second);//碰撞点的位置。用于后面photon pass
    
    HitRecord rec;
    obj->hit(ray, 0.001, infinity, rec);//这时候才用hit函数获取碰撞信息hitrecord

    //下面获取碰撞点的法线和材质信息，和pm类似
    Vec3 n = rec.normal;
    Vec3 nl = dot(n, ray.direction()) < 0 ? n : -n;
    
    std::pair<Refl_t, Color> feature = get_feature(rec.mat_ptr, x);
    Color f = feature.second;
    
    if (feature.first == DIFF) {
        // 第一次直接撞上漫反射表面：记录 HitPoint，停止追踪
        // PPM 将负责计算该点的所有光照（直接+间接）
        #pragma omp critical
        hit_points.emplace_back(x, nl, throughput * f, pixel_index, initial_radius * initial_radius);
        return;
    } else if (feature.first == SPEC) {
        // 镜面反射：继续递归追踪
        Ray reflray = Ray(x, reflect(ray.direction(), n));
        trace_eye_path(reflray, dep + 1, pixel_index, world, throughput * f, hit_points, initial_radius, direct_buffer, width);
    } else if (feature.first == REFR) {
        // 折射/介质：计算菲涅尔项，决定反射或折射
        double refraction_ratio = dot(n, ray.direction()) < 0 ? (1.0/1.5) : 1.5;
        Vec3 unit_dir = unit_vector(ray.direction());
        double cos_theta = fmin(dot(-unit_dir, nl), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta*cos_theta);
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        
        Vec3 d_refracted;
        if (!cannot_refract) d_refracted = refract(unit_dir, nl, refraction_ratio);
        
        if (cannot_refract) {
            trace_eye_path(Ray(x, reflect(unit_dir, nl)), dep + 1, pixel_index, world, throughput * f, hit_points, initial_radius, direct_buffer, width);
        } else {
            auto r0 = (1-1.5)/(1+1.5); r0 = r0*r0;
            double Re = r0 + (1-r0)*pow((1 - cos_theta), 5);
            double Tr = 1 - Re;
            double P = .25 + .5 * Re;
            
            // 俄罗斯轮盘赌：随机选择反射或折射路径
            if (random_double() < P) {
                trace_eye_path(Ray(x, reflect(unit_dir, nl)), dep + 1, pixel_index, world, throughput * f * (Re/P), hit_points, initial_radius, direct_buffer, width);
            } else {
                trace_eye_path(Ray(x, d_refracted), dep + 1, pixel_index, world, throughput * f * (Tr/(1-P)), hit_points, initial_radius, direct_buffer, width);
            }
        }
    }
}

// 第二步：Photon Pass (光子追踪)
// 从光源发射光子，当光子击中漫反射表面时，更新附近的 HitPoint
inline void trace_photon_ppm(Ray ray, int dep, Color power, HashGrid& grid, const HittableObjList& world) {
    if (max_in_xyz(power) < 1e-8) return;
    
    std::pair<int, double> intersect_result = nearest_hit(ray, world);
    if (intersect_result.first == -1) return;
    
    HittableObj* obj = world.objects[intersect_result.first].get();
    Point3 x = ray.at(intersect_result.second);
    
    HitRecord rec;
    obj->hit(ray, 0.001, infinity, rec);
    Vec3 n = rec.normal;
    Vec3 nl = dot(n, ray.direction()) < 0 ? n : -n;
    
    std::pair<Refl_t, Color> feature = get_feature(rec.mat_ptr, x);
    Color f = feature.second;
    
    // 俄罗斯轮盘赌：决定光子是否存活
    double p_survive = max_in_xyz(f);
    if (++dep > 5) {
        if (random_double() < p_survive) f = f / p_survive;
        else return;
    }

    if (feature.first == DIFF) {
        // 漫反射表面：更新附近的 HitPoint
        grid.update(x, ray.direction(), power);
        
        // 漫反射散射：随机选择一个新的方向继续追踪光子
        double r1 = 2 * pi * random_double();
        double r2 = random_double();
        double r2s = sqrt(r2);
        
        Vec3 w = nl;
        Vec3 u = unit_vector(cross((fabs(w.x()) > .1 ? Vec3(0, 1, 0) : Vec3(1, 0, 0)), w));
        Vec3 v = cross(w, u);
        Vec3 d = unit_vector(u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1 - r2));
        
        trace_photon_ppm(Ray(x, d), dep, power * f, grid, world);
        
    } else if (feature.first == SPEC) {
        // 镜面反射
        Ray reflray = Ray(x, reflect(ray.direction(), n));
        trace_photon_ppm(reflray, dep, power * f, grid, world);
    } else if (feature.first == REFR) {
        // 折射
        double refraction_ratio = dot(n, ray.direction()) < 0 ? (1.0/1.5) : 1.5;
        Vec3 unit_dir = unit_vector(ray.direction());
        double cos_theta = fmin(dot(-unit_dir, nl), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta*cos_theta);
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        
        Vec3 d_refracted;
        if (!cannot_refract) d_refracted = refract(unit_dir, nl, refraction_ratio);
        
        if (cannot_refract) {
            trace_photon_ppm(Ray(x, reflect(unit_dir, nl)), dep, power * f, grid, world);
        } else {
            auto r0 = (1-1.5)/(1+1.5); r0 = r0*r0;
            double Re = r0 + (1-r0)*pow((1 - cos_theta), 5);
            double Tr = 1 - Re;
            double P = .25 + .5 * Re;
            
            if (random_double() < P) {
                trace_photon_ppm(Ray(x, reflect(unit_dir, nl)), dep, power * f * (Re/P), grid, world);
            } else {
                trace_photon_ppm(Ray(x, d_refracted), dep, power * f * (Tr/(1-P)), grid, world);
            }
        }
    }
}

// PPM 主渲染函数
inline void render_ppm(
    const HittableObjList& world, 
    const std::vector<shared_ptr<HittableObj>>& lights,
    const Camera& cam, 
    int image_width, 
    int image_height, 
    int total_photons, // 总光子数
    int max_depth,
    double initial_radius,
    std::vector<unsigned char>& buffer
) {
    std::cout << "Starting Progressive Photon Mapping (PPM)..." << std::endl;
    
    // PPM 参数
    int iterations = 100; // 迭代次数
    int photons_per_iter = total_photons / iterations; // 每次迭代发射的光子数
    if (photons_per_iter < 1000) photons_per_iter = 1000;
    double alpha = 0.7; // 半径缩减参数 (0.7 是经验值)
    
    std::cout << "Iterations: " << iterations << ", Photons/Iter: " << photons_per_iter << std::endl;

    // 1. Eye Pass (视线追踪阶段)
    std::cout << "Eye Pass..." << std::endl;
    std::vector<HitPoint> hit_points;
    std::vector<Color> direct_buffer(image_width * image_height, Color(0,0,0)); // 暂未使用
    
    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = image_height-1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            auto u = (i + random_double()) / (image_width-1);
            auto v = (j + random_double()) / (image_height-1);
            Ray r = cam.get_ray(u, v);
            
            int pixel_index = ((image_height - 1 - j) * image_width + i);
            trace_eye_path(r, 0, pixel_index, world, Color(1,1,1), hit_points, initial_radius, direct_buffer, image_width);
        }
    }
    std::cout << "Hit Points: " << hit_points.size() << std::endl;
    
    // 2. Iterations (迭代阶段)
    for (int iter = 0; iter < iterations; ++iter) {
        std::cout << "\rIteration " << iter + 1 << "/" << iterations << std::flush;
        
        // 构建哈希网格，用于加速光子查找
        double current_radius = 0;
        if (!hit_points.empty()) current_radius = sqrt(hit_points[0].r2);
        HashGrid grid(current_radius * 2.0, hit_points.size() + 1000);
        
        std::vector<HitPoint*> hp_ptrs;
        for (auto& hp : hit_points) hp_ptrs.push_back(&hp);
        grid.build(hp_ptrs);
        
        // Trace Photons (光子追踪)
        #pragma omp parallel for schedule(dynamic, 1)
        for (int i = 0; i < photons_per_iter; ++i) {
            if (lights.empty()) continue;
            // 随机选择一个光源
            int light_idx = static_cast<int>(random_double(0, lights.size()-0.01));
            auto light = lights[light_idx];
            if (auto sphere = std::dynamic_pointer_cast<Sphere>(light)) {
                // 从光源表面随机发射光子
                Point3 origin = sphere->center + random_unit_vector() * sphere->radius;
                Vec3 dir = random_unit_vector();
                if (dot(dir, origin - sphere->center) < 0) dir = -dir;
                
                Color L = std::dynamic_pointer_cast<DiffuseLight>(sphere->mat_ptr)->emit->value(0,0,origin);
                double area = 4 * pi * sphere->radius * sphere->radius;
                Color photon_power = L * area * pi / photons_per_iter; // 单个光子的能量
                
                trace_photon_ppm(Ray(origin, dir), 0, photon_power, grid, world);
            }
        }
        
        // Update HitPoints (更新 HitPoint 统计数据并缩减半径)
        for (auto& hp : hit_points) {
            if (hp.n_new > 0) {
                double N = hp.n_accum;
                double M = hp.n_new;
                
                // 半径缩减公式 (Hachisuka et al. 2008)
                // R_{i+1}^2 = R_i^2 * (N + alpha * M) / (N + M)
                double ratio = (N + alpha * M) / (N + M);
                
                hp.r2 *= ratio;
                // 累积能量也需要按比例缩放，以保持密度估计的一致性
                hp.flux_accum = (hp.flux_accum + hp.flux_new) * ratio;
                hp.n_accum = N + alpha * M;
                
                // 重置当前迭代的统计
                hp.n_new = 0;
                hp.flux_new = Color(0,0,0);
            }
        }
    }
    std::cout << std::endl;
    
    // 3. Reconstruct Image (重建图像)
    std::vector<Color> final_image(image_width * image_height, Color(0,0,0));
    for (const auto& hp : hit_points) {
        if (hp.r2 > 1e-8) {
            // 辐射度估计公式
            // L = Flux / (Area * Total_Emitted_Photons)
            // 注意：这里的 flux_accum 已经包含了半径缩减的修正
            Color radiance = hp.flux_accum / (pi * hp.r2 * photons_per_iter);
            final_image[hp.pixel_index] += radiance * hp.throughput;
        }
    }
    
    // 写入缓冲区 (包含 ACES 色调映射和 Gamma 校正)
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
    std::cout << "Done." << std::endl;
}

#endif
