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

// 通用 KD-Tree 节点
template<typename T>
struct KDNode {
    T* data;
    KDNode *left = nullptr;
    KDNode *right = nullptr;
    Point3 min_box, max_box; // 节点的包围盒 (AABB)

    KDNode(T* d) : data(d), min_box(d->p), max_box(d->p) {}
    KDNode() : data(nullptr), min_box(Point3(infinity, infinity, infinity)), max_box(Point3(-infinity, -infinity, -infinity)) {}
};

// 通用 KD-Tree 实现
// T 必须包含 Point3 p 成员
template<typename T>
class KDTree {
public:
    // 构造函数：传入对象列表（注意：KDTree 存储的是对象的指针，请确保对象在 Tree 生命周期内有效）
    KDTree(std::vector<T>& items) {
        std::vector<T*> item_ptrs;
        item_ptrs.reserve(items.size());
        for (auto& item : items) item_ptrs.push_back(&item);
        root = build_recursive(item_ptrs, 0, item_ptrs.size(), 0);
    }

    ~KDTree() {
        delete_tree(root);
    }

    // 搜索函数：查找距离 p 在 radius 范围内的所有对象，并对每个对象调用 callback
    // Callback 签名: void(T* item, double dist_sq)
    template<typename Func>
    void search(const Point3& p, double radius, Func callback) const {
        search_recursive(root, p, radius * radius, callback);
    }

private:
    KDNode<T>* root;

    void delete_tree(KDNode<T>* node) {
        if (!node) return;
        delete_tree(node->left);
        delete_tree(node->right);
        delete node;
    }

    KDNode<T>* build_recursive(std::vector<T*>& points, int start, int end, int depth) {
        if (start >= end) return nullptr;

        int axis = depth % 3;
        int mid = (start + end) / 2;

        std::nth_element(points.begin() + start, points.begin() + mid, points.begin() + end,
            [axis](T* a, T* b) {
                return a->p[axis] < b->p[axis];
            });

        KDNode<T>* node = new KDNode<T>(points[mid]);
        
        // 计算包围盒
        node->min_box = Point3(infinity, infinity, infinity);
        node->max_box = Point3(-infinity, -infinity, -infinity);
        
        for (int i = start; i < end; ++i) {
            Point3 p = points[i]->p;
            for (int k = 0; k < 3; ++k) {
                if (p[k] < node->min_box[k]) node->min_box[k] = p[k];
                if (p[k] > node->max_box[k]) node->max_box[k] = p[k];
            }
        }

        node->left = build_recursive(points, start, mid, depth + 1);
        node->right = build_recursive(points, mid + 1, end, depth + 1);

        return node;
    }

    template<typename Func>
    void search_recursive(KDNode<T>* node, const Point3& p, double radius_sq, Func callback) const {
        if (!node) return;

        // 剪枝：计算查询点到节点包围盒的最小距离平方
        double dist_sq_box = 0;
        for (int i = 0; i < 3; ++i) {
            if (p[i] < node->min_box[i]) dist_sq_box += (node->min_box[i] - p[i]) * (node->min_box[i] - p[i]);
            else if (p[i] > node->max_box[i]) dist_sq_box += (p[i] - node->max_box[i]) * (p[i] - node->max_box[i]);
        }

        // 如果查询点到包围盒的距离超过了搜索半径，则该节点及其子节点都不可能包含符合条件的对象
        if (dist_sq_box > radius_sq) return;

        // 检查当前节点的对象
        double dist_sq = (node->data->p - p).length_squared();
        if (dist_sq <= radius_sq) {
            callback(node->data, dist_sq);
        }

        search_recursive(node->left, p, radius_sq, callback);
        search_recursive(node->right, p, radius_sq, callback);
    }
};

#endif