#include "TerrainGenerator.h"

TerrainGenerator::TerrainGenerator(int seed) : noise(seed) {}
TerrainGenerator::TerrainGenerator(float sizeX, float sizeZ, int resX, int resZ) : noise() {
    init(sizeX, sizeZ, resX, resZ);
}
TerrainGenerator::TerrainGenerator(float sizeX, float sizeZ, int resX, int resZ, int seed) : noise(seed) {
    init(sizeX, sizeZ, resX, resZ);
}

void TerrainGenerator::init(float sizeX, float sizeZ, int resX, int resZ) {
    grid.init(sizeX, sizeZ, resX, resZ);
    grid.GenerateMesh();
}

void TerrainGenerator::Render(Camera& camera) {
    grid.Render(camera);
}


void TerrainGenerator::GenerateFlatTerrain() {
    grid.TransformPoints([this](Vertex& vertex, unsigned int index) {
        vertex.Position.y = 0.0f;
    });
    grid.GenerateMesh();
}

void TerrainGenerator::GenerateRandomTerrain(float height) {
    grid.TransformPoints([this, height](Vertex& vertex, unsigned int index) {
        vertex.Position.y = noise.WhiteNoise(vertex.Position.x, vertex.Position.z) * height;
    });
    grid.GenerateMesh();
}

void TerrainGenerator::GeneratePerlinTerrain(float scale, float height, int octaves, float persistence, float lacunarity) {
    this->GeneratePerlinTerrain(scale, scale, height, octaves, persistence, lacunarity);
}

void TerrainGenerator::GeneratePerlinTerrain(float scale_x, float scale_z, float height, int octaves, float persistence, float lacunarity) {
    // grid.TransformPoints([this, scale_x, scale_z, height, octaves, persistence, lacunarity](Vertex& vertex, unsigned int index) {
    //     vertex.Position.y = noise.PerlinNoise(vertex.Position.x * scale_x, vertex.Position.z * scale_z, octaves, persistence, lacunarity) * height;
    // });
    // grid.GenerateMesh();
}

void TerrainGenerator::GenerateFractalTerrain(float scale, float height, int octaves, float persistence, float lacunarity) {
    this->GenerateFractalTerrain(scale, scale, height, octaves, persistence, lacunarity);
}

void TerrainGenerator::GenerateFractalTerrain(float scale_x, float scale_z, float height, int octaves, float persistence, float lacunarity) {
    // grid.TransformPoints([this, scale_x, scale_z, height, octaves, persistence, lacunarity](Vertex& vertex, unsigned int index) {
    //     vertex.Position.y = noise.FractalNoise(vertex.Position.x * scale_x, vertex.Position.z * scale_z, octaves, persistence, lacunarity) * height;
    // });
    // grid.GenerateMesh();
}

void TerrainGenerator::GenerateCrater(float depth, float radius, glm::vec3 center) {}