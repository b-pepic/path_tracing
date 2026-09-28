# Monte Carlo Path Tracer

Jednostavan path tracer u C++17 koji renderira scenu sa sferama i ravninom,
s podrskom za difuzne, zrcalne i providne (staklene) materijale. Izlaz je
PPM slika (`out.ppm`).

<img width="1280" height="960" alt="out" src="https://github.com/user-attachments/assets/044aab3c-dd3c-4986-baf7-6640f2607970" />


## Kompajliranje i pokretanje

```bash
g++ -std=c++17 -O2 raytrace.cpp -o out.exe -fopenmp
./out.exe
```

OpenMP (`-fopenmp`) nije obavezan, ali bez njega je render znatno sporiji.

Parametri se mijenjaju na vrhu `raytrace.cpp`:
`IMG_WIDTH`, `IMG_HEIGHT`, `SAMPLES_PER_PIXEL`, `FOV`.
Manji `SAMPLES_PER_PIXEL` = brzi render, ali vise suma.

## Struktura

| Datoteka | Sadrzaj |
|---|---|
| `geometry.h` | Predlozak `vec<DIM,T>` i tipovi `Vec2f`, `Vec3f`, ... uz osnovne operatore (skalarni i vektorski produkt, norma, normalizacija). |
| `ray.h` | Klasa `Ray` - ishodiste + smjer. |
| `image.h` | Spremnik piksela i spremanje u binarni PPM (P6). |
| `random_utils.h` | RNG, `reflect`, `refract` (Snell), `schlick_fresnel`, ortonormirana baza, cosine-weighted uzorkovanje polusfere. |
| `material.h` | Model materijala (kd / ks / kt). |
| `objects.h` | `Object` (apstraktna baza), `Sphere`, `Plane` i uzorkovanje svjetlece sfere. |
| `scene.h` | Lista objekata, presjeci, NEE i glavna rekurzija `cast_ray`. |
| `raytrace.cpp` | Kamera, petlja renderiranja, definicija scene, `main`. |

## Model materijala

Nema zasebnih tipova materijala. Jedan `Material` opisan je s tri koeficijenta:

- `kd` - udio difuznog (Lambertovog) odbijanja
- `ks` - udio zrcalne refleksije
- `kt` - udio transmisije (staklo)
- `refractive_index` - indeks loma, relevantan samo kad je `kt > 0`

Ako je `kd + ks + kt < 1`, razlika predstavlja apsorpciju - zraka se tu zaustavlja.
Ovo omogucuje mjesovite materijale (npr. `0.7 / 0.3 / 0.0` = mat povrsina s
zrcalnim sjajem), sto s fiksnim tipovima nije bilo moguce.

Emisivni materijali (`emission > 0`) su izvori svjetla; u sceni je to svjetleca sfera.

## Kako radi renderiranje

1. **Kamera** - `ray_to_pixel` preslikava piksel u smjer zrake preko FOV-a i
   aspect ratia. Unutar piksela se dodaje nasumicni pomak (jitter) pa se
   antialiasing dobiva besplatno iz usrednjavanja uzoraka.

2. **Presjeci** - `scene_intersect` prolazi kroz sve objekte i uzima najblizi
   pogodak. Sfera se rjesava kvadratnom jednadzbom, ravnina linearnom.
   Pod je ravnina, a ne ogromna sfera, zbog numericke stabilnosti u `float`.

3. **Izbor grane** - u `cast_ray` se grana bira stohasticki prema omjeru
   `kd : ks : kt`. Svaka zraka radi tocno jednu stvar, a preko mnogo uzoraka
   se omjer na slici vidi kao postotak difuznog / zrcalnog / staklenog izgleda.
   Tezina se dijeli s vjerojatnoscu izbora grane da procjena ostane nepristrana.

4. **Difuzna grana** - direktno svjetlo racuna se preko **Next Event Estimation**:
   umjesto da se ceka da nasumicna zraka slucajno pogodi malu svjetlecu sferu,
   smjer se bira unutar stosca koji gleda ravno u nju (`Sphere::sample_cone`).
   Bez toga se pojavljuju "fireflies" - rijetke jako svijetle tocke koje ne
   nestaju ni pri velikom broju uzoraka. Indirektno svjetlo nastavlja se
   cosine-weighted uzorkovanjem polusfere.

5. **Zrcalna grana** - deterministicka refleksija oko normale.

6. **Transmisivna grana** - Schlickova aproksimacija Fresnela odlucuje hoce li
   se zraka reflektirati ili lomiti; kod totalne unutarnje refleksije lom nije
   moguc pa se uvijek reflektira.

7. **Prekid puta** - `MAX_DEPTH` ogranicava dubinu rekurzije, a Russian roulette
   nakon nekoliko odbijanja nasumicno prekida put s vjerojatnoscu ovisnom o
   svjetlini materijala. Prezivjeli putovi se skaliraju s `1/survive_prob` pa
   rezultat ostaje nepristran.

8. **Zavrsna obrada** - uzorci se usrednje i primijeni se gamma korekcija (2.2),
   bez koje slika izgleda pretamno.
