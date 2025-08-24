#include "Noise.h"

#include <cmath>




uint64_t hash(uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

Noise::Noise() {
    this->SetSeed();
}

Noise::Noise(uint64_t seed) {
    this->SetSeed(seed);
}

void Noise::SetSeed() {
    this->seed = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

void Noise::SetSeed(uint64_t seed) {
    this->seed = seed;
}

uint64_t Noise::GetSeed() {
    return this->seed;
}

float Noise::WhiteNoise(float x) {
    return WhiteNoise(x, 0.0f, 0.0f, 0.0f);
}

float Noise::WhiteNoise(float x, float y) {
    return WhiteNoise(x, y, 0.0f, 0.0f);
}

float Noise::WhiteNoise(float x, float y, float z) {
    return WhiteNoise(x, y, z, 0.0f);
}

float Noise::WhiteNoise(float x, float y, float z, float w) {
    int32_t ix = (int32_t)(x * 113.0f);  // Scale factor for precision
    int32_t iy = (int32_t)(y * 113.0f);
    int32_t iz = (int32_t)(z * 113.0f);
    int32_t iw = (int32_t)(w * 113.0f);

    uint64_t h = hash(ix ^ seed);
    h = hash(h ^ iy);
    h = hash(h ^ iz);
    h = hash(h ^ iw);

    return PCGRandom::RandomFloat(h, -1.0f, 1.0f);
}


float Noise::SmoothNoise(float x, float scale) {
    if (scale == 0.0f) return 0.0f;

    float tx = x * std::cos(0.5f);

    float x0 = tx * scale;
    float x1 = std::floor(x0);

    float x2 = x1 + 1.0f;
    float fx = x0 - x1;
    fx = fx * fx * (3.0f - 2.0f * fx);

    float n0 = this->WhiteNoise(x1);
    float n1 = this->WhiteNoise(x2);

    return lerp(n0, n1, fx);
}

float Noise::SmoothNoise(float x, float y, float scale) {
    if (scale == 0.0f) return 0.0f;
    rotate(x, y, 0.5f);

    float tx = x * std::cos(0.5f) - y * std::sin(0.5f);
    float ty = x * std::sin(0.5f) + y * std::cos(0.5f);

    float x0 = tx * scale;
    float y0 = ty * scale;

    float x1 = std::floor(x0);
    float y1 = std::floor(y0);

    float x2 = x1 + 1.0f;
    float y2 = y1 + 1.0f;

    float fx = std::abs(x0 - x1);
    float fy = std::abs(y0 - y1);

    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);

    float n0 = this->WhiteNoise(x1, y1);
    float n1 = this->WhiteNoise(x2, y1);
    float n2 = this->WhiteNoise(x1, y2);
    float n3 = this->WhiteNoise(x2, y2);

    float l1 = lerp(n0, n1, fx);
    float l2 = lerp(n2, n3, fx);

    return lerp(l1, l2, fy);
}

float Noise::SmoothNoise(float x, float y, float z, float scale) {
    if (scale == 0.0f) return 0.0f;
    rotate(x, y, z, 0.5f);
    
    float x0 = x * scale;
    float y0 = y * scale;
    float z0 = z * scale;

    float x1 = std::floor(x0);
    float y1 = std::floor(y0);
    float z1 = std::floor(z0);

    float x2 = x1 + 1.0f;
    float y2 = y1 + 1.0f;
    float z2 = z1 + 1.0f;

    float fx = x0 - x1;
    float fy = y0 - y1;
    float fz = z0 - z1;

    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    fz = fz * fz * (3.0f - 2.0f * fz);

    float n0 = this->WhiteNoise(x1, y1, z1);
    float n1 = this->WhiteNoise(x2, y1, z1);
    float n2 = this->WhiteNoise(x1, y2, z1);
    float n3 = this->WhiteNoise(x2, y2, z1);
    float n4 = this->WhiteNoise(x1, y1, z2);
    float n5 = this->WhiteNoise(x2, y1, z2);
    float n6 = this->WhiteNoise(x1, y2, z2);
    float n7 = this->WhiteNoise(x2, y2, z2);

    float l1 = lerp(n0, n1, fx);
    float l2 = lerp(n2, n3, fx);
    float l3 = lerp(n4, n5, fx);
    float l4 = lerp(n6, n7, fx);

    float i1 = lerp(l1, l2, fy);
    float i2 = lerp(l3, l4, fy);
    
    return lerp(i1, i2, fz);
}

float Noise::SmoothNoise(float x, float y, float z, float w, float scale) {
    if (scale == 0.0f) return 0.0f;
    rotate(x, y, z, w, 0.5f);
    
    float x0 = x * scale;
    float y0 = y * scale;
    float z0 = z * scale;
    float w0 = w * scale;

    float x1 = std::floor(x0);
    float y1 = std::floor(y0);
    float z1 = std::floor(z0);
    float w1 = std::floor(w0);

    float x2 = x1 + 1.0f;
    float y2 = y1 + 1.0f;
    float z2 = z1 + 1.0f;
    float w2 = w1 + 1.0f;

    float fx = x0 - x1;
    float fy = y0 - y1;
    float fz = z0 - z1;
    float fw = w0 - w1;

    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    fz = fz * fz * (3.0f - 2.0f * fz);
    fw = fw * fw * (3.0f - 2.0f * fw);

    float n0 = this->WhiteNoise(x1, y1, z1, w1);
    float n1 = this->WhiteNoise(x2, y1, z1, w1);
    float n2 = this->WhiteNoise(x1, y2, z1, w1);
    float n3 = this->WhiteNoise(x2, y2, z1, w1);
    float n4 = this->WhiteNoise(x1, y1, z2, w1);
    float n5 = this->WhiteNoise(x2, y1, z2, w1);
    float n6 = this->WhiteNoise(x1, y2, z2, w1);
    float n7 = this->WhiteNoise(x2, y2, z2, w1);
    float n8 = this->WhiteNoise(x1, y1, z1, w2);
    float n9 = this->WhiteNoise(x2, y1, z1, w2);
    float n10 = this->WhiteNoise(x1, y2, z1, w2);
    float n11 = this->WhiteNoise(x2, y2, z1, w2);
    float n12 = this->WhiteNoise(x1, y1, z2, w2);
    float n13 = this->WhiteNoise(x2, y1, z2, w2);
    float n14 = this->WhiteNoise(x1, y2, z2, w2);
    float n15 = this->WhiteNoise(x2, y2, z2, w2);

    float l1 = lerp(n0, n1, fx);
    float l2 = lerp(n2, n3, fx);
    float l3 = lerp(n4, n5, fx);
    float l4 = lerp(n6, n7, fx);
    float l5 = lerp(n8, n9, fx);
    float l6 = lerp(n10, n11, fx);
    float l7 = lerp(n12, n13, fx);
    float l8 = lerp(n14, n15, fx);

    float i1 = lerp(l1, l2, fy);
    float i2 = lerp(l3, l4, fy);
    float i3 = lerp(l5, l6, fy);
    float i4 = lerp(l7, l8, fy);

    float j1 = lerp(i1, i2, fz);
    float j2 = lerp(i3, i4, fz);

    return lerp(j1, j2, fw);
}

float Noise::FractalNoise(float x, float scale, int octaves, float persistence, float lacunarity) {
    float total = 0.0f, frequency = scale, amplitude = 1.0f, maxAmp = 0.0f;
    for (int i = 0; i < octaves; i++) {
        total += SmoothNoise(x + i * 67, frequency) * amplitude;
        maxAmp += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return total / maxAmp;
}

float Noise::FractalNoise(float x, float y, float scale, int octaves, float persistence, float lacunarity) {
    float total = 0.0f, frequency = scale, amplitude = 1.0f, maxAmp = 0.0f;
    for (int i = 0; i < octaves; i++) {
        total += SmoothNoise(x + i * 67, y - i * 79, frequency) * amplitude;
        maxAmp += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return total / maxAmp;
}

float Noise::FractalNoise(float x, float y, float z, float scale, int octaves, float persistence, float lacunarity) {
    float total = 0.0f, frequency = scale, amplitude = 1.0f, maxAmp = 0.0f;
    for (int i = 0; i < octaves; i++) {
        total += SmoothNoise(x + i * 67, y - i * 79, z + i * 97, frequency) * amplitude;
        maxAmp += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return total / maxAmp;
}

float Noise::FractalNoise(float x, float y, float z, float w, float scale, int octaves, float persistence, float lacunarity) {
    float total = 0.0f, frequency = scale, amplitude = 1.0f, maxAmp = 0.0f;
    for (int i = 0; i < octaves; i++) {
        total += SmoothNoise(x + i * 67, y - i * 79, z + i * 97, w - i * 137, frequency) * amplitude;
        maxAmp += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return total / maxAmp;
}