#include "Grid.h"

Grid::Grid() : size_x(1.0f), size_y(1.0f), resolution_x(3), resolution_y(3) {
    this->GeneratePoints();
}

Grid::Grid(float size_x, float size_y, int resolution_x, int resolution_y) : size_x(size_x), size_y(size_y), resolution_x(resolution_x), resolution_y(resolution_y) {
    if (size_x <= 0.0f) size_x = 1.0f;
    if (size_y <= 0.0f) size_y = 1.0f;
    if (resolution_x <= 2) resolution_x = 2;
    if (resolution_y <= 2) resolution_y = 2;
    this->GeneratePoints();
}

Grid::Grid(const Grid& other) : size_x(other.size_x), size_y(other.size_y), resolution_x(other.resolution_x), resolution_y(other.resolution_y), points(other.points), triangles(other.triangles) { }

Grid::~Grid() {
    this->points.clear();
}

void Grid::init(float size_x, float size_y, int resolution_x, int resolution_y) {
    this->size_x = size_x;
    this->size_y = size_y;
    this->resolution_x = resolution_x;
    this->resolution_y = resolution_y;
    this->GeneratePoints();
}

void Grid::GeneratePoints() {
    float step_x = this->size_x / (this->resolution_x - 1);
    float step_y = this->size_y / (this->resolution_y - 1);

    this->points.clear();
    this->points.resize(this->resolution_x * this->resolution_y);
    glm::vec3 center = glm::vec3(this->size_x / 2.0f, 0.0f, this->size_y / 2.0f);
    
    for (unsigned int i = 0; i < this->resolution_x; i++) {
        for (unsigned int j = 0; j < this->resolution_y; j++) {
            glm::vec3 pos = glm::vec3(i * step_x, 0.0f, j * step_y) - center;
            this->points[i * this->resolution_y + j] = pos;
        }
    }
    this->GenerateTriangles();
}

void Grid::GenerateTriangles() {

    this->triangles.clear();
    this->triangles.reserve((this->resolution_x - 1) * (this->resolution_y - 1) * 2);
    
    for (unsigned int i = 0; i < this->resolution_x - 1; i++) {
        for (unsigned int j = 0; j < this->resolution_y - 1; j++) {
             unsigned int p1 = i * this->resolution_y + j;
            unsigned int p2 = (i + 1) * this->resolution_y + j;
            unsigned int p3 = i * this->resolution_y + (j + 1); 
            unsigned int p4 = (i + 1) * this->resolution_y + (j + 1);

            std::array<unsigned int, 3> t1 = { 
                p1,
                p2,
                p3
            };

            std::array<unsigned int, 3> t2 = {
                p2,
                p4,
                p3
            };
            

            this->triangles.push_back(t1);
            this->triangles.push_back(t2);
        }

    }
}

void Grid::GenerateMesh() {
    
    std::vector<GLfloat> vertices;
    vertices.reserve(this->points.size() * 3);
    for (const auto& vec : this->points) {
        vertices.push_back(vec.x);
        vertices.push_back(vec.y);
        vertices.push_back(vec.z);
    }

    std::vector<GLuint> indices;
    indices.reserve(this->triangles.size() * 3);
    for (const auto& triangle : this->triangles) {
        indices.push_back(triangle[0]);
        indices.push_back(triangle[1]);
        indices.push_back(triangle[2]);
    }

    this->mesh.Initialize(vertices, indices, { 3 });
    this->mesh.SetShader("res/shader/default.vert", "res/shader/default.frag");
    this->mesh.InitUniform();
}


void Grid::Render(Camera& camera) {
    this->mesh.Render(camera);
}