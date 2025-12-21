#ifndef BVH_SERIALIZER_H
#define BVH_SERIALIZER_H

#include "bvh.h"
#include "triangle.h"
#include <iostream>
#include <fstream>

inline void save_bvh_node(shared_ptr<HittableObj> node, std::ostream& out) {
    // Check type
    if (auto bvh = std::dynamic_pointer_cast<BvhNode>(node)) {
        // Type 0: BvhNode
        int type = 0;
        out.write((char*)&type, sizeof(int));
        
        // Write Box
        out.write((char*)&bvh->box, sizeof(aabb));
        
        // Recurse
        save_bvh_node(bvh->left, out);
        save_bvh_node(bvh->right, out);
    } else if (auto tri = std::dynamic_pointer_cast<Triangle>(node)) {
        // Type 1: Triangle
        int type = 1;
        out.write((char*)&type, sizeof(int));
        
        // Write Vertices
        out.write((char*)&tri->v0, sizeof(Point3));
        out.write((char*)&tri->v1, sizeof(Point3));
        out.write((char*)&tri->v2, sizeof(Point3));
    } else {
        // Unknown type or null
        int type = -1;
        out.write((char*)&type, sizeof(int));
    }
}

inline shared_ptr<HittableObj> load_bvh_node(std::istream& in, shared_ptr<Material> m) {
    int type;
    in.read((char*)&type, sizeof(int));
    
    if (type == 0) {
        auto node = make_shared<BvhNode>();
        in.read((char*)&node->box, sizeof(aabb));
        node->left = load_bvh_node(in, m);
        node->right = load_bvh_node(in, m);
        return node;
    } else if (type == 1) {
        Point3 v0, v1, v2;
        in.read((char*)&v0, sizeof(Point3));
        in.read((char*)&v1, sizeof(Point3));
        in.read((char*)&v2, sizeof(Point3));
        return make_shared<Triangle>(v0, v1, v2, m);
    }
    return nullptr;
}

inline bool save_bvh_to_file(const std::string& filename, shared_ptr<HittableObj> root) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    save_bvh_node(root, out);
    return true;
}

inline shared_ptr<HittableObj> load_bvh_from_file(const std::string& filename, shared_ptr<Material> m) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) return nullptr;
    return load_bvh_node(in, m);
}

#endif
