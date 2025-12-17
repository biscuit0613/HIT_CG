#ifndef UTILS_H
#define UTILS_H

#include <cmath>
#include <limits>
#include <memory>
#include <random>


// Usings

using std::shared_ptr;
using std::make_shared;
using std::sqrt;

// 常量

const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.1415926535897932385;

// 工具函数

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

inline double random_double() {
    // 使用 thread_local 保证多线程安全，每个线程都有自己的随机数生成器实例，对于光追这种高度并行的任务尤为重要。
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(generator);
}

inline double random_double(double min, double max) {
    // 返回一个 [min,max) 之间的随机实数。
    return min + (max-min)*random_double();
}

inline double clamp(double x, double min, double max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

#endif
