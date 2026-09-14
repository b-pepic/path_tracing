#pragma once
#include "geometry.h"

// Umjesto fiksnog MaterialType enuma (DIFFUSE / MIRROR / GLASS), materijal
// sada opisujemo s tri koeficijenta koji kazu KOLIKO je materijal
// difuzan, zrcalan i providan. 
// kd + ks + kt ne mora biti tocno 1 - ako je zbroj manji od 1, razlika
// (1 - kd - ks - kt) predstavlja apsorpciju (materijal "guta" tu energiju).

class Material
{
  public:
    Vec3f diffuse_colour;   // albedo / boja materijala (koristi se za sve lobove kao tint)
    Vec3f emission;         // koliko svjetla materijal SAM emitira

    float kd = 1.0f;        // udio difuznog (Lambert) odbijanja
    float ks = 0.0f;        // udio zrcalnog (savrsena refleksija) odbijanja
    float kt = 0.0f;        // udio transmisije (staklo/refrakcija, Fresnel unutar ove grane)

    float refractive_index = 1.5f; // koristi se samo kad kt > 0 (npr. staklo ~1.5)

    Material() : diffuse_colour(Vec3f(0, 0, 0)), emission(Vec3f(0, 0, 0)) { }

    // opcenit konstruktor - kd/ks/kt po defaultu daju cisto difuzan materijal
    // (isto ponasanje kao stari "MaterialType::DIFFUSE")
    Material(const Vec3f &colour, float kd_ = 1.0f, float ks_ = 0.0f, float kt_ = 0.0f, float ior = 1.5f)
        : diffuse_colour(colour), emission(Vec3f(0, 0, 0)), kd(kd_), ks(ks_), kt(kt_), refractive_index(ior) { }

    // konstruktor za emisivni materijal (izvor svjetla) - i dalje difuzan po defaultu,
    // sto je bitno jer se emisija dodaje bez obzira na tip, ali svjetlo samo po sebi
    // rijetko treba dalje odbijati zraku (obicno je zadnja povrsina na koju zraka padne)
    static Material make_light(const Vec3f &emission_colour)
    {
        Material m(Vec3f(0, 0, 0), 1.0f, 0.0f, 0.0f);
        m.emission = emission_colour;
        return m;
    }

    // zbroj kd+ks+kt - koristi se za normalizaciju vjerojatnosti izbora grane
    float lobe_sum() const { return kd + ks + kt; }
};
