#include "vec3.h"
#include "ray.h"
#include "hittable_obj.h"
#include "material.hpp"

/** 
平面的方程：ax + by + cz + d = 0
其中，(a, b, c) 是平面的法向量，d 是常数。
*/
class Plane {
public:
    Plane(const Point3& point, const Vec3& normal, shared_ptr<Material> m)
        : point_(point), normal_(unit_vector(normal)), mat_ptr(m) {
        d_ = -dot(normal_, point_);
    }

    virtual bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
        auto denom = dot(normal_, r.direction());
        if (fabs(denom) > 1e-6) { // 避免与平行光线相交
            auto t = -(dot(normal_, r.origin()) + d_) / denom;
            if (t < t_max && t > t_min) {
                rec.t = t;
                rec.p = r.at(t);
                rec.set_face_normal(r, normal_);
                rec.mat_ptr = mat_ptr;
                return true;
            }
        }
        return false;
    }
private:
    Point3 point_; // 平面上的一点
    Vec3 normal_;  // 平面的法向量
    double d_;     // 平面方程中的常数项
    shared_ptr<Material> mat_ptr; // 材质指针
};