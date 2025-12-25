#ifndef RENDERER_SPPM_H
#define RENDERER_SPPM_H

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

// --- Old Implementation (Commented out for comparison) ---
/*
struct HitPoint {
    Point3 p;
    Vec3 normal;
    Vec3 wo; // 指向相机的方向
    shared_ptr<Material> mat;
    Color throughput; // 路径权重 (Eye Path Throughput)
    int pixel_index; // 图像缓冲区中的索引

    // 直接光照贡献 (Next Event Estimation)
    Color direct_light;

    // SPPM 统计数据
    Color flux; // 当前迭代收集的光子能量
    Color accumulated_flux; // 累积的光子能量 (包含半径缩减修正)
    double radius_squared; // 当前搜索半径的平方
    double n_photons; // 当前迭代收集的光子数
    double accumulated_photon_count; // 累积有效光子数
};

struct Photon {
    Point3 p;
    Vec3 dir;
    Color power;
};

// 改进的哈希网格，支持邻域搜索
class HashGrid {
public:
    HashGrid(double cell_size, int size) : cell_size(cell_size), size(size) {
        table.resize(size);
    }

    void build(const std::vector<HitPoint*>& hit_points) {
        for (auto& list : table) list.clear();
        for (auto hp : hit_points) {
            // 将击中点添加到其所在的网格单元
            int idx = hash(hp->p);
            table[idx].push_back(hp);
        }
    }

    // 查找给定点 p 周围半径 radius 内的所有击中点
    // 实际上我们检查 p 所在的单元及其 3x3x3 邻域
    void find_nearby(const Point3& p, const Vec3& normal, std::vector<HitPoint*>& result) {
        int cx = static_cast<int>(std::floor(p.x() / cell_size));
        int cy = static_cast<int>(std::floor(p.y() / cell_size));
        int cz = static_cast<int>(std::floor(p.z() / cell_size));

        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int idx = hash_coords(cx + dx, cy + dy, cz + dz);
                    for (auto hp : table[idx]) {
                        // 距离检查
                        double dist_sq = (hp->p - p).length_squared();
                        if (dist_sq <= hp->radius_squared) {
                            // 法线检查 (避免漏光)
                            if (dot(hp->normal, normal) > 0.5) {
                                result.push_back(hp);
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
        // 使用大质数进行哈希
        unsigned int h = (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
        return h % size;
    }

    std::vector<std::list<HitPoint*>> table;
    double cell_size;
    int size;
};

// 计算直接光照 (Next Event Estimation)
inline Color estimate_direct_light(
    const HittableObjList& world, 
    const std::vector<shared_ptr<HittableObj>>& lights,
    const HitRecord& rec,
    const Vec3& view_dir
) {
    Color direct_light(0,0,0);
    
    for (const auto& light : lights) {
        // 假设光源是球体
        if (auto sphere = std::dynamic_pointer_cast<Sphere>(light)) {
            // 对光源进行采样
            Vec3 light_pos = sphere->center + random_unit_vector() * sphere->radius;
            Vec3 light_dir = light_pos - rec.p;
            double dist_sq = light_dir.length_squared();
            double dist = sqrt(dist_sq);
            light_dir = unit_vector(light_dir);

            // 阴影光线
            Ray shadow_ray(rec.p, light_dir);
            HitRecord shadow_rec;
            
            // 检查是否被遮挡 (注意 t_max 设为 dist - epsilon)
            if (!world.hit(shadow_ray, 0.001, dist - 0.001, shadow_rec)) {
                // 未被遮挡，计算贡献
                double cos_theta = dot(rec.normal, light_dir);
                if (cos_theta > 0) {
                    // 获取光源强度 (假设 DiffuseLight)
                    Color light_intensity = std::dynamic_pointer_cast<DiffuseLight>(sphere->mat_ptr)->emit->value(0, 0, light_pos);
                    
                    double area = 4 * pi * sphere->radius * sphere->radius;
                    Vec3 light_normal = unit_vector(light_pos - sphere->center);
                    double cos_light = dot(-light_dir, light_normal);
                    
                    if (cos_light > 0) {
                        Ray scattered;
                        Color attenuation;
                        if (rec.mat_ptr->scatter(shadow_ray, rec, attenuation, scattered)) {
                             direct_light += light_intensity * attenuation * cos_theta * cos_light * area / (pi * dist_sq);
                        }
                    }
                }
            }
        }
    }
    return direct_light;
}


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
    // ... old implementation ...
}
*/

