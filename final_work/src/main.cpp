#include "utils.h"
#include "hittable_list.hpp"
#include "sphere.h"
#include "camera.h"
#include "material.hpp"
#include "mesh_loader.h"
#include "bvh.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <omp.h>

// --- 新增: ACES 色调映射 ---
// 这种映射曲线能更好地保留高光细节，让画面更有电影感
Vec3 aces_approx(Vec3 v) {
    v *= 0.6;
    double a = 2.51;
    double b = 0.03;
    double c = 2.43;
    double d = 0.59;
    double e = 0.14;
    // 逐分量应用公式
    return Vec3(
        clamp((v.x()*(a*v.x()+b))/(v.x()*(c*v.x()+d)+e), 0.0, 1.0),
        clamp((v.y()*(a*v.y()+b))/(v.y()*(c*v.y()+d)+e), 0.0, 1.0),
        clamp((v.z()*(a*v.z()+b))/(v.z()*(c*v.z()+d)+e), 0.0, 1.0)
    );
}


// 递归光线追踪函数
/**
*@param  r 入射光线
*@param  world 场景中的所有物体
*@param depth 当前递归深度
*@return 该光线的颜色
*/
Color ray_color(const Ray& r, const HittableObj& world, int depth) {
    HitRecord rec;

    // 如果超过了光线反弹限制，则不再收集光线。
    // if (depth <= 0)
    //     return Color(0,0,0);

    // 0.001 是为了忽略非常接近零的撞击
    if (world.hit(r, 0.001, infinity, rec)) {
        Ray scatteredRay;
        Color attenuation;
        Color emitted = rec.mat_ptr->emitted(0, 0, rec.p);

        // 递归步骤：散射光线并累积颜色
        if (rec.mat_ptr->scatter(r, rec, attenuation, scatteredRay)) {
            // Russian Roulette (轮盘赌)
            // 当深度较深时（例如反弹超过5次，即 depth < 45），启用轮盘赌
            if (depth < 45) {
                double p = 0.8; // 存活概率
                if (random_double() > p)
                    return emitted; // 终止路径
                attenuation = attenuation / p; // 能量补偿
            }
            return emitted + attenuation * ray_color(scatteredRay, world, depth-1);
        }
        return emitted;
    }

    // 如果没有击中任何物体，返回环境光
    // 为了防止背景太暗，可以稍微调亮一点，或者保持原样作为环境光
    Vec3 unit_direction = unit_vector(r.direction());
    auto t = 0.5*(unit_direction.y() + 1.0);
    return (1.0-t)*Color(1.0, 1.0, 1.0) + t*Color(0.5, 0.7, 1.0);
}

// --- 拓展 1: 光子映射占位符 ---
// void build_photon_map(const Hittable& world, const std::vector<Light>& lights) {
//     // 1. 从光源发射光子
//     // 2. 追踪光子穿过场景（反射/折射）
//     // 3. 将光子撞击点存储在 k-d 树中
// }
// Color estimate_caustics(const HitRecord& rec) {
//     // 查询 k-d 树以获取最近的光子并估计辐射率
//     return Color(0,0,0);
// }


// // 将颜色写入流的工具函数,这玩意不方便定义在utils.h里，因为会引入循环依赖
// void write_color(std::ostream &out, Color pixelColor, int samplesPerPixel)
//  {
//     auto r = pixelColor.x();
//     auto g = pixelColor.y();
//     auto b = pixelColor.z();

//     // 将颜色除以样本数并进行 gamma=2.0 的伽马校正。
//     auto scale = 1.0 / samplesPerPixel;
//     r = sqrt(scale * r);
//     g = sqrt(scale * g);
//     b = sqrt(scale * b);

//     // 写入每个颜色分量的转换后的 [0,255] 值。
//     out << static_cast<int>(256 * clamp(r, 0.0, 0.999)) << ' '
//         << static_cast<int>(256 * clamp(g, 0.0, 0.999)) << ' '
//         << static_cast<int>(256 * clamp(b, 0.0, 0.999)) << '\n';
// }

