#ifndef RAY_H
#define RAY_H

#include "vec3.h"

// 光线类
/**
*对于每一个光线对象
*@param orig 光线的起点
*@param dir  光线的方向
*@brief at(t) 返回光线在参数 t 处的位置
*/
class Ray {
public:
    Ray() {}
    Ray(const Point3& origin, const Vec3& direction)
        : orig(origin), dir(direction) {}

    Point3 origin() const  { return orig; }
    Vec3 direction() const { return dir; }

    Point3 at(double t) const {
        return orig + t*dir;//光沿直线传播
    }

public:
    Point3 orig;
    Vec3 dir;
};

#endif
