#pragma once
#include "objects.h"
#include "random_utils.h"
#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>



class Scene
{
    std::vector<Object*> objects;
    std::vector<Sphere*> lights; // podskup objects koji su izvori svjetla (emission > 0)

    static constexpr int MAX_DEPTH = 8;

    bool scene_intersect(const Ray &ray, Vec3f &hit_point, Vec3f &hit_normal, Object* &hit_object) const
    {
        float closest_t = std::numeric_limits<float>::max();
        bool found = false;

        for (Object* obj : objects)
        {
            float t;
            Vec3f normal;
            if (obj->ray_intersect(ray, t, normal) && t < closest_t)
            {
                closest_t = t;
                hit_normal = normal;
                hit_object = obj;
                found = true;
            }
        }

        if (found)
            hit_point = ray.origin + ray.direction * closest_t;

        return found;
    }

    // -----------------------------------------------------------------
    // Next Event Estimation (NEE) = direktno uzorkovanje svjetla.
    // Poziva se samo iz difuzne grane (kao i prije) - zrcalna i
    // transmisivna grana nemaju smisleno "direktno" uzorkovanje jer
    // gledaju samo u jednom tocno odredjenom smjeru.
    // -----------------------------------------------------------------
    Vec3f direct_lighting(const Vec3f &hit_point, const Vec3f &normal, const Material &mat) const
    {
        Vec3f result(0, 0, 0);

        for (Sphere* light : lights)
        {
            Vec3f sample_dir;
            float pdf;
            light->sample_cone(hit_point, sample_dir, pdf);

            float cos_surface = normal * sample_dir;
            if (cos_surface <= 0.f || pdf <= 0.f)
                continue; // svjetlo je "iza" povrsine iz ove tocke, ne doprinosi

            Ray shadow_ray(hit_point + normal * 1e-4f, sample_dir);
            Vec3f s_hit, s_normal;
            Object* s_obj = nullptr;

            if (scene_intersect(shadow_ray, s_hit, s_normal, s_obj) && s_obj == light)
            {
                // Lambertov BRDF (albedo/pi) * emisija * cos(theta) / pdf(prostorni kut)
                Vec3f brdf = mat.diffuse_colour * (1.0f / PI_F);
                Vec3f contrib(
                    brdf.x * light->material.emission.x,
                    brdf.y * light->material.emission.y,
                    brdf.z * light->material.emission.z
                );
                result = result + contrib * (cos_surface / pdf);
            }
            // ako je zaklonjeno (shadow_ray ne pogadja bas 'light'), ne dodajemo nista - to su sjene
        }

        return result;
    }

    // Racuna zrcalnu refleksiju oko normale n. Odvojeno u funkciju samo da
    // se switch/if-else lanac u cast_ray citljivije podijeli po granama.
    static Vec3f mirror_bounce(const Vec3f &incoming_dir, const Vec3f &n)
    {
        Vec3f d = reflect(incoming_dir, n);
        d.normalize();
        return d;
    }

  public:

