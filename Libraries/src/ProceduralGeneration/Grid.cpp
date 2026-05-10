#include "Grid.h"

#include "utilities.h"

#include <algorithm>
#include <limits>

namespace
{

constexpr unsigned int TerrainChunkCellSize = 64;

struct FrustumPlane
{
    glm::vec3 normal = glm::vec3(0.0f);
    float distance = 0.0f;
};

FrustumPlane NormalizePlane(FrustumPlane plane)
{
    const float length = glm::length(plane.normal);
    if (length <= 1e-6f)
    {
        return plane;
    }
    plane.normal /= length;
    plane.distance /= length;
    return plane;
}

std::array<FrustumPlane, 6> ExtractFrustumPlanes(const glm::mat4 &matrix)
{
    return {
        NormalizePlane(
            {{matrix[0][3] + matrix[0][0], matrix[1][3] + matrix[1][0], matrix[2][3] + matrix[2][0]}, matrix[3][3] + matrix[3][0]}),
        NormalizePlane(
            {{matrix[0][3] - matrix[0][0], matrix[1][3] - matrix[1][0], matrix[2][3] - matrix[2][0]}, matrix[3][3] - matrix[3][0]}),
        NormalizePlane(
            {{matrix[0][3] + matrix[0][1], matrix[1][3] + matrix[1][1], matrix[2][3] + matrix[2][1]}, matrix[3][3] + matrix[3][1]}),
        NormalizePlane(
            {{matrix[0][3] - matrix[0][1], matrix[1][3] - matrix[1][1], matrix[2][3] - matrix[2][1]}, matrix[3][3] - matrix[3][1]}),
        NormalizePlane(
            {{matrix[0][3] + matrix[0][2], matrix[1][3] + matrix[1][2], matrix[2][3] + matrix[2][2]}, matrix[3][3] + matrix[3][2]}),
        NormalizePlane(
            {{matrix[0][3] - matrix[0][2], matrix[1][3] - matrix[1][2], matrix[2][3] - matrix[2][2]}, matrix[3][3] - matrix[3][2]}),
    };
}

bool IsBoundsVisible(const glm::vec3 &minBounds, const glm::vec3 &maxBounds, const std::array<FrustumPlane, 6> &planes)
{
    for (const FrustumPlane &plane : planes)
    {
        const glm::vec3 positiveVertex = {
            plane.normal.x >= 0.0f ? maxBounds.x : minBounds.x,
            plane.normal.y >= 0.0f ? maxBounds.y : minBounds.y,
            plane.normal.z >= 0.0f ? maxBounds.z : minBounds.z,
        };

        if (glm::dot(plane.normal, positiveVertex) + plane.distance < 0.0f)
        {
            return false;
        }
    }

    return true;
}

void AppendVertex(std::vector<float> &vertices, const Vertex &vertex)
{
    vertices.push_back(vertex.Position.x);
    vertices.push_back(vertex.Position.y);
    vertices.push_back(vertex.Position.z);

    vertices.push_back(vertex.Normal.x);
    vertices.push_back(vertex.Normal.y);
    vertices.push_back(vertex.Normal.z);

    vertices.push_back(vertex.Color.x);
    vertices.push_back(vertex.Color.y);
    vertices.push_back(vertex.Color.z);
}

} // namespace

Grid::Grid() : size_x(1.0f), size_z(1.0f), resolution_x(3), resolution_z(3) { this->GeneratePoints(); }

Grid::Grid(float size_x, float size_z, int resolution_x, int resolution_z)
    : size_x(size_x), size_z(size_z), resolution_x(resolution_x), resolution_z(resolution_z)
{
    if (size_x <= 0.0f)
    {
        size_x = 1.0f;
    }
    if (size_z <= 0.0f)
    {
        size_z = 1.0f;
    }
    if (resolution_x <= 2)
    {
        resolution_x = 2;
    }
    if (resolution_z <= 2)
    {
        resolution_z = 2;
    }
    this->GeneratePoints();
}

Grid::Grid(const Grid &other)
    : size_x(other.size_x), size_z(other.size_z), resolution_x(other.resolution_x), resolution_z(other.resolution_z), points(other.points),
      triangles(other.triangles)
{
}

Grid::~Grid() { this->Destroy(); }

void Grid::Destroy()
{
    this->DestroyRenderMeshes();
    this->points.clear();
    this->triangles.clear();
}

void Grid::init(float size_x, float size_z, int resolution_x, int resolution_z)
{
    this->size_x = size_x;
    this->size_z = size_z;
    this->resolution_x = resolution_x;
    this->resolution_z = resolution_z;

    this->GeneratePoints();
    this->GenerateTriangles();
    this->GenerateNormals();
}