int main(int argc, char* argv[]) {

    if (argc > 1 && std::string(argv[1]) == "-h") {
        std::cout << "用法：./RayTracer <image_name>.ppm <image_width> <image_height> <samples_per_pixel>\n";
        std::cout << "例如：./RayTracer output.ppm 800 450 1000\n";
        std::cout << "如果不提供参数,将使用默认值。400, 225, 500\n";
        std::cout <<"查看ppm文件: cd ../&& python3 read_ppm.py <flie_name>.ppm"<< std::endl;
        return 0;
    }
    // 图像
    const auto aspect_ratio = 16.0 / 9.0;
    // argv[0] 是程序名, argv[1] 是文件名, argv[2] 是宽度, argv[3] 是高度, argv[4] 是采样数
    const int image_width = (argc >= 3) ? std::atoi(argv[2]) : 400;
    const int image_height = (argc >= 4) ? std::atoi(argv[3]) : static_cast<int>(image_width / aspect_ratio);
    const int samples_per_pixel = (argc >= 5) ? std::atoi(argv[4]) : 500; // 每个像素的采样数，这个玩意越大，噪点越少越真实，但是越慢
    const int max_depth = 50; // 递归深度

    // 世界，显而易见的世界也是一个支持光追的物体列表
    HittableObjList world;

    //用指针的方式创建材质，方便多个物体共享同一个材质。
    auto material_ground = make_shared<Lambertian>(Color(0.5, 0.5, 0.5)); // 地面灰色
    // auto material_ground_texture = make_shared<ImageTexture>("maodie.png"); 
    // auto material_ground = make_shared<Lambertian>(material_ground_texture);

    auto material_wall_back = make_shared<Lambertian>(Color(0.7, 0.3, 0.3)); // 后墙红色
    auto material_wall_right = make_shared<Lambertian>(Color(0.3, 0.7, 0.3)); // 右墙绿色
    auto material_wall_left = make_shared<Lambertian>(Color(0.3, 0.3, 0.7)); // 左墙蓝色
    
    auto material_center = make_shared<Lambertian>(Color(0.1, 0.2, 0.5));
    // auto material_center_texture = make_shared<ImageTexture>("maodie.png");
    // auto material_center = make_shared<Lambertian>(material_center_texture);

    auto material_glass = make_shared<Dielectric>(1.5); // 玻璃 / 水晶球
    // 增加一点粗糙度 (fuzz = 0.1) 让金属看起来更真实，不是完美的镜子
    auto material_metal  = make_shared<Metal>(Color(0.8, 0.6, 0.2), 0.1);
    // 提高光源亮度，配合 ACES 色调映射
    auto material_light = make_shared<DiffuseLight>(Color(8.0, 8.0, 8.0)); // 强光
    // 加载 Mesh (Dragon)
    // 这里的 scale 和 offset 是根据 obj 文件的大致坐标范围估算的
    // 原始坐标大概在 X[-60, -20], Y[-16, -10], Z[-10, 2]
    // 我们希望它在场景中心 (0, 0, -1) 附近，且大小适中
    auto material_dragon = make_shared<Metal>(Color(0.9, 0.1, 0.1), 0.1); // 红色金属龙
    auto dragon_triangles = load_obj("../mesh/xyzrgb_dragon.obj", material_glass, 0.1, Point3(4, 1.3, -1.0));
    if (dragon_triangles->objects.size() > 0) {
        std::cout << "正在构建 BVH..." << std::endl;
        world.add(make_shared<BvhNode>(*dragon_triangles, 0, 1));
    }
    //目前平面类还每实现
    // 地面
    world.add(make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, material_ground));
    // 后墙 (z = -3 左右)
    world.add(make_shared<Sphere>(Point3(0, 0, -1003), 1000, material_wall_back));
    // 左墙
    world.add(make_shared<Sphere>(Point3(-1002, 0, -1), 1000, material_wall_left));
    // 右墙
    world.add(make_shared<Sphere>(Point3( 1002, 0, -1), 1000, material_wall_right));

    // 光源 (在上方)
    world.add(make_shared<Sphere>(Point3(0, 4, -1), 1.0, material_light));

    // 物体
    // world.add(make_shared<Sphere>(Point3( 0.0,    0.0, -1.0),   0.5, material_center));
    // world.add(make_shared<Sphere>(Point3(-1.1,    0.0, -1.0),   0.5, material_glass));
    // world.add(make_shared<Sphere>(Point3( 1.1,    0.0, -1.0),   0.5, material_metal));

    // 摄像机
    Point3 lookfrom(0, 1, 4); // 调整相机位置，正对墙角
    Point3 lookat(0,0,-1);
    Vec3 vup(0,1,0);
    auto dist_to_focus = (lookfrom-lookat).length();
    auto aperture = 2.0;

    Camera cam(lookfrom, lookat, vup, 35, aspect_ratio); // 稍微增大 FOV 以看到更多墙角

    // 渲染
    std::string filename = (argc >= 2) ? argv[1] : "output.ppm";
    filename = "../images/" + filename;
    std::ofstream outfile(filename);
    outfile << "P3\n" << image_width << " " << image_height << "\n255\n";

    std::cout << "用OpenMP加速,开始渲染了嗷..." << std::endl;

    // --- 原始代码 (已注释) ---
    /*
    // 创建一个缓冲区来存储像素颜色，以便并行计算
    std::vector<Color> buffer(image_width * image_height);
    int scanlines_remaining = image_height;

    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = image_height-1; j >= 0; --j) {
        // 简单的进度显示（非线程安全，但足够用）
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
            // 将计算结果存入缓冲区
            // 缓冲区索引：(image_height - 1 - j) * image_width + i
            buffer[(image_height - 1 - j) * image_width + i] = pixel_color;
        }
    }

    // 将缓冲区写入文件
    for (const auto& pixel : buffer) {
        write_color(outfile, pixel, samples_per_pixel);
    }
    */

    // --- 优化后的代码 ---
    // 优化1：使用 unsigned char 替代 Color (double)，减少 8 倍内存占用
    // 优化2：将 Gamma 校正和颜色转换提前到并行循环中，减少主线程负担
    std::vector<unsigned char> buffer(image_width * image_height * 3);
    int scanlines_remaining = image_height;

    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = image_height-1; j >= 0; --j) {
        // 简单的进度显示（非线程安全，但足够用）
        #pragma omp critical
        {
            --scanlines_remaining;
            if (scanlines_remaining % 10 == 0)
                std::cerr << "\r剩余扫描线: " << scanlines_remaining << ' ' << std::flush;
        }

        for (int i = 0; i < image_width; ++i) {
            Color pixel_color(0, 0, 0);
            for (int s = 0; s < samples_per_pixel; ++s) {
                auto u = (i + random_double()) / (image_width-1);
                auto v = (j + random_double()) / (image_height-1);
                Ray r = cam.get_ray(u, v);
                pixel_color += ray_color(r, world, max_depth);
            }
            
            // 立即进行颜色处理（Tone Mapping + Gamma 校正 + 转换）
            auto scale = 1.0 / samples_per_pixel;
            Vec3 color = pixel_color * scale;

            // 1. 应用 ACES 色调映射 (处理 HDR 高光)
            color = aces_approx(color);

            // // 2. Gamma 校正 (Gamma 近似2.0, 即开根号)
            // auto r = sqrt(color.x());
            // auto g = sqrt(color.y());
            // auto b = sqrt(color.z());

            //标准的伽马校正gamma=2.2   
            auto r = pow(color.x(), 1.0/2.2);
            auto g = pow(color.y(), 1.0/2.2);
            auto b = pow(color.z(), 1.0/2.2);

            // 计算缓冲区索引
            int index = ((image_height - 1 - j) * image_width + i) * 3;
            buffer[index] = static_cast<unsigned char>(256 * clamp(r, 0.0, 0.999));
            buffer[index+1] = static_cast<unsigned char>(256 * clamp(g, 0.0, 0.999));
            buffer[index+2] = static_cast<unsigned char>(256 * clamp(b, 0.0, 0.999));
        }
    }

    // 将缓冲区写入文件
    for (size_t i = 0; i < buffer.size(); i += 3) {
        outfile << static_cast<int>(buffer[i]) << ' '
                << static_cast<int>(buffer[i+1]) << ' '
                << static_cast<int>(buffer[i+2]) << '\n';
    }

    std::cout << "\n完事\n";
    outfile.close();
    std::cout << "ppm格式的文件已保存到 " << filename << std::endl;
    filename = (argc >= 2) ? argv[1] : "output.ppm";
    std::cout << "查看ppm文件: cd ../&& python3 read_ppm.py " << filename  << std::endl;
}
