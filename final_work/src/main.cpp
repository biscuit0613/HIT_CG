#include "material.hpp"
#include "renderer_path.h"
#include "renderer_ppm.h"
#include "renderer_pm.h"
#include "vec3.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <omp.h>
#include <cstring>

// 将颜色写入流的工具函数,这玩意不方便定义在utils.h里，因为会引入循环依赖
void write_color(std::ostream &out, Color pixelColor, int samplesPerPixel)
 {
    auto r = pixelColor.x();
    auto g = pixelColor.y();
    auto b = pixelColor.z();

    // 将颜色除以样本数并进行 gamma=2.0 的伽马校正。
    auto scale = 1.0 / samplesPerPixel;
    r = sqrt(scale * r);
    g = sqrt(scale * g);
    b = sqrt(scale * b);

    // 写入每个颜色分量的转换后的 [0,255] 值。
    out << static_cast<int>(256 * clamp(r, 0.0, 0.999)) << ' '
        << static_cast<int>(256 * clamp(g, 0.0, 0.999)) << ' '
        << static_cast<int>(256 * clamp(b, 0.0, 0.999)) << '\n';
}

int main(int argc, char* argv[]) {

    std::string mode = "pt"; // 默认是路径追踪
    std::string filename = "output.ppm";
    int width = 400;
    int height = 225;
    int samples = 100;

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-m" || arg == "--mode") && i + 1 < argc) {
            mode = argv[++i];
        } else if (arg == "-o"||arg == "--out" && i + 1 < argc) {
            filename = argv[++i];
        } else if ((arg == "-w" || arg == "--width") && i + 1 < argc) {
            width = std::atoi(argv[++i]);
            height = static_cast<int>(width / (16.0/9.0));
        } else if ((arg == "-h" || arg == "--height") && i + 1 < argc) {
            height = std::atoi(argv[++i]);
            width = static_cast<int>(height * (16.0/9.0));
        } else if ((arg == "-s" || arg == "--spp") && i + 1 < argc) {
            samples = std::atoi(argv[++i]);
        }
    }
    
    if (height == 0) height = static_cast<int>(width / (16.0/9.0));

    std::cout << "光追模式(path tracing/pm/ppm): " << mode << "\n";
    std::cout << "尺寸: " << width << "x" << height << "\n";
    std::cout << "采样数(path tracing)/光子数(pm/ppm): " << samples << "\n";

    // 图像
    const auto aspect_ratio = double(width) / height;
    const int image_width = width;
    const int image_height = height;
    const int samples_per_pixel = samples; 
    const int max_depth = 50; // 递归深度

    // 世界，显而易见的世界也是一个支持光追的物体列表
    HittableObjList world;
    std::vector<shared_ptr<HittableObj>> lights; // Keep track of lights for SPPM
    
    //用指针的方式创建材质，方便多个物体共享同一个材质。
    auto material_ground = make_shared<Lambertian>(Color(0.5, 0.5, 0.5)); // 地面灰色
    auto material_cat_pic = make_shared<ImageTexture>("maodie.png"); 
    auto material_cat = make_shared<Lambertian>(material_cat_pic);

    auto material_wall_back = make_shared<Lambertian>(Color(0.7, 0.3, 0.3)); // 后墙红色
    auto material_wall_right = make_shared<Lambertian>(Color(0.3, 0.7, 0.3)); // 右墙绿色
    auto material_wall_left = make_shared<Lambertian>(Color(0.3, 0.3, 0.7)); // 左墙蓝色
    
    // auto material_center = make_shared<Lambertian>(Color(0.1, 0.2, 0.5));
    auto texture_center = make_shared<ImageTexture>("maodie.png");
    auto material_center = make_shared<Lambertian>(texture_center);

    auto material_glass = make_shared<Dielectric>(1.5); // 玻璃 
    auto color_glass = make_shared<Dielectric>(1.5,Color(0,0.5,0));//绿色玻璃
    // 增加一点粗糙度 (fuzz = 0.01) 让金属看起来更真实，不是完美的镜子
    auto material_metal  = make_shared<Metal>(Color(0.8, 0.6, 0.2), 0.01);

    auto material_light = make_shared<DiffuseLight>(Color(50.0, 50.0, 50.0)); // 光（path tracing用的）
    // auto material_light = make_shared<DiffuseLight>(Color(100.0, 100.0, 100.0)); // 更强的光（PM用的）
    //目前平面类还每实现
    // 地面
    world.add(make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, material_ground));
    // 后墙 (z = -3 左右)
    world.add(make_shared<Sphere>(Point3(0, 0, -1003), 1000, material_wall_back));
    // 左墙
    world.add(make_shared<Sphere>(Point3(-1002, 0, -1), 1000, material_wall_left));
    // 右墙
    world.add(make_shared<Sphere>(Point3( 1002, 0, -1), 1000, material_wall_right));
    //前墙
    world.add(make_shared<Sphere>(Point3(0, 0, 1005), 1000, material_wall_back));

    // 光源 (在上方)
    auto light_sphere = make_shared<Sphere>(Point3(0.8, 1.5, 0.2), 0.2, material_light);
    //这里把光源放到摄像机前面，可以直接看见光源，方便测试PM直接光照部分
    
    world.add(light_sphere);
    lights.push_back(light_sphere); // Add to lights list

    // 物体
    // world.add(make_shared<Sphere>(Point3( -0.5,    0.0, 0.5),   0.5, material_center));
    // world.add(make_shared<Sphere>(Point3( 0.0,    0.0, -1.0),   0.4, material_cat));
    world.add(make_shared<Sphere>(Point3(-0.5,    0.0, 0.2),   0.5, material_glass));
    // world.add(make_shared<Sphere>(Point3(-0.5,    0.0, 0.2),   0.5, color_glass));
    world.add(make_shared<Sphere>(Point3( 1.1,    0.0, -1.1),   0.7, material_metal));

    // 摄像机
    Point3 lookfrom(0, 1, 4); // 调整相机位置，正对墙角
    Point3 lookat(0,0,-1);
    
    // Point3 lookfrom(0, 0.5, 2.5); // 降低高度，拉近距离
    // Point3 lookat(0, -0.5, 0.5);  // 看向玻璃球下方的地面区域
    
    Vec3 vup(0,1,0);
    auto dist_to_focus = (lookfrom-lookat).length();//或许可以实现景深效果？
    auto aperture = 2.0;

    Camera cam(lookfrom, lookat, vup, 35, aspect_ratio); // 稍微增大 FOV 以看到更多墙角

    // 渲染
    std::string filename_ = "../images/" + filename;
    std::ofstream outfile(filename_);
    outfile << "P3\n" << image_width << " " << image_height << "\n255\n";

    std::vector<unsigned char> buffer;

    if (mode == "pm") {
        // PM的参数 
        int num_photons = samples * 10000; 
        double radius = 0.002; 
        render_pm(world, lights, cam, image_width, image_height, num_photons, max_depth, radius, buffer);
    } else if (mode == "ppm") {
        // PPM 参数
        int num_photons = samples * 10000; 
        double radius = 0.01; //ppm的初始半径要大，因为会不断缩减，如果一开始没有搜索到光子，后面就更难搜到了
        render_ppm(world, lights, cam, image_width, image_height, num_photons, max_depth, radius, buffer);
    } else {
        // 默认路径追踪
        render_path_tracing(world, cam, image_width, image_height, samples_per_pixel, max_depth, buffer);
    }

    // 将缓冲区写入文件
    for (size_t i = 0; i < buffer.size(); i += 3) {
        outfile << static_cast<int>(buffer[i]) << ' '
                << static_cast<int>(buffer[i+1]) << ' '
                << static_cast<int>(buffer[i+2]) << '\n';
    }
    outfile.close();

    std::cout << "ppm格式的文件已保存到 " << filename << std::endl;
    std::cout << "查看ppm文件: cd ../&& python3 read_ppm.py " << filename  << std::endl;
}
