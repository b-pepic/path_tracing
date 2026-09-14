#include "image.h"
#include "geometry.h"
#include "ray.h"
#include "objects.h"
#include "scene.h"
#include "random_utils.h"
#include <cmath>
#include <iostream>

using namespace std;

// compile with: g++ -std=c++17 -O2 raytrace.cpp -o out.exe -fopenmp


// -----------------------------------------------------------------
// PODESIVI PARAMETRI
// -----------------------------------------------------------------
static const int IMG_WIDTH   = 1280;
static const int IMG_HEIGHT  = 960;
static const int SAMPLES_PER_PIXEL = 10000; // sto vise, to manje suma (ali sporije).
static const float FOV = PI_F / 3.f; // 60 stupnjeva

// vraca zraku koja ide od kamere do piksela (i, j) na slici.
// jitter_x, jitter_y su u [0,1) i pomicu tocku unutar piksela
Ray ray_to_pixel(Vec3f origin, const Image &img, int i, int j, float jitter_x, float jitter_y)
{
    float aspect_ratio = img.width / (float)img.height;

    // preslikavamo piksel (i,j) u interval otprilike [-1, 1] (l=-1, r=1, b=1, t=-1),
    // pa skaliramo s tan(fov/2) i aspect ratio-m da dobijemo pravi kut vidnog polja
    float px = (2.f * (i + jitter_x) / (float)img.width - 1.f) * tan(FOV / 2.f) * aspect_ratio;
    float py = (1.f - 2.f * (j + jitter_y) / (float)img.height) * tan(FOV / 2.f);

    Vec3f dir(px, py, -1.f);
    dir.normalize();

    return Ray(origin, dir);
}

void render(const Scene &scene)
{
    Image img(IMG_WIDTH, IMG_HEIGHT);
    Vec3f camera_origin(0, 0, 0);

    cout << "Renderiram " << IMG_WIDTH << "x" << IMG_HEIGHT
         << " sa " << SAMPLES_PER_PIXEL << " uzoraka po pikselu..." << endl;

    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = 0; j < IMG_HEIGHT; ++j)
    {
        for (int i = 0; i < IMG_WIDTH; ++i)
        {
            Vec3f colour(0, 0, 0);

            // Monte Carlo: saljemo puno nasumicnih zraka kroz (otprilike) isti
            // piksel i usrednjimo rezultate
            for (int s = 0; s < SAMPLES_PER_PIXEL; ++s)
            {
                float jitter_x = random_float();
                float jitter_y = random_float();

                Ray ray = ray_to_pixel(camera_origin, img, i, j, jitter_x, jitter_y);
                colour = colour + scene.cast_ray(ray);
            }

            colour = colour * (1.0f / SAMPLES_PER_PIXEL);

            // gamma korekcija (gamma = 2.2) - bez ovoga slika izgleda pretamno,
            // jer monitori ne prikazuju boju linearno
            Vec3f gamma_corrected(
                std::pow(std::max(colour.x, 0.f), 1.f / 2.2f),
                std::pow(std::max(colour.y, 0.f), 1.f / 2.2f),
                std::pow(std::max(colour.z, 0.f), 1.f / 2.2f)
            );

            img.set_pixel(i, j, gamma_corrected);
        }

        #pragma omp critical
        cout << "\rRed " << (j + 1) << " / " << IMG_HEIGHT << flush;
    }
    cout << endl;

    img.save("out.ppm");
    cout << "Spremljeno u out.ppm" << endl;
}

int main()
{
    Scene scene;

    // Material(colour, kd, ks, kt, refractive_index) - vidi material.h
    // kd = difuzni udio, ks = zrcalni udio, kt = transmisivni (staklo) udio.
    

    
    Material red       (Vec3f(0.7f, 0.1f, 0.1f), 1.0f, 0.0f, 0.0f);
    Material green      (Vec3f(0.1f, 0.6f, 0.1f), 1.0f, 0.0f, 0.0f);
    Material grey_floor (Vec3f(0.6f, 0.6f, 0.6f), 1.0f, 0.0f, 0.0f);

    
    Material mirror(Vec3f(0.9f, 0.9f, 0.9f), 0.0f, 1.0f, 0.0f);

    
    Material glass(Vec3f(1.0f, 1.0f, 1.0f), 0.0f, 0.0f, 1.0f);
    glass.refractive_index = 1.5f;

    // NOVO: mjesoviti materijal koji ranije nije bio moguc 
    Material glossy_ceramic(Vec3f(0.75f, 0.75f, 0.8f), 0.7f, 0.3f, 0.0f);

    // izvor svjetla = emisivna sfera
    Material light_mat = Material::make_light(Vec3f(15.f, 15.f, 15.f));

    // pod - beskonacna ravnina
    static Plane floor_plane(Vec3f(0, -4, 0), Vec3f(0, 1, 0), grey_floor);

    static Sphere s_red(Vec3f(-3.5f, -1.f, -16.f), 2.f, red);
    static Sphere s_mirror(Vec3f(-0.5f, -1.5f, -11.f), 1.5f, mirror);
    static Sphere s_glass(Vec3f(2.2f, -1.5f, -13.f), 1.5f, glass);
    static Sphere s_green(Vec3f(4.5f, 0.5f, -20.f), 2.5f, green);
    static Sphere s_glossy(Vec3f(0.5f, -2.f, -7.f), 1.f, glossy_ceramic);
    static Sphere light_sphere(Vec3f(0.f, 8.f, -20.f), 3.5f, light_mat);

    scene.add_object(&floor_plane);
    scene.add_object(&s_red);
    scene.add_object(&s_mirror);
    scene.add_object(&s_glass);
    scene.add_object(&s_green);
    scene.add_object(&s_glossy);
    scene.add_object(&light_sphere);

    render(scene);
    return 0;
}
