#pragma once
#include <cmath>
#include "geometry.h"
#include "ray.h"
#include "material.h"
#include "random_utils.h"

class Object
{
  public:
    Material material;
    virtual bool ray_intersect(const Ray &ray, float &t, Vec3f &normal) const = 0;
    virtual ~Object() {}
};

class Sphere : public Object
{
    // svaka sfera definirana je sa centrom i polumjerom
    float r;
    Vec3f c;

  public:
    Sphere(const Vec3f &c, const float &r) : c(c), r(r) { }
    Sphere(const Vec3f &c, const float &r, const Material &mat) : c(c), r(r)
    {
        Object::material = mat;
    }

    // racunamo postoji li presjek zrake i sfere, ako postoji racunamo udaljenost od kamere do presjeka
    // i normalu na povrsini sfere u tom presjeku
    //
    // zraka: P(t) = O + t*D
    // sfera: |P - C|^2 = r^2
    // uvrstimo P(t) u jednadzbu sfere -> dobijemo kvadratnu jednadzbu po t:
    //   a*t^2 + b*t + c_coef = 0
    //   a = D . D
    //   b = 2 * (O - C) . D
    //   c_coef = (O - C).(O - C) - r^2
    bool ray_intersect(const Ray &ray, float &t, Vec3f &normal) const
    {
        Vec3f oc = ray.origin - c;

        float a = ray.direction * ray.direction;
        float b = 2.f * (oc * ray.direction);
        float c_coef = oc * oc - r * r;

        float discriminant = b * b - 4.f * a * c_coef;

        // negativna diskriminanta znaci da nema presjeka
        if (discriminant < 0.f)
            return false;

        float sqrt_disc = std::sqrt(discriminant);

        // rjesenja kvadratne jednadzbe (t1 je blizi presjek, t2 dalji - jer je -sqrt < +sqrt)
        float t1 = (-b - sqrt_disc) / (2.f * a);
        float t2 = (-b + sqrt_disc) / (2.f * a);

        const float eps = 1e-4f;

        // zelimo najblizi presjek koji je ISPRED kamere (t > eps)
        // ako je t1 iza kamere (npr. kamera je unutar sfere), probamo t2
        if (t1 > eps)
        {
            t = t1;
        }
        else if (t2 > eps)
        {
            t = t2;
        }
        else
        {
            return false; // oba presjeka su iza zrake, sfera je "iza nas"
        }

        Vec3f hit_point = ray.origin + ray.direction * t;

        // jedinicna normala na povrsini sfere u presjeku
        // ne dijelimo eksplicitno s r jer normalize() to odradi (dijeli s normom vektora)
        normal = hit_point - c;
        normal.normalize();

        return true;
    }

    Vec3f get_center() const { return c; }
    float get_radius() const { return r; }

    // Uzorkovanje smjera prema ovoj sferi kad je ona izvor svjetla
    // ("cone sampling"). Umjesto da nasumicno gadjamo cijelu polusferu
    // i cekamo da slucajno pogodimo malu svjetlecu kuglu (sto je rijetko
    // i uzrokuje "fireflies" - rijetke, jako svijetle tocke koje ne
    // nestaju ni pri jako velikom broju uzoraka), ovdje eksplicitno
    // biramo smjer UNUTAR stošca koji tocno gleda u sferu iz tocke
    // 'from'. To je standardna tehnika ("Next Event Estimation") koja
    // drasticno smanjuje varijancu za male/jake izvore svjetla.
    void sample_cone(const Vec3f &from, Vec3f &out_dir, float &out_pdf) const
    {
        Vec3f w = c - from;
        float dist = w.norm();
        w.normalize();

        float sin_theta_max2 = std::min(1.0f, (r * r) / (dist * dist));
        float cos_theta_max = std::sqrt(std::max(0.0f, 1.0f - sin_theta_max2));

        float r1 = random_float();
        float r2 = random_float();

        float cos_theta = 1.0f - r1 * (1.0f - cos_theta_max);
        float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));
        float phi = 2.0f * PI_F * r2;

        Vec3f tangent, bitangent;
        build_onb(w, tangent, bitangent);

        Vec3f local_dir(sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta);
        out_dir = tangent * local_dir.x + bitangent * local_dir.y + w * local_dir.z;
        out_dir.normalize();

        // uniformna gustoca po prostornom kutu unutar stošca
        out_pdf = 1.0f / (2.0f * PI_F * (1.0f - cos_theta_max));
    }
};

// Beskonacna ravnina - koristimo je za "pod" umjesto ogromne sfere
// radijusa 10000. Ovo NIJE bio glavni uzrok "trokutastih" artefakata
// (to smo empirijski provjerili - pravi uzrok su fireflies, vidi
// objasnjenje uz Scene::cast_ray), ali giant-sphere trik ostaje rizican
// u float (single precision) aritmetici jer kvadratna jednadzba tada
// radi s brojevima reda velicine 10^8, sto moze izazvati gubitak
// preciznosti (catastrophic cancellation). Plane je numericki potpuno
// stabilan (linearna jednadzba, bez velikih brojeva), pa je ionako
// bolji izbor za pod.
class Plane : public Object
{
    Vec3f point;  // bilo koja tocka na ravnini
    Vec3f normal; // normala ravnine (normalizirana)

  public:
    Plane(const Vec3f &point, const Vec3f &normal) : point(point), normal(normal)
    {
        this->normal.normalize();
    }
    Plane(const Vec3f &point, const Vec3f &normal, const Material &mat) : point(point), normal(normal)
    {
        this->normal.normalize();
        Object::material = mat;
    }

    // jednadzba ravnine: (P - point) . normal = 0
    // uvrstimo zraku P(t) = O + t*D:
    //   (O + t*D - point) . normal = 0
    //   t = (point - O) . normal / (D . normal)
    bool ray_intersect(const Ray &ray, float &t, Vec3f &out_normal) const
    {
        float denom = ray.direction * normal;

        // ako je zraka gotovo paralelna s ravninom, nema (stabilnog) presjeka
        if (std::fabs(denom) < 1e-6f)
            return false;

        float t_hit = ((point - ray.origin) * normal) / denom;

        if (t_hit < 1e-4f)
            return false; // presjek je iza kamere

        t = t_hit;
        // normala uvijek gleda prema zraci (prema kameri/upadnoj strani)
        out_normal = (denom < 0.f) ? normal : -normal;
        return true;
    }
};
