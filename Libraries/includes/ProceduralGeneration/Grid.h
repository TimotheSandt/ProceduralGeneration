#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <functional>

#include "Mesh.h"
#include "Renderer3D.h"

struct Vertex
{
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec3 Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 Color = glm::vec3(0.45f, 0.7f, 0.35f);
};

class Grid
{
  public:
    Grid();
    Grid(float size_x, float size_z, int resolution_x, int resolution_z);
    Grid(const Grid &other);
    ~Grid();

    void Destroy();

    void init(float size_x, float size_z, int resolution_x, int resolution_z);

    void GeneratePoints();
    void GenerateTriangles();
    void GenerateNormals();
    void GenerateMesh();

    void TransformPoints(const std::function<void(Vertex &, unsigned int)> &func);

    void Render(const Renderer3D &renderer3D, Camera &camera);

    unsigned int GetResolutionX() { return this->resolution_x; }
    unsigned int GetResolutionY() { return this->resolution_z; }
    std::vector<Vertex> GetPoints() { return this->points; }
    std::vector<std::array<unsigned int, 3>> GetTriangles() { return this->triangles; }
    Mesh &GetMesh() { return this->mesh; }

    unsigned int GetPointCount() { return this->points.size(); }
    unsigned int GetTriangleCount() { return this->triangles.size(); }
    unsigned int GetChunkCount() const { return static_cast<unsigned int>(this->chunks.size()); }
    unsigned int GetLastRenderedChunkCount() const { return this->lastRenderedChunkCount; }
    unsigned int GetLastRenderedTriangleCount() const { return this->lastRenderedTriangleCount; }

  private:
    struct Chunk
    {
        Mesh mesh;
        glm::vec3 minBounds = glm::vec3(0.0f);
        glm::vec3 maxBounds = glm::vec3(0.0f);
        unsigned int triangleCount = 0;
    };

    void DestroyRenderMeshes();
    void GenerateChunkedMesh();
    void GenerateSingleMesh();

    float size_x;
    float size_z;
    unsigned int resolution_x;
    unsigned int resolution_z;
    std::vector<Vertex> points;
    std::vector<std::array<unsigned int, 3>> triangles;
    std::vector<Chunk> chunks;
    unsigned int lastRenderedChunkCount = 0;
    unsigned int lastRenderedTriangleCount = 0;

    Mesh mesh;
};
