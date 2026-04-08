#include "suites/Suites.h"

#include "ProceduralGeneration/Grid.h"
#include "ProceduralGeneration/TerrainGenerator.h"

#include <cmath>

namespace tests {

namespace {

void AssertVec3Near(const glm::vec3& actual, const glm::vec3& expected, float epsilon, const std::string& message) {
    AssertNear(actual.x, expected.x, epsilon, message + " (x)");
    AssertNear(actual.y, expected.y, epsilon, message + " (y)");
    AssertNear(actual.z, expected.z, epsilon, message + " (z)");
}

}  // namespace

TestSuite CreateProceduralSuite() {
    TestSuite suite{"Procedural"};

    AddTest(suite, "default grid creates nine points", [] {
        Grid grid;
        AssertEqual(grid.GetPointCount(), 9u, "Default grid should contain 9 points");
    });

    AddTest(suite, "grid init computes point and triangle counts", [] {
        Grid grid;
        grid.init(4.0f, 6.0f, 4, 5);
        grid.GenerateTriangles();
        AssertEqual(grid.GetPointCount(), 20u, "Grid point count should match the resolution");
        AssertEqual(grid.GetTriangleCount(), 24u, "Grid triangle count should match the resolution");
    });

    AddTest(suite, "grid points are centered around origin", [] {
        Grid grid;
        grid.init(4.0f, 6.0f, 3, 4);
        const std::vector<Vertex> points = grid.GetPoints();
        AssertVec3Near(points.front().Position, glm::vec3(-2.0f, 0.0f, -3.0f), 1e-5f, "Grid should start at the negative half extents");
        AssertVec3Near(points.back().Position, glm::vec3(2.0f, 0.0f, 3.0f), 1e-5f, "Grid should end at the positive half extents");
    });

    AddTest(suite, "grid triangle winding stays stable", [] {
        Grid grid;
        grid.init(2.0f, 2.0f, 3, 3);
        grid.GenerateTriangles();
        const auto triangles = grid.GetTriangles();
        AssertEqual(triangles.front()[0], 0u, "First triangle should start at vertex 0");
        AssertEqual(triangles.front()[1], 3u, "First triangle second index should advance on x");
        AssertEqual(triangles.front()[2], 1u, "First triangle third index should advance on z");
    });

    AddTest(suite, "flat grid normals are unit length", [] {
        Grid grid;
        grid.init(2.0f, 2.0f, 4, 4);
        grid.GenerateTriangles();
        grid.GenerateNormals();
        const std::vector<Vertex> points = grid.GetPoints();
        for (const Vertex& vertex : points) {
            const float length = glm::length(vertex.Normal);
            AssertNear(length, 1.0, 1e-4, "Each normal should be normalized");
            Assert(std::abs(vertex.Normal.x) <= 1e-4f, "Flat grid normal x should stay near zero");
            Assert(std::abs(vertex.Normal.z) <= 1e-4f, "Flat grid normal z should stay near zero");
            Assert(std::abs(std::abs(vertex.Normal.y) - 1.0f) <= 1e-4f, "Flat grid normal y magnitude should be one");
        }
    });

    AddTest(suite, "transform points updates heights", [] {
        Grid grid;
        grid.init(2.0f, 2.0f, 3, 3);
        grid.GenerateTriangles();
        grid.TransformPoints([](Vertex& vertex, unsigned int index) {
            vertex.Position.y = static_cast<float>(index);
        });
        const std::vector<Vertex> points = grid.GetPoints();
        AssertNear(points[0].Position.y, 0.0, 1e-6, "First transformed height should match the callback");
        AssertNear(points[4].Position.y, 4.0, 1e-6, "Middle transformed height should match the callback");
        for (const Vertex& vertex : points) {
            Assert(std::isfinite(vertex.Normal.x) && std::isfinite(vertex.Normal.y) && std::isfinite(vertex.Normal.z), "Normals should remain finite after transforming points");
        }
    });

    AddTest(suite, "grid copy preserves generated data", [] {
        Grid original;
        original.init(3.0f, 5.0f, 4, 4);
        original.GenerateTriangles();
        original.GenerateNormals();

        Grid copy(original);
        AssertEqual(copy.GetPointCount(), original.GetPointCount(), "Copied grid should preserve point count");
        AssertEqual(copy.GetTriangleCount(), original.GetTriangleCount(), "Copied grid should preserve triangle count");
        AssertVec3Near(copy.GetPoints()[3].Position, original.GetPoints()[3].Position, 1e-6f, "Copied grid should preserve point positions");
    });

    AddTest(suite, "terrain generator init configures grid", [] {
        TerrainGenerator terrain(5.0f, 7.0f, 6, 8);
        AssertEqual(terrain.GetGrid().GetResolutionX(), 6u, "Terrain generator should propagate the x resolution");
        AssertEqual(terrain.GetGrid().GetResolutionY(), 8u, "Terrain generator should propagate the z resolution");
        AssertEqual(terrain.GetGrid().GetPointCount(), 48u, "Terrain generator should initialize the grid points");
    });

    return suite;
}

}  // namespace tests
