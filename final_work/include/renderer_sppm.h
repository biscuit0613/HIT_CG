#ifndef RENDERER_SPPM_H
#define RENDERER_SPPM_H

#include "utils.h"
#include "hittable_list.hpp"
#include "camera.h"
#include "material.hpp"
#include "sphere.h" 
#include <vector>
#include <list>
#include <cmath>
#include <iostream>
#include <omp.h>

struct HitPoint {
    Point3 p;
    Vec3 normal;
    Vec3 wo; // 指向相机的方向
    shared_ptr<Material> mat;
    Color throughput;
    int pixel_index; // 图像缓冲区中的索引

    // SPPM 特定数据
    Color flux;
    Color accumulated_flux;
    double radius_squared;
    double n_photons;
    double accumulated_photon_count;
};

struct Photon {
    Point3 p;
    Vec3 dir;
    Color power;
};

// 简单的哈希网格，用于空间查询
class HashGrid {
public:
    HashGrid(double cell_size, int size) : cell_size(cell_size), size(size) {
        table.resize(size);
    }

    void build(const std::vector<HitPoint*>& hit_points) {
        for (auto& list : table) list.clear();
        for (auto hp : hit_points) {
            Point3 p = hp->p;
            // 目前只添加到中心点所在的单元
            int idx = hash(p);
            table[idx].push_back(hp);
        }
    }

    void find_nearby(const Point3& p, std::vector<HitPoint*>& result) {
        int idx = hash(p);
        // 检查当前单元
        for (auto hp : table[idx]) {
            result.push_back(hp);
        }
        // 在实际应该检查周围的 3x3x3 个邻居单元
    }
    
