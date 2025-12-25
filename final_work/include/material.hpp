#ifndef MATERIAL_H
#define MATERIAL_H

#include "utils.h"
#include "ray.h"
#include "hittable_obj.h"
#include "texture.hpp"

// struct HitRecord;

/**
*材质基类，所有材质都应继承自此类
*材质决定了光线与物体表面交互的方式。
*@func emitted，表示材质发光（如光源材质）
*@func scatter，它决定了入射光线 (r_in) 如何变成成新的光线 (scatteredRay)，以及光线被衰减了多少 (attenuation或albedo)。
*/
class Material {
public:
    //emitted: 发射光线的颜色（对于自发光材质）,默认是黑色
    //这里用u,v参数是为了和纹理接口统一
    virtual Color emitted(double u, double v, const Point3& p) const {
        return Color(0, 0, 0);
    }

    // r_in: 入射光线
    // rec: 撞击点记录（包含位置、法线等）
    // attenuation: 颜色衰减（反射率/颜色）
    // scatteredRay: 交互后的新光线
    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scatteredRay
    ) const = 0;
};

/**
* 漫射光源材质 (Diffuse Light)模拟自发光的材质，如灯光
* DiffuseLight 构造函数：传入发光颜色或颜色指针
* scatter 函数实现：r_in碰到光源不进行互动，光源只会主动发光
* emitted 函数实现：返回发光颜色
*/
class DiffuseLight : public Material {
public:
    //传入颜色指针或者颜色值
    DiffuseLight(shared_ptr<Texture> a) : emit(a) {}
    DiffuseLight(Color c) : emit(make_shared<SolidColor>(c)) {}

    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scatteredRay) const override {
        return false; // 光源不散射光线，只发光
    }

    virtual Color emitted(double u, double v, const Point3& p) const override {
        return emit->value(u, v, p);
    }

public:
    shared_ptr<Texture> emit;
};


/*漫反射材质 (Lambertian)模拟粗糙表面，光线向各个方向随机散射
* Lambertian 构造函数：传入漫反射颜色
* scatter 函数实现：漫反射散射方向：法线方向 + 单位球内的随机向量
* 这近似了朗伯余弦定律 (Lambert's Cosine Law)
*/
class Lambertian : public Material {
public:
    //构造函数，传入漫反射颜色
    Lambertian(const Color& a) : albedo(make_shared<SolidColor>(a)) {}
    Lambertian(shared_ptr<Texture> a) : albedo(a) {}

    virtual bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scatteredRay) const override {
        // 漫反射散射方向：法线方向 + 单位球内的随机向量
        // 这近似了朗伯余弦定律 (Lambert's Cosine Law)
        auto scatter_direction = rec.normal + random_unit_vector();

        // 捕获退化散射方向（如果随机向量正好与法线相反，结果接近零）
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;

        scatteredRay = Ray(rec.p, scatter_direction);
        attenuation = albedo->value(rec.u, rec.v, rec.p);
        return true;
    }

public:
    shared_ptr<Texture> albedo;
};


/*金属材质 (Metal)模拟光滑或粗糙的金属表面，发生镜面反射
*@func 构造函数：传入颜色和模糊因子f
*@func scatter 函数实现：反射向量+加模糊因子 (fuzz)
*/
class Metal : public Material {
public:
    //传入颜色和模糊因子f
    Metal(const Color& a, double f) : albedo(a), fuzz(f < 1 ? f : 1) {}

    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scatteredRay
    ) const override {
        // 计算反射向量，直接用reflect函数
        Vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
        
        // 加上模糊因子 (fuzz)
        scatteredRay = Ray(rec.p, reflected + fuzz * random_in_unit_sphere());
        attenuation = albedo;
        
        // 只有当散射光线与法线在同一侧时才算有效反射
        return (dot(scatteredRay.direction(), rec.normal) > 0);
    }

public:
    Color albedo;
    double fuzz;// 模糊因子，范围 [0, 1]
};

/**  绝缘体/电介质材质 (Dielectric)模拟玻璃，发生折射和反射
*@func Dielectric 构造函数：传入折射率，吸光度
*@func scatter 函数实现：根据斯涅尔定律计算折射，考虑全内反射和菲涅尔效应
*/
class Dielectric : public Material {
public:
    Dielectric(double index_of_refraction, Color absorb = Color(0,0,0)) //absorb用于Beer's Law
        : ir(index_of_refraction), absorbance(absorb) {}//ir: 折射率之比

    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scatteredRay
    ) const override {       
        // 啤酒瓶定律（Beer's Law），这个对于有色玻璃才有用，我的透明玻璃球相当于else分支
        // 如果光线在介质内部传播 (!rec.front_face)，则根据距离衰减
        if (!rec.front_face) {
            // 距离是 rec.t，光线在介质中的传播距离
            double r = exp(-absorbance.x() * rec.t);
            double g = exp(-absorbance.y() * rec.t);
            double b = exp(-absorbance.z() * rec.t);
            attenuation = Color(r, g, b);
        } else {//光不穿过介质就不被吸收
            attenuation = Color(1.0, 1.0, 1.0);
        }
        // 如果是光击中前向面（从外部射入），折射率比是 1.0/ir
        // 如果是光击中背面（从内部射出），折射率比是 ir
        double refr_ratio = rec.front_face ? (1.0/ir) : ir;

        Vec3 unit_dir = unit_vector(r_in.direction());//入射光线单位向量
        // 计算 cos(theta) 和 sin(theta) 用于判断全内反射
        //注意这里的入射光线无论是反射还是折射，都是和法线夹钝角，所以unit_dir需要取负号。
        double cos_theta = fmin(dot(-unit_dir, rec.normal), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta*cos_theta);
        // 判断是否发生全内反射,当光线从高折射率介质射向低折射率介质，且入射角足够大时，无法折射 
        bool cannot_refract = refr_ratio * sin_theta > 1.0;
        Vec3 direction;
        // 菲涅尔效应 (Fresnel Effect) 近似
        //使用重要性采样 (Importance Sampling) ,参考 smallpt，人为增加反射的采样概率，然后通过权重补偿
        double refl_prob = reflectance_schlick(cos_theta, refr_ratio);//用Schlick近似计算反射率
        if (cannot_refract) {
            direction = reflect(unit_dir, rec.normal);
        }
        else {
            // 保证至少有 25% 的概率采样反射
            double P = 0.25 + 0.5 * refl_prob; 
            double RP = refl_prob / P;  // 反射路径的权重补偿
            double TP = (1.0 - refl_prob) / (1.0 - P); // 折射路径的权重补偿
            if (random_double() < P) {//俄罗斯轮盘赌选择反射还是折射，这里和smallpt不一样
                direction = reflect(unit_dir, rec.normal);
                attenuation = attenuation * RP;
            } else {
                direction = refract(unit_dir, rec.normal, refr_ratio);
                attenuation = attenuation * TP;
            }
        }
        scatteredRay = Ray(rec.p, direction);
        return true;
    }

public:
    double ir; // 折射率之比 (Index of Refraction)
    Color absorbance; // 吸光度，用于 Beer's Law
    // Schlick 近似：计算菲涅尔反射率
    static double reflectance_schlick(double cos, double ref_idx) {
        auto r0 = (1-ref_idx) / (1+ref_idx);
        r0 = r0*r0;
        return r0 + (1-r0)*pow((1 - cos), 5);
    }
};

#endif
