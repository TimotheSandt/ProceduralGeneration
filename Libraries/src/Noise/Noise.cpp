#include "Noise.h"

#include <chrono>


uint32_t hash(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

// Convert hash to float in range [-1, 1]
float hashToFloat(uint32_t h) {
    return (float)(h & 0xFFFFFF) / (float)0x800000 - 1.0f;
}

Noise::Noise() {
    this->SetSeed();
}

Noise::Noise(int seed) {
    this->SetSeed(seed);
}

void Noise::SetSeed() {
    this->seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

void Noise::SetSeed(int seed) {
    this->seed = seed;
}

int Noise::GetSeed() {
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
    uint32_t ix = *(uint32_t*)&x;
    uint32_t iy = *(uint32_t*)&y;
    uint32_t iz = *(uint32_t*)&z;
    uint32_t iw = *(uint32_t*)&w;
    uint32_t h = hash(ix ^ iy ^ iz ^ iw ^ seed);
    return hashToFloat(h);
}