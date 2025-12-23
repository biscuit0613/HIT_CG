#ifndef RENDERER_COMMON_H
#define RENDERER_COMMON_H

#include "utils.h"
#include "hittable_list.hpp"
#include "material.hpp"
#include "sphere.h"
#include <algorithm>
#include <vector>

/**
为了方便光子映射那几个算法获取材质和求交，把公共的功能提出来了
*/
// 材质类型枚举
enum Refl_t { DIFF, SPEC, REFR };

// 辅助函数：获取向量的最大分量
inline double max_in_xyz(const Vec3& v) {
    return std::max({v.x(), v.y(), v.z()});
}

// 射线求交，和hittableobj里的hit不一样，这里返回击中的物体索引和距离
//有了索引就可以通过world.objects[索引]来获取具体的物体指针，然后再用它的hit函数获取HitRecord
inline std::pair<int, double> nearest_hit(const Ray& ray, const HittableObjList& world) {
    double closest_so_far = infinity;
    int hit_idx = -1;
    HitRecord temp_rec;
    
    for (int i = 0; i < world.objects.size(); ++i) {
        if (world.objects[i]->hit(ray, 0.001, closest_so_far, temp_rec)) {
            closest_so_far = temp_rec.t;
            hit_idx = i;
        }
    }
    
    if (hit_idx != -1) return {hit_idx, closest_so_far};
    return {-1, 0};
}

// 辅助函数：返回一个二元组：材质类型和颜色
inline std::pair<Refl_t, Color> get_feature(shared_ptr<Material> mat, const Point3& p) {
    Color f(0,0,0);
    if (!mat) return {DIFF, f};
    
    if (auto lam = std::dynamic_pointer_cast<Lambertian>(mat)) {
        f = lam->albedo->value(0,0,p);//f就是albedo
        return {DIFF, f};
    }
    if (auto met = std::dynamic_pointer_cast<Metal>(mat)) {
        f = met->albedo;
        return {SPEC, f};
    }
    if (auto diel = std::dynamic_pointer_cast<Dielectric>(mat)) {
        f = Color(1,1,1); 
        return {REFR, f};
    }
    if (auto light = std::dynamic_pointer_cast<DiffuseLight>(mat)) {
        f = light->emit->value(0,0,p);
        return {DIFF, f};
    }
    return {DIFF, f};
}

#endif