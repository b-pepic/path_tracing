#pragma once
#include <random>
#include <cmath>
#include "geometry.h"


constexpr float PI_F = 3.14159265358979323846f;

// -----------------------------------------------------------------------
// Random utility funkcije potrebne za Monte Carlo path tracing.
// -----------------------------------------------------------------------

// thread_local da svaki thread (ako se koristi OpenMP) ima svoj generator
inline std::mt19937 &rng_engine()
{
    thread_local std::mt19937 gen(std::random_device{}());
    return gen;
}

// vraca nasumican broj u [0, 1)
inline float random_float()
{
    thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng_engine());
}

// reflektira vektor v oko normale n (n mora biti normaliziran)
inline Vec3f reflect(const Vec3f &v, const Vec3f &n)
{
    return v - n * (2.f * (v * n));
}

// Snellov zakon loma (refrakcija) - vraca lomljeni vektor.
// eta_ratio = n1/n2 (omjer indeksa loma sredstva iz kojeg zraka dolazi i u koje ulazi)
// vraca false ako dolazi do totalne unutarnje refleksije (nema rjesenja)
inline bool refract(const Vec3f &v, const Vec3f &n, float eta_ratio, Vec3f &out)
{
    Vec3f uv = v; uv.normalize();
    float cos_theta = std::fmin(-uv * n, 1.0f);
    float sin2_theta_out = eta_ratio * eta_ratio * (1.0f - cos_theta * cos_theta);
    if (sin2_theta_out > 1.0f) return false; // totalna unutarnja refleksija

    Vec3f r_out_perp = (uv + n * cos_theta) * eta_ratio;
    Vec3f r_out_par = n * (-std::sqrt(std::fabs(1.0f - sin2_theta_out)));
    out = r_out_perp + r_out_par;
    return true;
}

// Schlickova aproksimacija za Fresnelov koeficijent
// (koliko svjetla se reflektira vs. lomi na granici dva medija, ovisno o kutu)
inline float schlick_fresnel(float cosine, float ref_idx)
{
    float r0 = (1.f - ref_idx) / (1.f + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.f - r0) * std::pow((1.f - cosine), 5.f);
}

// gradi ortonormiranu bazu (tangent, bitangent) oko dane normale
// potrebno da bismo nasumičan smjer generiran u "lokalnom" prostoru (oko z-osi)
// preveli u prostor gdje je z-os = normala povrsine
inline void build_onb(const Vec3f &n, Vec3f &tangent, Vec3f &bitangent)
{
    // biramo pomocni vektor koji sigurno nije paralelan s n
    Vec3f helper = (std::fabs(n.x) > 0.9f) ? Vec3f(0, 1, 0) : Vec3f(1, 0, 0);
    tangent = cross(helper, n); tangent.normalize();
    bitangent = cross(n, tangent);
}

// cosine-weighted uzorkovanje polusfere oko normale.
// "cosine-weighted" znaci da su smjerovi blizu normale vjerojatniji -
// to tocno odgovara Lambertovom (difuznom) BRDF-u, pa se cos(theta) i pdf
// pokrate u Monte Carlo formuli
inline Vec3f sample_hemisphere_cosine(const Vec3f &normal)
{
    float r1 = random_float(); // nasumicni kut oko normale (0 - 2pi)
    float r2 = random_float(); // kontrolira udaljenost od pola (koliko "kos" je smjer)

    float phi = 2.f * PI_F * r1;
    float cos_theta = std::sqrt(1.f - r2); // cosine-weighted distribucija
    float sin_theta = std::sqrt(r2);

    // smjer u lokalnom koordinatnom sustavu (z = normala)
    Vec3f local_dir(sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta);

    Vec3f tangent, bitangent;
    build_onb(normal, tangent, bitangent);

    // transformacija iz lokalnog u world prostor
    Vec3f world_dir = tangent * local_dir.x + bitangent * local_dir.y + normal * local_dir.z;
    world_dir.normalize();
    return world_dir;
}