void Grid::GeneratePoints()
{
    float step_x = this->size_x / static_cast<float>(this->resolution_x - 1);
    float step_z = this->size_z / static_cast<float>(this->resolution_z - 1);

    this->points.clear();
    this->points.resize(this->resolution_x * this->resolution_z);
    glm::vec3 center = glm::vec3(this->size_x / 2.0f, 0.0f, this->size_z / 2.0f);

    for (unsigned int i = 0; i < this->resolution_x; i++)
    {
        for (unsigned int j = 0; j < this->resolution_z; j++)
        {
            glm::vec3 pos = glm::vec3(static_cast<float>(i) * step_x, 0.0f, static_cast<float>(j) * step_z) - center;
            Vertex &point = this->points[i * this->resolution_z + j];
            point.Position = pos;
            point.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            point.Color = glm::vec3(0.45f, 0.7f, 0.35f);
        }
    }
}

void Grid::GenerateTriangles()
{

    this->triangles.clear();
    this->triangles.reserve((this->resolution_x - 1) * (this->resolution_z - 1) * 2);

    for (unsigned int i = 0; i < this->resolution_x - 1; i++)
    {
        for (unsigned int j = 0; j < this->resolution_z - 1; j++)
        {
            unsigned int p1 = i * this->resolution_z + j;
            unsigned int p2 = (i + 1) * this->resolution_z + j;
            unsigned int p3 = i * this->resolution_z + (j + 1);
            unsigned int p4 = (i + 1) * this->resolution_z + (j + 1);

            std::array<unsigned int, 3> t1 = {p1, p2, p3};

            std::array<unsigned int, 3> t2 = {p2, p4, p3};

            this->triangles.push_back(t1);
            this->triangles.push_back(t2);
        }
    }
}

void Grid::GenerateNormals()
{

    for (auto &vec : this->points)
    {
        vec.Normal = glm::vec3(0.0f);
    }

    for (const auto &triangle : this->triangles)
    {
        glm::vec3 p1 = this->points[triangle[0]].Position;
        glm::vec3 p2 = this->points[triangle[1]].Position;
        glm::vec3 p3 = this->points[triangle[2]].Position;
        glm::vec3 normal = glm::normalize(glm::cross(p2 - p1, p3 - p1));

        this->points[triangle[0]].Normal += normal;
        this->points[triangle[1]].Normal += normal;
        this->points[triangle[2]].Normal += normal;
    }

    for (auto &vec : this->points)
    {
        vec.Normal = glm::normalize(vec.Normal);
    }
}

void Grid::DestroyRenderMeshes()
{
    this->mesh.Destroy();
    for (Chunk &chunk : this->chunks)
    {
        chunk.mesh.Destroy();
    }
    this->chunks.clear();
    this->lastRenderedChunkCount = 0;
    this->lastRenderedTriangleCount = 0;
}

void Grid::GenerateMesh()
{
    this->DestroyRenderMeshes();

    const unsigned int cellCountX = this->resolution_x > 0 ? this->resolution_x - 1 : 0;
    const unsigned int cellCountZ = this->resolution_z > 0 ? this->resolution_z - 1 : 0;
    if (cellCountX > TerrainChunkCellSize || cellCountZ > TerrainChunkCellSize)
    {
        this->GenerateChunkedMesh();
        return;
    }

    this->GenerateSingleMesh();
}

void Grid::GenerateSingleMesh()
{

    std::vector<float> vertices;
    vertices.reserve(this->points.size() * 9);
    for (const auto &vec : this->points)
    {
        AppendVertex(vertices, vec);
    }

    std::vector<std::uint32_t> indices;
    indices.reserve(this->triangles.size() * 3);
    for (const auto &triangle : this->triangles)
    {
        indices.push_back(triangle[0]);
        indices.push_back(triangle[1]);
        indices.push_back(triangle[2]);
    }

    this->mesh.Initialize(vertices, indices, {3, 3, 3});
    this->mesh.SetShader(GET_RESOURCE_PATH("shader/default.vert"), GET_RESOURCE_PATH("shader/default.frag"));
    this->mesh.UploadTransform();
}

