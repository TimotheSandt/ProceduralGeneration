#pragma once


class Noise
{
public:
    Noise();
    Noise(int seed);
    ~Noise() = default;

    void SetSeed();
    void SetSeed(int seed);
    int GetSeed();

    virtual float WhiteNoise(float x);
    virtual float WhiteNoise(float x, float y);
    virtual float WhiteNoise(float x, float y, float z);
    virtual float WhiteNoise(float x, float y, float z, float w);

private:
    int seed;
};
