#ifndef HITTABLE_LIST_H
#define HITTABLE_LIST_H

#include "hittable_obj.h"

#include <memory>
#include <vector>

using std::shared_ptr;
using std::make_shared;

/**
* HittableObjList 类继承自 HittableObj，管理多个 HittableObj 对象的集合
* 在这里面实现了 hit 函数，遍历所有对象以检测光线与场景中任意物体的相交情况
*/
class HittableObjList : public HittableObj {
public:
    HittableObjList() {}
    //带参构造函数，初始化时添加一个支持光追的物体
    HittableObjList(shared_ptr<HittableObj> object) { add(object); }

public:
    std::vector<shared_ptr<HittableObj>> objects;// 存储支持光追的物体列表，每一个元素都是指向对象的智能指针。
    void clear() { objects.clear(); }
    //add 方法，向列表中添加一个支持光追的物体
    void add(shared_ptr<HittableObj> object) { objects.push_back(object); }

    virtual bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const override;
    virtual bool bounding_box(double time0, double time1, aabb& output_box) const override;
};

//迭代检测光线与场景中所有物体的相交情况，这里还不涉及反射，只是检测相交，处理遮挡。t就是光线的参数。
/**
*@param & r 入射光线
*@param t_min 最小 t 值，防止自相交
*@param t_max 最大 t 值 ，一开始是infty,会随着击中物体的距离变小而更新
*@param & rec 用于存储相交信息的 HitRecord 结构体
*@return 如果光线与任意物体相交，返回 true 并填充
*/
bool HittableObjList::hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
    HitRecord temp_rec;
    bool hitFirstObj = false;//是否击中第一个物体 
    double closest2Camera = t_max;
    //注意这里传入的是引用，减小内存开销，这个在物体多的时候巨慢。
    for (const auto& object : objects) {
        if (object->hit(r, t_min, closest2Camera, temp_rec)) {
            hitFirstObj = true;
            closest2Camera = temp_rec.t;
            rec = temp_rec;
        }
    }

    return hitFirstObj;
}

bool HittableObjList::bounding_box(double time0, double time1, aabb& output_box) const {
    if (objects.empty()) return false;

    aabb temp_box;
    bool first_box = true;

    for (const auto& object : objects) {
        if (!object->bounding_box(time0, time1, temp_box)) return false;
        output_box = first_box ? temp_box : surrounding_box(output_box, temp_box);
        first_box = false;
    }

    return true;
}

#endif