    // specular_bounce: true ako je ova zraka nastala iz kamere (primarna),
    // ili iz zrcalne/transmisivne grane. U tim slucajevima zelimo direktno
    // "vidjeti" emisiju objekta ako ga pogodimo. Ako je false (nastala je
    // iz difuzne grane), emisiju NE dodajemo ovdje jer je vec obracunata
    // preko direct_lighting() u prethodnom hit-u - inace bismo istu
    // svjetlost brojali dvaput.
    Vec3f cast_ray(const Ray &ray, int depth = MAX_DEPTH, bool specular_bounce = true) const
    {
        if (depth <= 0)
            return Vec3f(0, 0, 0);

        Vec3f hit_point, hit_normal;
        Object* hit_object = nullptr;

        if (!scene_intersect(ray, hit_point, hit_normal, hit_object))
            return Vec3f(0, 0, 0); // pobjegla u "svemir" - crna pozadina

        const Material &mat = hit_object->material;

        Vec3f result(0, 0, 0);
        if (specular_bounce)
            result = mat.emission; // vidimo svjetlo direktno (kamera / nakon zrcala-stakla)

        // Russian roulette - prekidamo put nasumicno na temelju "snage" materijala.
        // Ovo je vec postojeci mehanizam za kontrolu dubine puta; ostaje nepromijenjen.
        float survive_prob = std::max({mat.diffuse_colour.x, mat.diffuse_colour.y, mat.diffuse_colour.z});
        survive_prob = std::min(survive_prob, 0.95f);
        if (depth < MAX_DEPTH - 2)
        {
            if (random_float() > survive_prob)
                return result;
        }
        else
        {
            survive_prob = 1.0f;
        }

        // ---------------------------------------------------------------
        // Izbor grane (difuzna / zrcalna / transmisivna) - ovo zamjenjuje
        // stari switch(mat.type). Svaka zraka i dalje radi TOCNO JEDNU
        // stvar (kao i prije), ali koja je to stvar sada se bira
        // stohasticki prema kd:ks:kt omjeru, umjesto da bude fiksno
        // zadano tipom materijala. Vise uzoraka po pikselu => taj omjer
        // se na slici vidi kao "postotak" difuznog/zrcalnog/staklenog
        // izgleda, tocno onako kako fizikalno i treba izgledati.
        //
        // kd/ks/kt ne moraju zbrajati na 1: ako je zbroj < 1, preostala
        // vjerojatnost (1 - lobe_sum) znaci da je zraka apsorbirana i put
        // ovdje zavrsava (nema nastavka rekurzije).
        // ---------------------------------------------------------------
        float lobe_sum = mat.lobe_sum();

        if (lobe_sum <= 1e-6f)
            return result; // cist apsorber (kd=ks=kt=0) - nema kamo dalje

        float xi = random_float() * lobe_sum;

        Vec3f new_origin, new_dir, weight;
        bool next_specular = true;

        if (xi < mat.kd)
        {
            // --- difuzna grana (Lambert) ---
            result = result + direct_lighting(hit_point, hit_normal, mat);

            new_dir = sample_hemisphere_cosine(hit_normal);
            new_origin = hit_point + hit_normal * 1e-4f;

            // weight = diffuse_colour * lobe_sum: faktor lobe_sum dolazi iz
            // dijeljenja s vjerojatnoscu izbora ove grane (kd/lobe_sum),
            // pa se kd skrati i ostane samo lobe_sum - isti trik kao
            // 1/survive_prob nekoliko redaka iznad.
            weight = mat.diffuse_colour * lobe_sum;
            next_specular = false;
        }
        else if (xi < mat.kd + mat.ks)
        {
            // --- zrcalna grana (savrsena refleksija) ---
            new_dir = mirror_bounce(ray.direction, hit_normal);
            new_origin = hit_point + hit_normal * 1e-4f;

            weight = mat.diffuse_colour * lobe_sum;
            next_specular = true;
        }
        else
        {
            // --- transmisivna grana (staklo) - Fresnel unutar grane odlucuje
            // ide li ZRAKA reflektirati ili refraktirati, isto kao prije u
            // MaterialType::GLASS grani ---
            Vec3f n = hit_normal;
            float eta_ratio;
            bool entering = (ray.direction * n) < 0.f;

            if (entering) eta_ratio = 1.0f / mat.refractive_index;
            else { n = -n; eta_ratio = mat.refractive_index; }

            float cos_theta = std::fmin(-(ray.direction * n), 1.0f);
            float fresnel = schlick_fresnel(cos_theta, mat.refractive_index);

            Vec3f refracted;
            bool can_refract = refract(ray.direction, n, eta_ratio, refracted);

            if (!can_refract || random_float() < fresnel)
            {
                new_dir = reflect(ray.direction, n);
                new_origin = hit_point + n * 1e-4f;
            }
            else
            {
                new_dir = refracted;
                new_origin = hit_point - n * 1e-4f;
            }
            new_dir.normalize();

            weight = Vec3f(1, 1, 1) * lobe_sum;
            next_specular = true;
        }

        Ray next_ray(new_origin, new_dir);
        Vec3f incoming = cast_ray(next_ray, depth - 1, next_specular);

        Vec3f reflected = Vec3f(
            weight.x * incoming.x,
            weight.y * incoming.y,
            weight.z * incoming.z
        ) * (1.0f / survive_prob);

        return result + reflected;
    }

    void add_object(Object* object)
    {
        objects.push_back(object);

        // ako objekt emitira svjetlo I radi se o sferi, dodajemo ga u
        // listu svjetala da bismo ga mogli direktno uzorkovati (NEE)
        Vec3f e = object->material.emission;
        if (e.x > 0.f || e.y > 0.f || e.z > 0.f)
        {
            Sphere* s = dynamic_cast<Sphere*>(object);
            if (s) lights.push_back(s);
        }
    }
};
