#ifndef TEXTURE_H
#define TEXTURE_H

#include "utils.h"
#include "vec3.h"
#include "stb_image.h"

/**
* 纹理基类，所有纹理类型都应继承自此类
*@brief value(u, v, p) 根据纹理坐标 (u, v) 和位置 p 返回颜色值
*/
class Texture {
public:
    virtual Color value(double u, double v, const Point3& p) const = 0;
};


/**
* 纯色纹理类，表示单一颜色的纹理
* @brief value重载：始终返回相同的颜色值，和uv无关
*/
class SolidColor : public Texture {
public:
    SolidColor() {}
    SolidColor(Color c) : color_value(c) {}

    virtual Color value(double u, double v, const Point3& p) const override {
        return color_value;
    }

private:
    Color color_value;
};

/** 图像纹理类，基于图像文件的纹理
* @brief value重载：根据纹理坐标 (u, v) 返回对应的图像颜色值
*/
class ImageTexture : public Texture {
public:
    const static int bytes_per_pixel = 3;

    ImageTexture()
      : data(nullptr), width(0), height(0), bytes_per_scanline(0) {}

    ImageTexture(const char* filename) {
        auto components_per_pixel = bytes_per_pixel;

        data = stbi_load(
           ("../textures/" + std::string(filename)).c_str() , &width, &height, &components_per_pixel, components_per_pixel);

        if (!data) {
            std::cerr << "打不开'" << filename << "'.\n";
            width = height = 0;
        }

        bytes_per_scanline = bytes_per_pixel * width;
    }

    ~ImageTexture() {
        //检查data是否为空，避免重复释放内存
        if (data) stbi_image_free(data);
    }

    virtual Color value(double u, double v, const Point3& p) const override {
        // 如果纹理数据不存在，返回红色作为错误指示
        if (data == nullptr)
            return Color(1, 0, 1);

        // 把u和v限制在[0,1]范围内
        u = clamp(u, 0.0, 1.0);
        v = 1.0 - clamp(v, 0.0, 1.0);  // 翻转V以匹配图像坐标

        auto i = static_cast<int>(u * width);
        auto j = static_cast<int>(v * height);

        // 限制整数映射，因为实际坐标应该小于1.0
        if (i >= width)  i = width - 1;
        if (j >= height) j = height - 1;

        const double color_scale = 1.0 / 255.0;
        auto pixel = data + j*bytes_per_scanline + i*bytes_per_pixel;

        return Color(color_scale*pixel[0], color_scale*pixel[1], color_scale*pixel[2]);
    }

private:
    unsigned char *data;
    int width, height;
    int bytes_per_scanline;
};

#endif