void Grid::GenerateChunkedMesh()
{
    ShaderProgram terrainShader;
    terrainShader.SetShader(GET_RESOURCE_PATH("shader/default.vert"), GET_RESOURCE_PATH("shader/default.frag"));

    const unsigned int cellCountX = this->resolution_x - 1;
    const unsigned int cellCountZ = this->resolution_z - 1;
    const unsigned int chunkCountX = (cellCountX + TerrainChunkCellSize - 1) / TerrainChunkCellSize;
    const unsigned int chunkCountZ = (cellCountZ + TerrainChunkCellSize - 1) / TerrainChunkCellSize;
    this->chunks.reserve(static_cast<std::size_t>(chunkCountX) * static_cast<std::size_t>(chunkCountZ));

    for (unsigned int chunkX = 0; chunkX < chunkCountX; ++chunkX)
    {
        const unsigned int startX = chunkX * TerrainChunkCellSize;
        const unsigned int endX = std::min(startX + TerrainChunkCellSize, cellCountX);

        for (unsigned int chunkZ = 0; chunkZ < chunkCountZ; ++chunkZ)
        {
            const unsigned int startZ = chunkZ * TerrainChunkCellSize;
            const unsigned int endZ = std::min(startZ + TerrainChunkCellSize, cellCountZ);
            const unsigned int localResolutionZ = endZ - startZ + 1;
            const unsigned int localResolutionX = endX - startX + 1;

            std::vector<float> vertices;
            vertices.reserve(static_cast<std::size_t>(localResolutionX) * static_cast<std::size_t>(localResolutionZ) * 9);
            glm::vec3 minBounds(std::numeric_limits<float>::max());
            glm::vec3 maxBounds(std::numeric_limits<float>::lowest());

            for (unsigned int x = startX; x <= endX; ++x)
            {
                for (unsigned int z = startZ; z <= endZ; ++z)
                {
                    const Vertex &vertex = this->points[x * this->resolution_z + z];
                    AppendVertex(vertices, vertex);
                    minBounds = glm::min(minBounds, vertex.Position);
                    maxBounds = glm::max(maxBounds, vertex.Position);
                }
            }

            std::vector<std::uint32_t> indices;
            indices.reserve(static_cast<std::size_t>(endX - startX) * static_cast<std::size_t>(endZ - startZ) * 6);
            for (unsigned int x = startX; x < endX; ++x)
            {
                for (unsigned int z = startZ; z < endZ; ++z)
                {
                    const std::uint32_t p1 = (x - startX) * localResolutionZ + (z - startZ);
                    const std::uint32_t p2 = (x - startX + 1) * localResolutionZ + (z - startZ);
                    const std::uint32_t p3 = (x - startX) * localResolutionZ + (z - startZ + 1);
                    const std::uint32_t p4 = (x - startX + 1) * localResolutionZ + (z - startZ + 1);

                    indices.push_back(p1);
                    indices.push_back(p2);
                    indices.push_back(p3);
                    indices.push_back(p2);
                    indices.push_back(p4);
                    indices.push_back(p3);
                }
            }

            Chunk chunk;
            chunk.minBounds = minBounds;
            chunk.maxBounds = maxBounds;
            chunk.triangleCount = static_cast<unsigned int>(indices.size() / 3);
            chunk.mesh.Initialize(std::move(vertices), std::move(indices), {3, 3, 3});
            chunk.mesh.SetShaderCopy(terrainShader);
            chunk.mesh.UploadTransform();
            this->chunks.push_back(std::move(chunk));
        }
    }

    LOG_INFO("Generated chunked terrain: chunks=", this->chunks.size(), ", triangles=", this->triangles.size());
}

void Grid::TransformPoints(const std::function<void(Vertex &, unsigned int)> &func)
{
    for (unsigned int i = 0; i < points.size(); ++i)
    {
        func(points[i], i);
    }
    GenerateNormals();
}

void Grid::Render(const Renderer3D &renderer3D, Camera &camera)
{
    if (this->chunks.empty())
    {
        this->lastRenderedChunkCount = this->mesh.IsShaderCompiled() ? 1 : 0;
        this->lastRenderedTriangleCount = this->lastRenderedChunkCount > 0 ? static_cast<unsigned int>(this->triangles.size()) : 0;
        renderer3D.DrawMesh(this->mesh, camera);
        return;
    }

    const std::array<FrustumPlane, 6> frustumPlanes = ExtractFrustumPlanes(camera.GetMatrix());
    this->lastRenderedChunkCount = 0;
    this->lastRenderedTriangleCount = 0;
    for (Chunk &chunk : this->chunks)
    {
        if (!IsBoundsVisible(chunk.minBounds, chunk.maxBounds, frustumPlanes))
        {
            continue;
        }

        renderer3D.DrawMesh(chunk.mesh, camera);
        ++this->lastRenderedChunkCount;
        this->lastRenderedTriangleCount += chunk.triangleCount;
    }
}