// --- New Implementation based on expert code ---

// Forward declarations
struct SPPMnode;
class SPPM_KDTree;

// SPPM Node (Hit Point)
struct SPPMnode {
    Point3 p;
    Color throughput;
    Vec3 normal;
    double r2; // radius squared
    int index; // pixel index
    double prob; // probability
    
    Color flux;
    Color accumulated_flux;
    double n_photons;
    double accumulated_photon_count;
    
    // Constructor for Hit Point
    SPPMnode(Point3 p_, Color tr_, Vec3 n_, double r2_, int idx_, double prob_)
        : p(p_), throughput(tr_), normal(n_), r2(r2_), index(idx_), prob(prob_),
          flux(0,0,0), accumulated_flux(0,0,0), n_photons(0), accumulated_photon_count(0) {}
          
    // Constructor for Photon (used in query)
    SPPMnode(Point3 p_, Color power_, Vec3 n_)
        : p(p_), throughput(power_), normal(n_), r2(0), index(-1), prob(1.0) {}
};

// KDTree (using HashGrid internally)
class SPPM_KDTree {
public:
    SPPM_KDTree(double cell_size, int size) : cell_size(cell_size), size(size) {
        table.resize(size);
    }

    void build(std::vector<SPPMnode>& nodes) {
        for (auto& list : table) list.clear();
        for (auto& node : nodes) {
            int idx = hash(node.p);
            table[idx].push_back(&node);
        }
    }

