#ifndef CAMERA_H
#define CAMERA_H

#include "utils.h"
#include "ray.h"
#include "vec3.h"


/**
相机类
* @param lookfrom 相机位置
* @param lookat   相机指向的目标点
* @param vup      相机的上向量
* @param vfov     垂直视野 (vertical field-of-view)，以度为单位
* @param aspect_ratio  图像的宽高比
* @func get_ray(s, t) 生成从相机发出的光线，s 和 t 是归一化的图像平面坐标
*/
class Camera {
public:
    Camera(
        Point3 lookfrom,
        Point3 lookat,
        Vec3   vup,
        double vfov, // 垂直视野 (vertical field-of-view)，以度为单位
        double aspect_ratio
    ) {
        auto theta = degrees_to_radians(vfov);
        auto h = tan(theta/2);
        auto viewport_height = 2.0 * h;
        auto viewport_width = aspect_ratio * viewport_height;

        auto w = unit_vector(lookfrom - lookat);
        auto u = unit_vector(cross(vup, w));
        auto v = cross(w, u);

        origin = lookfrom;
        horizontal = viewport_width * u;
        vertical = viewport_height * v;
        lower_left_corner = origin - horizontal/2 - vertical/2 - w;
    }

    Ray get_ray(double s, double t) const {
        return Ray(origin, lower_left_corner + s*horizontal + t*vertical - origin);
    }

private:
    Point3 origin;
    Point3 lower_left_corner;
    Vec3 horizontal;
    Vec3 vertical;
};

#endif
