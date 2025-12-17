#ifndef MATERIAL_H
#define MATERIAL_H

#include "utils.h"
#include "ray.h"
#include "hittable_obj.h"

// struct HitRecord;

/**
*材质基类，所有材质都应继承自此类
*材质决定了光线与物体表面交互的方式。
*@func emitted，表示材质发光（如光源材质）
*@func scatter，它决定了入射光线 (r_in) 如何被散射成新的光线 (scatteredRay)，以及光线被衰减了多少 (loss)。
*/
class Material {
public:
    //emitted: 发射光线的颜色（对于自发光材质）默认是黑色
    virtual Color emitted(double u, double v, const Point3& p) const {
        return Color(0, 0, 0);
    }

    // r_in: 入射光线
    // rec: 撞击点记录（包含位置、法线等）
    // attenuation: 颜色衰减（反射率/颜色）
    // scatteredRay: 散射后的新光线
    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scatteredRay
    ) const = 0;
};

// 漫射光源材质 (Diffuse Light)
/**
* 模拟自发光的材质，如灯光
* DiffuseLight 构造函数：传入发光颜色或颜色指针
* scatter 函数实现：光源不散射光线，只发光
* emitted 函数实现：返回发光颜色
*/
class DiffuseLight : public Material {
public:
    //传入颜色指针或者颜色值
    DiffuseLight(shared_ptr<Color> a) : emit(a) {}
    DiffuseLight(Color c) : emit(make_shared<Color>(c)) {}

    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scatteredRay
    ) const override {
        // 光源不散射光线，只发光
        return false;
    }

    virtual Color emitted(double u, double v, const Point3& p) const override {
        return *emit;
    }

public:
    shared_ptr<Color> emit;
};

// 朗伯漫反射材质 (Lambertian)
/*模拟粗糙表面，光线向各个方向随机散射
* Lambertian 构造函数：传入漫反射颜色
* scatter 函数实现：漫反射散射方向：法线方向 + 单位球内的随机向量
* 这近似了朗伯余弦定律 (Lambert's Cosine Law)
*/
class Lambertian : public Material {
public:
    //构造函数，传入漫反射颜色
    Lambertian(const Color& a) : albedo(a) {}

    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scatteredRay
    ) const override {
        // 漫反射散射方向：法线方向 + 单位球内的随机向量
        // 这近似了朗伯余弦定律 (Lambert's Cosine Law)
        auto scatter_direction = rec.normal + random_unit_vector();

        // 捕获退化散射方向（如果随机向量正好与法线相反，结果接近零）
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;

        scatteredRay = Ray(rec.p, scatter_direction);
        attenuation = albedo;
        return true;
    }

public:
    Color albedo;
};

// 金属材质 (Metal)
/*模拟光滑或粗糙的金属表面，发生镜面反射
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

// 绝缘体/电介质材质 (Dielectric)
/**  模拟玻璃、水、钻石等透明物体，发生折射和反射
*@func Dielectric 构造函数：传入折射率
*@func scatter 函数实现：根据斯涅尔定律计算折射，考虑全内反射和菲涅尔效应
*/
class Dielectric : public Material {
public:
    /*index_of_refraction: 折射率，见高中物理*/
    Dielectric(double index_of_refraction) : ir(index_of_refraction) {}

    virtual bool scatter(
        const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scatteredRay
    ) const override {
        attenuation = Color(1.0, 1.0, 1.0); // 玻璃通常不吸收光线（全透明）
        
        // 判断是进入介质还是离开介质
        // 如果是光击中前向面（从外部射入），折射率比是 1.0/ir
        // 如果是光击中背面（从内部射出），折射率比是 ir
        double refraction_ratio = rec.front_face ? (1.0/ir) : ir;

        Vec3 unit_direction = unit_vector(r_in.direction());
        
        // 计算 cos(theta) 和 sin(theta) 用于判断全内反射
        //注意这里的入射光线无论是反射还是折射，都是和法线夹钝角，所以需要取负号。
        double cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta*cos_theta);

        // 判断是否发生全内反射 (Total Internal Reflection)
        // 当光线从高折射率介质射向低折射率介质，且入射角足够大时，无法折射
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        
        Vec3 direction;

        // 菲涅尔效应 (Fresnel Effect) 近似
        // 即使可以折射，也有一部分光会被反射（如从侧面看玻璃窗）
        if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, refraction_ratio);

        scatteredRay = Ray(rec.p, direction);
        return true;
    }

private:
    double ir; // 折射率 (Index of Refraction)

    // Schlick 近似：计算菲涅尔反射比率
    static double reflectance(double cosine, double ref_idx) {
        auto r0 = (1-ref_idx) / (1+ref_idx);
        r0 = r0*r0;
        return r0 + (1-r0)*pow((1 - cosine), 5);
    }
};

#endif
