#ifndef RENDERER_PATH_H
#define RENDERER_PATH_H

#include "utils.h"
#include "hittable_list.hpp"
#include "camera.h"
#include "material.hpp"
#include <iostream>
#include <vector>
#include <omp.h>

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
    return (1.0-t)*Color(1.0, 1.0, 1.0) + t*Color(0.5, 0.7, 1.0);
}

inline void render_path_tracing(
    const HittableObjList& world, 
    const Camera& cam, 
    int image_width, 
    int image_height, 
    int samples_per_pixel, 
    int max_depth,
    std::vector<unsigned char>& buffer
) {
    std::cout << "开始光追渲染" << std::endl;
    
    buffer.resize(image_width * image_height * 3);
    int height_remain = image_height;

    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = image_height-1; j >= 0; --j) {
        #pragma omp critical
        {
            --height_remain;
                std::cerr << "\r剩余高度height: " << height_remain << ' ' << std::flush;
        }

        for (int i = 0; i < image_width; ++i) {
            Color pixel_color(0, 0, 0);
            for (int s = 0; s < samples_per_pixel; ++s) {
                auto u = (i + random_double()) / (image_width-1);
                auto v = (j + random_double()) / (image_height-1);
                Ray r = cam.get_ray(u, v);
                pixel_color += ray_color(r, world, max_depth);
            }
            
            // Tone Mapping色调映射 + Gamma矫正
            auto scale = 1.0 / samples_per_pixel;
            Vec3 color = pixel_color * scale;
            color = aces_approx(color);

            auto r = sqrt(color.x());
            auto g = sqrt(color.y());
            auto b = sqrt(color.z());

            int index = ((image_height - 1 - j) * image_width + i) * 3;
            buffer[index] = static_cast<unsigned char>(256 * clamp(r, 0.0, 0.999));
            buffer[index+1] = static_cast<unsigned char>(256 * clamp(g, 0.0, 0.999));
            buffer[index+2] = static_cast<unsigned char>(256 * clamp(b, 0.0, 0.999));
        }
    }
    std::cout << "\n光追渲染完成。\n";
}

#endif