    // Query with a photon (represented as SPPMnode)
    void query(const SPPMnode& photon, void* c = nullptr) {
        int cx = static_cast<int>(std::floor(photon.p.x() / cell_size));
        int cy = static_cast<int>(std::floor(photon.p.y() / cell_size));
        int cz = static_cast<int>(std::floor(photon.p.z() / cell_size));

        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int idx = hash_coords(cx + dx, cy + dy, cz + dz);
                    for (auto hp : table[idx]) {
                        double dist_sq = (hp->p - photon.p).length_squared();
                        if (dist_sq <= hp->r2) {
                            if (dot(hp->normal, photon.normal) > 0.5) {
                                // Update flux
                                // photon.throughput is the power
                                #pragma omp atomic
                                hp->flux.e[0] += photon.throughput.e[0];
                                #pragma omp atomic
                                hp->flux.e[1] += photon.throughput.e[1];
                                #pragma omp atomic
                                hp->flux.e[2] += photon.throughput.e[2];
                                
                                #pragma omp atomic
                                hp->n_photons += 1;
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

    std::vector<std::list<SPPMnode*>> table;
    double cell_size;
    int size;
};

// sppm_backtrace
inline std::vector<SPPMnode> sppm_backtrace(Ray ray, int dep, int index, const HittableObjList& world, Color pref = Color(1, 1, 1), double prob = 1.) {
    std::vector<SPPMnode> result, tmp;
    if (max_in_xyz(pref) < 1e-4 || prob < 1e-4) return result;
    int into = 0;
    std::pair<int, double> intersect_result = nearest_hit(ray, world);
    if (intersect_result.first == -1)
        return result;
    
    HittableObj* obj = world.objects[intersect_result.first].get();
    Point3 x = ray.at(intersect_result.second);
    
    HitRecord rec;
    obj->hit(ray, 0.001, infinity, rec);
    Vec3 n = rec.normal;
    
    std::pair<Refl_t, Color> feature = get_feature(rec.mat_ptr, x);
    Color f = feature.second;
    Vec3 nl = dot(n, ray.direction()) < 0 ? into = 1, n : -n;
    double p = max_in_xyz(f);
    
    if (max_in_xyz(f) < 1e-4)
        return result;
        
    if (++dep > 5)
        if(random_double() < p) f = f / p;
        else return result;
        
    Ray reflray = Ray(x, reflect(ray.direction(), nl));
    
    if (feature.first == DIFF) {
        result.push_back(SPPMnode(x, pref * f, nl, 0.1, index, prob)); 
    }
    
    if (feature.first == SPEC) {
        tmp = sppm_backtrace(reflray, dep, index, world, pref * f, prob);
        result.insert(result.end(), tmp.begin(), tmp.end());
    }
    
    if (feature.first == REFR) {
        double refraction_ratio = into ? (1.0/1.5) : 1.5;
        Vec3 unit_dir = unit_vector(ray.direction());
        
        double cos_theta = fmin(dot(-unit_dir, nl), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta*cos_theta);
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        
        if (cannot_refract) {
             return sppm_backtrace(reflray, dep, index, world, pref * f, prob);
        }
        
        auto r0 = (1-1.5)/(1+1.5); r0 = r0*r0;
        double Re = r0 + (1-r0)*pow((1 - cos_theta), 5);
        double Tr = 1 - Re;
        double P = .25 + .5 * Re;
        double RP = Re / P;
        double TP = Tr / (1 - P);
        
        Vec3 refracted = refract(unit_dir, nl, refraction_ratio);

        if (dep > 2) {
            if (random_double() < P) {
                tmp = sppm_backtrace(reflray, dep, index, world, pref * f, prob * RP);
                result.insert(result.end(), tmp.begin(), tmp.end());
            } else {
                tmp = sppm_backtrace(Ray(x, refracted), dep, index, world, pref * f, prob * TP);
                result.insert(result.end(), tmp.begin(), tmp.end());
            }
        } else {
             tmp = sppm_backtrace(reflray, dep, index, world, pref * f, prob * Re);
             result.insert(result.end(), tmp.begin(), tmp.end());
             tmp = sppm_backtrace(Ray(x, refracted), dep, index, world, pref * f, prob * Tr);
             result.insert(result.end(), tmp.begin(), tmp.end());
        }
    }
    return result;
}

// sppm_forward
inline void sppm_forward(Ray ray, int dep, Color col, SPPM_KDTree* kdt, const HittableObjList& world, double prob = 1.) {
    if (max_in_xyz(col) < 1e-4) return;
    int into = 0;
    std::pair<int, double> intersect_result = nearest_hit(ray, world);
    if (intersect_result.first == -1) return;
    
    HittableObj* obj = world.objects[intersect_result.first].get();
    Point3 x = ray.at(intersect_result.second);
    
    HitRecord rec;
    obj->hit(ray, 0.001, infinity, rec);
    Vec3 n = rec.normal;
    
    std::pair<Refl_t, Color> feature = get_feature(rec.mat_ptr, x);
    Color f = feature.second;
    Vec3 nl = dot(n, ray.direction()) < 0 ? into = 1, n : -n;
    double p = max_in_xyz(f);
    
    if (max_in_xyz(f) < 1e-4) {
        kdt->query(SPPMnode(x, col, nl));
        return;
    }
    
    if (++dep > 5) {
        if (random_double() < p) f = f / p;
        else {
            kdt->query(SPPMnode(x, col, nl));
            return;
        }
    }
    
    if (feature.first == DIFF) {
        kdt->query(SPPMnode(x, col, nl));
        
        double r1 = 2 * pi * random_double();
        double r2 = random_double();
        double r2s = sqrt(r2);
        
        Vec3 w = nl;
        Vec3 u = unit_vector(cross((fabs(w.x()) > .1 ? Vec3(0, 1, 0) : Vec3(1, 0, 0)), w));
        Vec3 v = cross(w, u);
        Vec3 d = unit_vector(u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1 - r2));
        
        sppm_forward(Ray(x, d), dep, col * f, kdt, world, prob);
    } else {
        Ray reflray = Ray(x, reflect(ray.direction(), nl));
        if (feature.first == SPEC) {
            sppm_forward(reflray, dep, col * f, kdt, world, prob);
        } else {
             double refraction_ratio = into ? (1.0/1.5) : 1.5;
             Vec3 unit_dir = unit_vector(ray.direction());
             
             double cos_theta = fmin(dot(-unit_dir, nl), 1.0);
             double sin_theta = sqrt(1.0 - cos_theta*cos_theta);
             bool cannot_refract = refraction_ratio * sin_theta > 1.0;
             
             Vec3 d_refracted;
             if (!cannot_refract) d_refracted = refract(unit_dir, nl, refraction_ratio);
             
             if (cannot_refract) {
                 sppm_forward(reflray, dep, col * f, kdt, world, prob);
             } else {
                 auto r0 = (1-1.5)/(1+1.5); r0 = r0*r0;
                 double Re = r0 + (1-r0)*pow((1 - cos_theta), 5);
                 double Tr = 1 - Re;
                 double P = .25 + .5 * Re;
                 double RP = Re / P;
                 double TP = Tr / (1 - P);
                 
                 if (dep > 2) {
                     if (random_double() < P) {
                         sppm_forward(reflray, dep, col * f, kdt, world, prob * RP);
                     } else {
                         sppm_forward(Ray(x, d_refracted), dep, col * f, kdt, world, prob * TP);
                     }
                 } else {
                     sppm_forward(reflray, dep, col * f, kdt, world, prob * Re);
                     sppm_forward(Ray(x, d_refracted), dep, col * f, kdt, world, prob * Tr);
                 }
             }
        }
    }
}

// New render_sppm
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
    std::cout << "Starting SPPM (Expert Mode)..." << std::endl;
    
    // 1. Eye Pass
    std::vector<SPPMnode> hit_points;
    
    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = image_height-1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            auto u = (i + random_double()) / (image_width-1);
            auto v = (j + random_double()) / (image_height-1);
            Ray r = cam.get_ray(u, v);
            
            int pixel_index = ((image_height - 1 - j) * image_width + i);
            std::vector<SPPMnode> nodes = sppm_backtrace(r, 0, pixel_index, world);
            
            #pragma omp critical
            hit_points.insert(hit_points.end(), nodes.begin(), nodes.end());
        }
    }
    
