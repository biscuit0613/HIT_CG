#ifndef SPHERE_H
#define SPHERE_H

#include "hittable_obj.h"
#include "vec3.h"


// 球体类，继承自 HittableObj
/*** Sphere 类表示三维空间中的一个球体
*@param center 球心位置
*@param radius 球的半径
*@param mat_ptr 指向球体材质的智能指针
*@func hit(r, t_min, t_max, rec) 判断光线 r 是否与球体相交
*/
class Sphere : public HittableObj {
public:
    Sphere() {}
    Sphere(Point3 cen, double r, shared_ptr<Material> m)
        : center(cen), radius(r), mat_ptr(m) {};

    virtual bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override;

    virtual bool bounding_box(double time0, double time1, aabb& output_box) const override;

private:
    static void get_sphere_uv(const Point3& p, double& u, double& v) {
        // p: a given point on the sphere of radius one, centered at the origin.
        // u: returned value [0,1] of angle around the Y axis from X=-1.
        // v: returned value [0,1] of angle from Y=-1 to Y=+1.
        //     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
        //     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
        //     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>

        auto theta = acos(-p.y());
        auto phi = atan2(-p.z(), p.x()) + pi;

        u = phi / (2*pi);
        v = theta / pi;
    }

public:
    Point3 center;
    double radius;
    shared_ptr<Material> mat_ptr;
};
// 光线与球体相交的实现
bool Sphere::hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
    Vec3 o2c = r.origin() - center;//光线原点指向球心的向量
    auto a = r.direction().length_squared();//光线方向向量的长度平方
    auto half_b = dot(o2c, r.direction());
    auto c = o2c.length_squared() - radius*radius;

    auto Delta = half_b*half_b - a*c;//判别式
    if (Delta < 0) return false;
    auto sqrtd = sqrt(Delta);

    // 找到位于可接受范围内的最近的根。
    auto root = (-half_b - sqrtd) / a;
    if (root < t_min || root > t_max) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || root > t_max)
            return false;
    }
    //把交点信息存入 hitRecord 结构体里面
    rec.t = root;
    rec.p = r.at(rec.t);//计算交点
    Vec3 outward_normal = (rec.p - center) / radius;
    rec.set_face_normal(r, outward_normal);
    get_sphere_uv(outward_normal, rec.u, rec.v);
    rec.mat_ptr = mat_ptr;

    return true;
}

bool Sphere::bounding_box(double time0, double time1, aabb& output_box) const {
    output_box = aabb(
        center - Vec3(radius, radius, radius),
        center + Vec3(radius, radius, radius));
    return true;
}

#endif
