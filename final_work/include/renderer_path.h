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
    HitRecord rec;

    // 0.001 是为了忽略非常接近零的撞击
    if (world.hit(r, 0.001, infinity, rec)) {
        Ray scatteredRay;
        Color attenuation;
        Color emitted = rec.mat_ptr->emitted(0, 0, rec.p);

        // 递归步骤：散射光线并累积颜色
        if (rec.mat_ptr->scatter(r, rec, attenuation, scatteredRay)) {
            // Russian Roulette (轮盘赌)
            if (depth < 45) {
                double p = 0.8; // 存活概率
                if (random_double() > p)
                    return emitted; // 终止路径
                attenuation = attenuation / p; // 能量补偿
            }

            return emitted + attenuation * ray_color(scatteredRay, world, depth-1) ;
        }
        // 增加微弱的环境光 (Ambient Light)
        Color ambient(0.1, 0.1, 0.1);
        return emitted+ attenuation * ambient;
    }

    // 环境光
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
    std::cout << "Starting Path Tracing..." << std::endl;
    
    buffer.resize(image_width * image_height * 3);
    int scanlines_remaining = image_height;

    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = image_height-1; j >= 0; --j) {
        #pragma omp critical
        {
            --scanlines_remaining;
            if (scanlines_remaining % 10 == 0)
                std::cerr << "\rScanlines remaining: " << scanlines_remaining << ' ' << std::flush;
        }

        for (int i = 0; i < image_width; ++i) {
            Color pixel_color(0, 0, 0);
            for (int s = 0; s < samples_per_pixel; ++s) {
                auto u = (i + random_double()) / (image_width-1);
                auto v = (j + random_double()) / (image_height-1);
                Ray r = cam.get_ray(u, v);
                pixel_color += ray_color(r, world, max_depth);
            }
            
            // Tone Mapping + Gamma
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
    std::cout << "\nPath Tracing Done.\n";
}

#endif
