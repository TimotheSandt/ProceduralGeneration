#pragma once

#include "Grid.h"
#include "Noise.h"

#include <functional>
#include <cmath>
#include <memory>

class TerrainGenerator
{
public:
    TerrainGenerator() = default;
    TerrainGenerator(int seed);
    TerrainGenerator(float sizeX, float sizeZ, int resX, int resZ);
    TerrainGenerator(float sizeX, float sizeZ, int resX, int resZ, int seed);
    ~TerrainGenerator();
    
    void Destroy();

    void init(float sizeX, float sizeZ, int resX, int resZ);
    void Render(Camera& camera);


    // Terrain Generation
    void GenerateFlatTerrain();
    
    void GenerateRandomTerrain(float height = 1.0f);
    
    void GeneratePerlinTerrain(float scale, float height, int octaves, float persistence, float lacunarity);
    void GenerateFractalTerrain(float scale, float height, int octaves, float persistence, float lacunarity);


    // Terrain Modification
    void GenerateCrater(float depth, float radius, glm::vec3 center);



    Grid& GetGrid() { return grid; }
    Mesh& GetMesh() { return grid.GetMesh(); }

    void SetNoiseSeed(int seed) { noise.SetSeed(seed); }
    

private:
    Grid grid;
    Noise noise;
};