    int hash(const Point3& p) const {
        int x = static_cast<int>(p.x() / cell_size);
        int y = static_cast<int>(p.y() / cell_size);
        int z = static_cast<int>(p.z() / cell_size);
        // 简单的哈希函数
        unsigned int h = (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
        return h % size;
    }

    std::vector<std::list<HitPoint*>> table;
    double cell_size;
    int size;
};


inline void render_sppm(
    const HittableObjList& world, 
    const std::vector<shared_ptr<HittableObj>>& lights,
    const Camera& cam, 
    int image_width, 
    int image_height, 
    int iterations, 
    int photons_per_iter,
    int max_depth,
    double initial_radius,
    std::vector<unsigned char>& buffer
) {
    std::cout << "开始 SPPM 渲染..." << std::endl;
    
    // 1. 初始化击中点 (视线/眼睛 阶段)
    std::vector<HitPoint> hit_points;
    hit_points.reserve(image_width * image_height);
    
    std::cout << "阶段 1: 视线追踪 (Eye Pass)..." << std::endl;
    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = image_height-1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            // 像素内抖动抗锯齿
            auto u = (i + random_double()) / (image_width-1);
            auto v = (j + random_double()) / (image_height-1);
            Ray r = cam.get_ray(u, v);
            
            Color throughput(1,1,1);
            int depth = 0;
            
            while (depth < max_depth) {
                HitRecord rec;
                if (world.hit(r, 0.001, infinity, rec)) {
                    Ray scattered;
                    Color attenuation;
                    Color emitted = rec.mat_ptr->emitted(rec.u, rec.v, rec.p);
                    
                    // 尝试散射
                    if (rec.mat_ptr->scatter(r, rec, attenuation, scattered)) {
                        // 检查是否为漫反射材质 (Lambertian)
                        // 我们只在漫反射表面存储击中点以进行光子映射估计
                        if (std::dynamic_pointer_cast<Lambertian>(rec.mat_ptr)) {
                            HitPoint hp;
                            hp.p = rec.p;
                            hp.normal = rec.normal;
                            hp.wo = -unit_vector(r.direction());
                            hp.mat = rec.mat_ptr;
                            hp.throughput = throughput * attenuation; // 包含反照率
                            hp.pixel_index = ((image_height - 1 - j) * image_width + i);
                            hp.flux = Color(0,0,0);
                            hp.accumulated_flux = Color(0,0,0);
                            hp.radius_squared = initial_radius * initial_radius;
                            hp.n_photons = 0;
                            hp.accumulated_photon_count = 0;
                            
                            #pragma omp critical
                            hit_points.push_back(hp);
                            
                            break; // 漫反射表面终止视线路径
                        } else {
                            // 镜面反射 (金属, 玻璃等) 继续追踪
                            throughput = throughput * attenuation;
                            r = scattered;
                            depth++;
                        }
                    } else {
                        // 发光或被吸收
                        break;
                    }
                } else {
                    // 背景
                    break;
                }
            }
        }
    }
    std::cout << "生成了 " << hit_points.size() << " 个击中点。" << std::endl;

    // 2. 迭代循环 (光子映射阶段)
    double cell_size = initial_radius * 2.0;
    HashGrid grid(cell_size, hit_points.size() + 1000);
    
    for (int iter = 0; iter < iterations; ++iter) {
        std::cout << "\r迭代 " << iter + 1 << "/" << iterations << std::flush;
        
        // 重建哈希网格
        std::vector<HitPoint*> hp_ptrs;
        for (auto& hp : hit_points) hp_ptrs.push_back(&hp);
        grid.build(hp_ptrs);
        
        // 光子发射
        #pragma omp parallel for schedule(dynamic, 1)
        for (int p_idx = 0; p_idx < photons_per_iter; ++p_idx) {
            // 随机选择一个光源
            if (lights.empty()) continue;
            int light_idx = static_cast<int>(random_double(0, lights.size()-0.01));
            auto light = lights[light_idx];
            
            // 在光源表面采样 (假设是球体光源)
            if (auto sphere = std::dynamic_pointer_cast<Sphere>(light)) {
                Point3 origin = sphere->center + random_unit_vector() * sphere->radius;
                Vec3 dir = random_unit_vector();
                if (dot(dir, origin - sphere->center) < 0) dir = -dir; // 确保向外发射
                
                Ray photon_ray(origin, dir);
                Color photon_power = std::dynamic_pointer_cast<DiffuseLight>(sphere->mat_ptr)->emit->value(0,0,origin);
                
                int depth = 0;
                while (depth < max_depth) {
                    HitRecord rec;
                    if (world.hit(photon_ray, 0.001, infinity, rec)) {
                        // 1. 检查附近的击中点
                        if (std::dynamic_pointer_cast<Lambertian>(rec.mat_ptr)) {
                            // 在网格中查找
                            int h = grid.hash(rec.p);
                            // 检查当前单元 (简化版，未检查邻居)
                            for (auto hp : grid.table[h]) {
                                double dist_sq = (hp->p - rec.p).length_squared();
                                if (dist_sq <= hp->radius_squared) {
                                    // 检查法线方向是否一致
                                    if (dot(hp->normal, rec.normal) > 0.5) { 
                                        // 累积光子贡献
                                        #pragma omp atomic
                                        hp->flux.e[0] += photon_power.e[0];
                                        #pragma omp atomic
                                        hp->flux.e[1] += photon_power.e[1];
                                        #pragma omp atomic
                                        hp->flux.e[2] += photon_power.e[2];
                                        
                                        #pragma omp atomic
                                        hp->n_photons += 1;
                                    }
                                }
                            }
                        }
                        
                        // 2. 散射光子
                        Ray scattered;
                        Color attenuation;
                        if (rec.mat_ptr->scatter(photon_ray, rec, attenuation, scattered)) {
                            // 俄罗斯轮盘赌决定光子是否存活
                            double p = std::max(attenuation.x(), std::max(attenuation.y(), attenuation.z()));
                            if (random_double() > p) break;
                            
                            photon_power = photon_power * attenuation / p;
                            photon_ray = scattered;
                            depth++;
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }
        }
        
        // 更新击中点 (半径缩减)
        for (auto& hp : hit_points) {
            if (hp.n_photons > 0) {
                double alpha = 0.7; // 缩减参数
                double N = hp.accumulated_photon_count;
                double M = hp.n_photons;
                double ratio = (N + alpha * M) / (N + M);
                
                hp.radius_squared *= ratio;
                hp.accumulated_flux = (hp.accumulated_flux + hp.flux) * ratio;
                hp.accumulated_photon_count = N + alpha * M;
                
                hp.flux = Color(0,0,0); // 重置以便下一轮使用
                hp.n_photons = 0;
            }
        }
    }
    
    // 3. 重建图像
    buffer.assign(image_width * image_height * 3, 0);
    for (const auto& hp : hit_points) {
        // 辐射率估计公式
        Color radiance = hp.accumulated_flux / (pi * hp.radius_squared * photons_per_iter * iterations) * hp.throughput;

        // 色调映射 (Tone Mapping)
        radiance = aces_approx(radiance);
        
        // Gamma 校正
        auto r = sqrt(radiance.x());
        auto g = sqrt(radiance.y());
        auto b = sqrt(radiance.z());
        
        int index = hp.pixel_index * 3;
        buffer[index] = static_cast<unsigned char>(256 * clamp(r, 0.0, 0.999));
        buffer[index+1] = static_cast<unsigned char>(256 * clamp(g, 0.0, 0.999));
        buffer[index+2] = static_cast<unsigned char>(256 * clamp(b, 0.0, 0.999));
    }
    
    std::cout << "\nSPPM 渲染完成。" << std::endl;
}

#endif