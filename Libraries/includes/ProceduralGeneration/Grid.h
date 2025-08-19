#pragma once


#include <vector>
#include <glm/glm.hpp>
#include <array>

#include "Mesh.h"

class Grid
{
public:
    Grid();
    Grid(float size_x, float size_y, int resolution_x, int resolution_y);
    Grid(const Grid& other);
    ~Grid();

    void init(float size_x, float size_y, int resolution_x, int resolution_y);

    void GeneratePoints();
    void GenerateTriangles();
    void GenerateMesh();

    void Render(Camera& camera);

    unsigned int GetResolutionX() { return this->resolution_x; }
    unsigned int GetResolutionY() { return this->resolution_y; }
    std::vector<glm::vec3> GetPoints() { return this->points; }
    std::vector<std::array<unsigned int, 3>> GetTriangles() { return this->triangles; }
    Mesh GetMesh() { return this->mesh; }


    unsigned int GetPointCount() { return this->points.size(); }
    unsigned int GetTriangleCount() { return this->triangles.size(); }

private:
    float size_x;
    float size_y;
    unsigned int resolution_x;
    unsigned int resolution_y;
    std::vector<glm::vec3> points;
    std::vector<std::array<unsigned int, 3>> triangles;

    Mesh mesh;
};