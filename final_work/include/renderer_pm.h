#ifndef RENDERER_PM_H
#define RENDERER_PM_H

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

// 简单的光子结构体
struct Photon {
    Point3 p;       // 位置
    Vec3 dir;       // 入射方向
    Color power;    // 能量
};

// 哈希网格：用于存储光子
class PhotonMap {
public:
    PhotonMap(double cell_size, int size) : cell_size(cell_size), size(size) {
        table.resize(size);
    }
    //photons是一个数组，每一个元素是一个Photon结构体
    void build(const std::vector<Photon>& photons) {
        for (auto& list : table) list.clear();
        for (size_t i = 0; i < photons.size(); ++i) {
            int idx = hash(photons[i].p);
            table[idx].push_back(&photons[i]);
        }
    }

    // 查找附近的 N 个光子并估算辐射度
    // 这里简化为固定半径查找 (Fixed Radius Search)
    Color estimate_radiance(const Point3& p, const Vec3& normal, double radius) const {
        Color flux(0,0,0);
        int count = 0;
        double r2 = radius * radius;

        int cx = static_cast<int>(std::floor(p.x() / cell_size));
        int cy = static_cast<int>(std::floor(p.y() / cell_size));
        int cz = static_cast<int>(std::floor(p.z() / cell_size));

        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int idx = hash_coords(cx + dx, cy + dy, cz + dz);
                    for (const auto* photon : table[idx]) {
                        double dist_sq = (photon->p - p).length_squared();
                        if (dist_sq <= r2) {
                            // 简单的密度估计核函数 (Constant Kernel)
                            // 也可以使用 Cone Filter 或 Gaussian Filter
                            // 检查光子方向是否从表面上方射入 (可选，防止漏光)
                            if (dot(normal, photon->dir) < 0) {
                                flux += photon->power;
                                count++;
                            }
                        }
                    }
                }
            }
        }
        
        if (count == 0) return Color(0,0,0);
        // 辐射度 = Flux / Area
        return flux / (pi * r2);
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

    std::vector<std::list<const Photon*>> table;
    double cell_size;
    int size;
};

// pass1光子追踪：发射光子并存储在 map 中
inline void trace_photon_pm(Ray ray, int dep, Color power, std::vector<Photon>& photons, const HittableObjList& world) {
    if (max_in_xyz(power) < 1e-9) return;// 如果辐射通量的最大分量小于1e-9,说明该光子已经被材质所吸收，直接返回
    
    HitRecord rec;//用hittable类里面那个结构体了，用来获取光子撞到的表面的信息
    if (!world.hit(ray, 0.001, infinity, rec)) return;//如果射到世界world外面了，也返回
    //到这里的光线都是没有被吸收而且撞到东西的。下面这三个是photon的信息，后面存到photon map里面
    Point3 x = rec.p;//光子撞到的点，也是photon的位置
    Vec3 n = rec.normal;//光子撞到的表面的法线
    Vec3 nl = dot(n, ray.direction()) < 0 ? n : -n;//调整法线方向，使其指向入射光线的一侧
    
    std::pair<Refl_t, Color> feature = get_feature(rec.mat_ptr, x);
    Color f = feature.second;//获取颜色（.first是材质类型）
    
    // 俄罗斯轮盘赌，当递归次数大于5次时候才会触发
    double p_survive = max_in_xyz(f);
    if (++dep > 5) {
        if (random_double() < p_survive) f = f / p_survive;
        else return;
    }
    //对光子撞到的材质进行判断
    if (feature.first == DIFF) {
        // 撞到漫反射表面：photons是一个数组，每一个元素是一个Photon结构体，光子photonpush进去。
        #pragma omp critical
        photons.push_back({x, ray.direction(), power});
        
        // 然后发生漫反射散射，继续弹射光子
        double r1 = 2 * pi * random_double();//均匀采样一个方向
        double r2 = random_double();//0到1之间的随机数  
        double r2s = sqrt(r2);//开根号是为了均匀采样球面
        
        Vec3 u = unit_vector(cross((fabs(nl.x()) > .1 ? Vec3(0, 1, 0) : Vec3(1, 0, 0)), nl));//u是一个与nl垂直的单位向量
        Vec3 v = cross(nl, u);//v也是一个与nl垂直的单位向量，并且u,v,nl三者互相垂直
        Vec3 d = unit_vector(u * cos(r1) * r2s + v * sin(r1) * r2s + nl * sqrt(1 - r2));//计算出漫反射方向d
        //用的公式是半球均匀采样的那个

        // 继续追踪光子
        trace_photon_pm(Ray(x, d), dep, power * f, photons, world);
        
    } else if (feature.first == SPEC) {
        // 撞到镜面，反射
        Ray reflray = Ray(x, reflect(ray.direction(), n));
        trace_photon_pm(reflray, dep, power * f, photons, world);//继续追踪光子
    } else if (feature.first == REFR) {
        //撞到电介质，折射，这里类似于material里面的scatter函数，因为不方便直接调用，就抄了了一遍
        double refraction_ratio = dot(n, ray.direction()) < 0 ? (1.0/1.5) : 1.5;
        Vec3 unit_dir = unit_vector(ray.direction());
        double cos_theta = fmin(dot(-unit_dir, nl), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta*cos_theta);
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        
        Vec3 d_refracted;
        if (!cannot_refract) d_refracted = refract(unit_dir, nl, refraction_ratio);
        
        if (cannot_refract) {
            trace_photon_pm(Ray(x, reflect(unit_dir, nl)), dep, power * f, photons, world);
        } else {
            auto r0 = (1-1.5)/(1+1.5); r0 = r0*r0;
            double Re = r0 + (1-r0)*pow((1 - cos_theta), 5);
            double Tr = 1 - Re;
            double P = .25 + .5 * Re;
            
            if (random_double() < P) {
                trace_photon_pm(Ray(x, reflect(unit_dir, nl)), dep, power * f * (Re/P), photons, world);
            } else {
                trace_photon_pm(Ray(x, d_refracted), dep, power * f * (Tr/(1-P)), photons, world);
            }
        }
    }
}

