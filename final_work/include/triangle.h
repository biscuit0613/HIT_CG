#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "hittable_obj.h"
#include "vec3.h"

/**
*三角形类，继承自 HittableObj，用于模型的表示和光线相交计算
*@param v0,v1,v2 三角形的三个顶点
*@param mp 指向三角形材质的智能指针
*@brief hit(r, t_min, t_max, rec) MT算法，判断光线 r 是否与三角形相交
*/
class Triangle : public HittableObj {
public:
    Triangle() {}
    Triangle(Point3 v0, Point3 v1, Point3 v2, shared_ptr<Material> m)
        : v0(v0), v1(v1), v2(v2), mp(m) {};

    // 光线与三角形相交的实现（Möller–Trumbore算法）
    /** 
    *@param  r 入射光线
    *@param  t_min 最小 t 值，防止自相交
    *@param  t_max 最大 t 值
    *@param  rec 用于存储相交信息的 HitRecord 结构体
    *@return 如果光线与三角形相交，返回 true 并填充 rec，否则返回 false
    */
    virtual bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override {
        Vec3 v0v1 = v1 - v0;
        Vec3 v0v2 = v2 - v0;
        Vec3 pvec = cross(r.direction(), v0v2);
        double det = dot(v0v1, pvec);

        // culling
        if (fabs(det) < 1e-8) return false;

        double invDet = 1.0 / det;

        Vec3 tvec = r.origin() - v0;
        double u = dot(tvec, pvec) * invDet;
        if (u < 0 || u > 1) return false;

        Vec3 qvec = cross(tvec, v0v1);
        double v = dot(r.direction(), qvec) * invDet;
        if (v < 0 || u + v > 1) return false;

        double t = dot(v0v2, qvec) * invDet;

        if (t < t_min || t > t_max) return false;

        rec.t = t;
        rec.p = r.at(t);
        rec.set_face_normal(r, unit_vector(cross(v0v1, v0v2))); 
        rec.mat_ptr = mp;
        rec.u = u;
        rec.v = v;

        return true;
    }

    virtual bool bounding_box(double time0, double time1, aabb& output_box) const override {
        double min_x = fmin(v0.x(), fmin(v1.x(), v2.x()));
        double min_y = fmin(v0.y(), fmin(v1.y(), v2.y()));
        double min_z = fmin(v0.z(), fmin(v1.z(), v2.z()));

        double max_x = fmax(v0.x(), fmax(v1.x(), v2.x()));
        double max_y = fmax(v0.y(), fmax(v1.y(), v2.y()));
        double max_z = fmax(v0.z(), fmax(v1.z(), v2.z()));

        output_box = aabb(
            Point3(min_x - 0.0001, min_y - 0.0001, min_z - 0.0001),
            Point3(max_x + 0.0001, max_y + 0.0001, max_z + 0.0001));
        return true;
    }

public:
    Point3 v0, v1, v2;
    shared_ptr<Material> mp;
};

#endif
