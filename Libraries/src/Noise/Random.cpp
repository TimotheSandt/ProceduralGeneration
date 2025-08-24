#include "Random.h"

PCGRandom::PCGRandom() {
    this->init();
}
PCGRandom::PCGRandom(uint64_t seed) {
    this->init(seed);
}

void PCGRandom::init() {
    this->init(static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
}

void PCGRandom::init(uint64_t seed) {
    state = 0U;
    this->next();
    state += seed;
    this->next();
}

uint64_t PCGRandom::next() {
    return PCGRandom::Random(state);
}

uint64_t PCGRandom::next(uint64_t min, uint64_t max) {
    return min + this->next() % (max - min);
}

float PCGRandom::nextFloat() {
    return (float)this->next() / (float)UINT64_MAX;
}

float PCGRandom::nextFloat(float min, float max) {
    return min + this->nextFloat() * (max - min);
}


uint64_t PCGRandom::Random(uint64_t& state) {
    state = state * 747796405u + 2891336453u;
    uint64_t result = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    result = (result >> 22u) ^ result;
    return result;
}

uint64_t PCGRandom::Random(uint64_t& state, uint64_t min, uint64_t max) {
    return min + PCGRandom::Random(state) % (max - min);
}

float PCGRandom::RandomFloat(uint64_t& state) {
    return (float)PCGRandom::Random(state) / (float)UINT64_MAX;
}

float PCGRandom::RandomFloat(uint64_t& state, float min, float max) {
    return min + PCGRandom::RandomFloat(state) * (max - min);
}