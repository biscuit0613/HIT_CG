#ifndef MESH_LOADER_H
#define MESH_LOADER_H

#include "triangle.h"
#include "hittable_list.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

inline shared_ptr<HittableObjList> load_obj(std::string filename, shared_ptr<Material> m, double scale, Point3 offset) {
    std::vector<Point3> vertices;
    auto objects = make_shared<HittableObjList>();

    std::ifstream in(filename);
    if (!in.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return objects;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.substr(0, 2) == "v ") {
            std::istringstream s(line.substr(2));
            double x, y, z;
            s >> x >> y >> z;
            vertices.push_back(Point3(x * scale, y * scale, z * scale) + offset);
        } else if (line.substr(0, 2) == "f ") {
            std::istringstream s(line.substr(2));
            std::string segment;
            std::vector<int> face_indices;
            while (std::getline(s, segment, ' ')) {
                if (segment.empty()) continue;
                // Handle v/vt/vn or v//vn or v
                size_t first_slash = segment.find('/');
                int idx;
                if (first_slash != std::string::npos) {
                    idx = std::stoi(segment.substr(0, first_slash));
                } else {
                    try {
                        idx = std::stoi(segment);
                    } catch (...) {
                        continue;
                    }
                }
                face_indices.push_back(idx - 1); // OBJ is 1-based
            }
            if (face_indices.size() >= 3) {
                // Simple triangulation for polygons
                for (size_t i = 1; i < face_indices.size() - 1; ++i) {
                    objects->add(make_shared<Triangle>(
                        vertices[face_indices[0]],
                        vertices[face_indices[i]],
                        vertices[face_indices[i+1]],
                        m
                    ));
                }
            }
        }
    }
    std::cerr << "Loaded " << objects->objects.size() << " triangles from " << filename << std::endl;
    return objects;
}

#endif
