#pragma once

#include <cstdint>
#include <chrono>

class PCGRandom {
private:
    uint64_t state;
    
public:
    PCGRandom();
    PCGRandom(uint64_t seed);

    void init();
    
    void init(uint64_t seed);
    
    uint64_t next();
    uint64_t next(uint64_t min, uint64_t max);
    float nextFloat();
    float nextFloat(float min, float max);

    static uint64_t Random(uint64_t& state);
    static uint64_t Random(uint64_t& state, uint64_t min, uint64_t max);
    static float RandomFloat(uint64_t& state);
    static float RandomFloat(uint64_t& state, float min, float max);
};