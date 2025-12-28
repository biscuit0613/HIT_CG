cmake_minimum_required(VERSION 3.10)
project(RayTracer)

set(CMAKE_CXX_STANDARD 17)

# 开启编译器优化 (-O3)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

include_directories(include)
include_directories(.)

find_package(OpenMP)

add_executable(RayTracer src/main.cpp)
