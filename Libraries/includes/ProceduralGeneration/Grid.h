#pragma once


#include <vector>
#include <glm/glm.hpp>
#include <array>
#include <functional>

#include "Mesh.h"

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec3 Color;
};

class Grid
{
public:
    Grid();
    Grid(float size_x, float size_z, int resolution_x, int resolution_z);
    Grid(const Grid& other);
    ~Grid();

    void Destroy();

    void init(float size_x, float size_z, int resolution_x, int resolution_z);

    void GeneratePoints();
    void GenerateTriangles();
    void GenerateNormals();
    void GenerateMesh();

    void TransformPoints(std::function<void(Vertex&, unsigned int)> func);

    void Render(Camera& camera);

    unsigned int GetResolutionX() { return this->resolution_x; }
    unsigned int GetResolutionY() { return this->resolution_z; }
    std::vector<Vertex> GetPoints() { return this->points; }
    std::vector<std::array<unsigned int, 3>> GetTriangles() { return this->triangles; }
    Mesh& GetMesh() { return this->mesh; }


    unsigned int GetPointCount() { return this->points.size(); }
    unsigned int GetTriangleCount() { return this->triangles.size(); }

private:
    float size_x;
    float size_z;
    unsigned int resolution_x;
    unsigned int resolution_z;
    std::vector<Vertex> points;
    std::vector<std::array<unsigned int, 3>> triangles;

    Mesh mesh;
};