#ifndef HITTABLE_H
#define HITTABLE_H

#include "ray.h"
#include "utils.h"
#include "aabb.h"

class Material;

/**
* 结构体，用于存储光线与物体相交时的信息
*@param p         相交点位置
*@param normal    相交点处的法线
*@param mat_ptr   相交点处的材质指针
*@param t         光线参数 t，在光线方程 P(t) = origin + t*direction 中,一开始是无穷大。
*@param front_face 布尔值，指示光线是否击中物体的前面
*@brief set_face_normal(r, outward_normal) 根据光线方向和外法线设置 front_face 和 normal
*/
struct HitRecord {
    Point3 p;
    Vec3 normal;
    shared_ptr<Material> mat_ptr;
    double t;
    double u;
    double v;
    bool front_face;
    /**
    光线与物体相交时，设置法线方向和前后面标志
    *@param & r 入射光线
    *@param & outward_normal 物体表面的外法线
    */
    inline void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        //如果光线方向和外法线的点积小于0，说明光线击中了物体的前面
        front_face = dot(r.direction(), outward_normal) < 0;
        //如果光线击中了物体的前面，法线保持不变，否则取反
        normal = front_face ? outward_normal : -outward_normal;
    }
};

/** 
* HittableObj 类，所有能发生光追的物体都应继承自此类
*@brief hit(r, t_min, t_max, rec) 判断光线 r 是否与物体相交
*/
class HittableObj {
public:
    /** 
    *hit 函数，判断光线与物体是否相交
    *@param & r 入射光线
    *@param t_min 最小 t 值，防止自相交
    *@param t_max 最大 t 值
    *@param & rec 用于存储相交信息的 HitRecord 结构体
    *@return 如果光线与物体相交，返回 true 并填充 rec，否则返回 false
    */
    virtual bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const = 0;

    virtual bool bounding_box(double time0, double time1, aabb& output_box) const = 0;
    //hit函数不应该在这里实现，因为每个具体的物体都有不同的相交逻辑，如果在这里实现就失去了多态性。
};

#endif