    // Initialize radius
    for (auto& hp : hit_points) hp.r2 = initial_radius * initial_radius;
    
    std::cout << "Hit points: " << hit_points.size() << std::endl;
    
    // 2. Iterations
    double cell_size = initial_radius * 2.0;
    SPPM_KDTree kdt(cell_size, hit_points.size() + 1000);
    
    for (int iter = 0; iter < iterations; ++iter) {
        std::cout << "\rIter " << iter + 1 << "/" << iterations << std::flush;
        
        kdt.build(hit_points);
        
        #pragma omp parallel for schedule(dynamic, 1)
        for (int p_idx = 0; p_idx < photons_per_iter; ++p_idx) {
            if (lights.empty()) continue;
            int light_idx = static_cast<int>(random_double(0, lights.size()-0.01));
            auto light = lights[light_idx];
            if (auto sphere = std::dynamic_pointer_cast<Sphere>(light)) {
                Point3 origin = sphere->center + random_unit_vector() * sphere->radius;
                Vec3 dir = random_unit_vector();
                if (dot(dir, origin - sphere->center) < 0) dir = -dir;
                
                Color L = std::dynamic_pointer_cast<DiffuseLight>(sphere->mat_ptr)->emit->value(0,0,origin);
                double area = 4 * pi * sphere->radius * sphere->radius;
                Color photon_power = L * area * pi / photons_per_iter;
                
                sppm_forward(Ray(origin, dir), 0, photon_power, &kdt, world);
            }
        }
        
        // Radius reduction
        for (auto& hp : hit_points) {
            if (hp.n_photons > 0) {
                double alpha = 0.7;
                double N = hp.accumulated_photon_count;
                double M = hp.n_photons;
                double ratio = (N + alpha * M) / (N + M);
                hp.r2 *= ratio;
                hp.accumulated_flux = (hp.accumulated_flux + hp.flux) * ratio;
                hp.accumulated_photon_count = N + alpha * M;
                hp.flux = Color(0,0,0);
                hp.n_photons = 0;
            }
        }
    }
    
    // 3. Reconstruct
    std::vector<Color> final_image(image_width * image_height, Color(0,0,0));
    for (const auto& hp : hit_points) {
        Color indirect = Color(0,0,0);
        if (hp.r2 > 1e-8) {
            indirect = hp.accumulated_flux / (pi * hp.r2) * hp.throughput;
        }
        Color ambient(0.05, 0.05, 0.05);
        final_image[hp.index] += indirect + ambient * hp.throughput;
    }
    
    // Write buffer
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
    std::cout << "\nDone." << std::endl;
}

#endif