// pass2光线追踪：使用光子图估算辐射度
inline Color eye_trace_estimate(Ray ray, int dep, int max_depth, const HittableObjList& world, const PhotonMap& photon_map, double radius) {
    HitRecord rec;
    if (!world.hit(ray, 0.001, infinity, rec)) return Color(0,0,0); // 背景色
    
    Point3 x = rec.p;
    Vec3 n = rec.normal;
    Vec3 nl = dot(n, ray.direction()) < 0 ? n : -n;
    
    std::pair<Refl_t, Color> feature = get_feature(rec.mat_ptr, x);
    Color f = feature.second;
    
    if (feature.first == DIFF) {
        // 这里为了简化，全部使用 Photon Map
        Color indirect = photon_map.estimate_radiance(x, nl, radius);
        return f * indirect;
    } else if (feature.first == SPEC) {
        if (dep > max_depth) return Color(0,0,0);//超过最大递归深度就返回黑色
        Ray reflray = Ray(x, reflect(ray.direction(), n));
        return f * eye_trace_estimate(reflray, dep + 1, max_depth, world, photon_map, radius);
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
            return f * eye_trace_estimate(Ray(x, reflect(unit_dir, nl)), dep + 1, max_depth, world, photon_map, radius);
        } else {
            auto r0 = (1-1.5)/(1+1.5); r0 = r0*r0;
            double Re = r0 + (1-r0)*pow((1 - cos_theta), 5);
            double Tr = 1 - Re;
            double P = .25 + .5 * Re;
            
            // 这里的逻辑和pathtracing非常相似，可以尝试一下之前没试过的smallpt里面的做法：
            // 浅层递归使用分裂光线减少噪点，深层递归使用俄罗斯轮盘赌
            if (dep < 3) {
                Color reflection = eye_trace_estimate(Ray(x, reflect(unit_dir, nl)), dep + 1, max_depth, world, photon_map, radius);
                Color refraction = eye_trace_estimate(Ray(x, d_refracted), dep + 1, max_depth, world, photon_map, radius);
                return f * (Re * reflection + Tr * refraction);
            } else {
                if (random_double() < P) {
                    return f * (Re/P) * eye_trace_estimate(Ray(x, reflect(unit_dir, nl)), dep + 1, max_depth, world, photon_map, radius);
                } else {
                    return f * (Tr/(1-P)) * eye_trace_estimate(Ray(x, d_refracted), dep + 1, max_depth, world, photon_map, radius);
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
    std::cout << "Starting Standard Photon Mapping (PM)..." << std::endl;
    std::cout << "Photons: " << num_photons << ", Radius: " << radius << std::endl;

    // 1. Photon Pass
    std::cout << "Photon Pass..." << std::endl;
    std::vector<Photon> photons;
    photons.reserve(num_photons);
    
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
            
            trace_photon_pm(Ray(origin, dir), 0, photon_power, photons, world);
        }
    }
    std::cout << "Stored Photons: " << photons.size() << std::endl;

    // 2. Build Photon Map
    std::cout << "Building Photon Map..." << std::endl;
    PhotonMap map(radius * 2.0, photons.size() + 1000);
    map.build(photons);

    // 3. Eye Pass (Render)
    std::cout << "Rendering..." << std::endl;
    std::vector<Color> final_image(image_width * image_height);
    
    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = image_height-1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            auto u = (i + random_double()) / (image_width-1);
            auto v = (j + random_double()) / (image_height-1);
            Ray r = cam.get_ray(u, v);
            
            Color pixel_color = eye_trace_estimate(r, 0, max_depth, world, map, radius);
            final_image[(image_height - 1 - j) * image_width + i] = pixel_color;
        }
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
    std::cout << "Done." << std::endl;
}

#endif