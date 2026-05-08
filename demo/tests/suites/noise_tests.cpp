#include "suites/Suites.h"

#include "Noise/Noise.h"
#include "Noise/Random.h"

#include <cmath>

namespace tests
{

TestSuite CreateNoiseSuite()
{
    TestSuite suite{"Noise"};

    AddTest(suite, "pcg with same seed is deterministic",
            []
            {
                PCGRandom a(42);
                PCGRandom b(42);
                for (int i = 0; i < 5; ++i)
                {
                    AssertEqual(a.next(), b.next(), "Random sequence should be deterministic for the same seed");
                }
            });

    AddTest(suite, "pcg ranged next stays in bounds",
            []
            {
                PCGRandom random(123);
                for (int i = 0; i < 64; ++i)
                {
                    const uint64_t value = random.next(10, 20);
                    Assert(value >= 10 && value < 20, "Ranged random integer should stay within bounds");
                }
            });

    AddTest(suite, "pcg nextFloat stays in unit interval",
            []
            {
                PCGRandom random(321);
                for (int i = 0; i < 64; ++i)
                {
                    const float value = random.nextFloat();
                    Assert(value >= 0.0f && value <= 1.0f, "Random float should stay within [0, 1]");
                }
            });

    AddTest(suite, "pcg nextFloat range stays in bounds",
            []
            {
                PCGRandom random(456);
                for (int i = 0; i < 64; ++i)
                {
                    const float value = random.nextFloat(-5.0f, 5.0f);
                    Assert(value >= -5.0f && value <= 5.0f, "Ranged random float should stay within bounds");
                }
            });

    AddTest(suite, "static random is deterministic for the same state",
            []
            {
                uint64_t stateA = 999;
                uint64_t stateB = 999;
                for (int i = 0; i < 5; ++i)
                {
                    AssertEqual(PCGRandom::Random(stateA), PCGRandom::Random(stateB), "Static random should be deterministic");
                }
            });

    AddTest(suite, "white noise is deterministic for the same seed",
            []
            {
                Noise noiseA(77);
                Noise noiseB(77);
                AssertNear(noiseA.WhiteNoise(1.25f, -3.5f), noiseB.WhiteNoise(1.25f, -3.5f), 1e-7, "White noise should be deterministic");
            });

    AddTest(suite, "different seeds change white noise",
            []
            {
                Noise noiseA(77);
                Noise noiseB(78);
                const float valueA = noiseA.WhiteNoise(0.5f, 1.25f, -2.5f);
                const float valueB = noiseB.WhiteNoise(0.5f, 1.25f, -2.5f);
                Assert(std::abs(valueA - valueB) > 1e-6f, "Different seeds should change the noise result");
            });

    AddTest(suite, "smooth noise returns zero when scale is zero",
            []
            {
                Noise noise(19);
                AssertNear(noise.SmoothNoise(4.0f, 0.0f), 0.0, 1e-7, "Smooth noise should be zero when scale is zero");
                AssertNear(noise.SmoothNoise(4.0f, 2.0f, 0.0f), 0.0, 1e-7, "2D smooth noise should be zero when scale is zero");
            });

    AddTest(suite, "fractal noise stays finite",
            []
            {
                Noise noise(55);
                const float value = noise.FractalNoise(1.0f, -2.0f, 0.75f, 4, 0.5f, 2.0f);
                Assert(std::isfinite(value), "Fractal noise should produce a finite value");
                Assert(value >= -1.1f && value <= 1.1f, "Fractal noise should remain normalized");
            });

    AddTest(suite, "4d white noise stays normalized",
            []
            {
                Noise noise(101);
                const float value = noise.WhiteNoise(1.0f, 2.0f, 3.0f, 4.0f);
                Assert(value >= -1.0f && value <= 1.0f, "4D white noise should stay within [-1, 1]");
            });

    return suite;
}

} // namespace tests
