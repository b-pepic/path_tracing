#pragma once
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "geometry.h"
using namespace std;

class Image
{
    vector<Vec3f> pixels;
    
  public:
    const int width;
    const int height;
    
    void set_pixel(int i, int j, const Vec3f colour)
    {
        pixels[i + j * width] = colour;
    }
    
    void save(const string path)
    {
        ofstream ofs;
        ofs.open(path, ofstream::binary);
        
        ofs << "P6\n" << width << " " << height << "\n255\n";
        
        for (size_t i = 0; i < width * height; ++i)
        {
            for (size_t color = 0; color < 3; ++color)
            {
                ofs << (unsigned char)(255 * clamp(pixels[i][color], 0.f, 1.f));
            }
        }
        
        ofs.close(); 
    }
    
    Image(const int width, const int height) : width(width), height(height)
    {
        pixels = vector<Vec3f>(width * height);
    }
};
