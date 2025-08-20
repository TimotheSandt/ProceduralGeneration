#include "Grid.h"

Grid::Grid() : size_x(1.0f), size_z(1.0f), resolution_x(3), resolution_z(3) {
    this->GeneratePoints();
}

Grid::Grid(float size_x, float size_z, int resolution_x, int resolution_z) : size_x(size_x), size_z(size_z), resolution_x(resolution_x), resolution_z(resolution_z) {
    if (size_x <= 0.0f) size_x = 1.0f;
    if (size_z <= 0.0f) size_z = 1.0f;
    if (resolution_x <= 2) resolution_x = 2;
    if (resolution_z <= 2) resolution_z = 2;
    this->GeneratePoints();
}

Grid::Grid(const Grid& other) : size_x(other.size_x), size_z(other.size_z), resolution_x(other.resolution_x), resolution_z(other.resolution_z), points(other.points), triangles(other.triangles) { }

Grid::~Grid() {
    this->points.clear();
}

void Grid::init(float size_x, float size_z, int resolution_x, int resolution_z) {
    this->size_x = size_x;
    this->size_z = size_z;
    this->resolution_x = resolution_x;
    this->resolution_z = resolution_z;

    this->GeneratePoints();
    this->GenerateTriangles();
    this->GenerateNormals();
}

void Grid::GeneratePoints() {
    float step_x = this->size_x / (this->resolution_x - 1);
    float step_z = this->size_z / (this->resolution_z - 1);

    this->points.clear();
    this->points.resize(this->resolution_x * this->resolution_z);
    glm::vec3 center = glm::vec3(this->size_x / 2.0f, 0.0f, this->size_z / 2.0f);
    
    for (unsigned int i = 0; i < this->resolution_x; i++) {
        for (unsigned int j = 0; j < this->resolution_z; j++) {
            glm::vec3 pos = glm::vec3(i * step_x, 0.0f, j * step_z) - center;
            this->points[i * this->resolution_z + j].Position = pos;
        }
    }
}

void Grid::GenerateTriangles() {

    this->triangles.clear();
    this->triangles.reserve((this->resolution_x - 1) * (this->resolution_z - 1) * 2);
    
    for (unsigned int i = 0; i < this->resolution_x - 1; i++) {
        for (unsigned int j = 0; j < this->resolution_z - 1; j++) {
             unsigned int p1 = i * this->resolution_z + j;
            unsigned int p2 = (i + 1) * this->resolution_z + j;
            unsigned int p3 = i * this->resolution_z + (j + 1); 
            unsigned int p4 = (i + 1) * this->resolution_z + (j + 1);

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

void Grid::GenerateNormals() {

    for (auto& vec : this->points) {
        vec.Normal = glm::vec3(0.0f);
    }
    
    for (const auto& triangle : this->triangles) {
        glm::vec3 p1 = this->points[triangle[0]].Position;
        glm::vec3 p2 = this->points[triangle[1]].Position;
        glm::vec3 p3 = this->points[triangle[2]].Position;
        glm::vec3 normal = glm::normalize(glm::cross(p2 - p1, p3 - p1));

        this->points[triangle[0]].Normal += normal;
        this->points[triangle[1]].Normal += normal;
        this->points[triangle[2]].Normal += normal;
    }

    for (auto& vec : this->points) {
        vec.Normal = glm::normalize(vec.Normal);
    }
}

void Grid::GenerateMesh() {
    
    std::vector<GLfloat> vertices;
    vertices.reserve(this->points.size() * 3);
    for (const auto& vec : this->points) {
        vertices.push_back(vec.Position.x);
        vertices.push_back(vec.Position.y);
        vertices.push_back(vec.Position.z);

        vertices.push_back(vec.Normal.x);
        vertices.push_back(vec.Normal.y);
        vertices.push_back(vec.Normal.z);
    }

    std::vector<GLuint> indices;
    indices.reserve(this->triangles.size() * 3);
    for (const auto& triangle : this->triangles) {
        indices.push_back(triangle[0]);
        indices.push_back(triangle[1]);
        indices.push_back(triangle[2]);
    }

    this->mesh.Initialize(vertices, indices, { 3, 3 });
    this->mesh.SetShader("res/shader/default.vert", "res/shader/default.frag");
    this->mesh.InitUniform();
}


void Grid::TransformPoints(std::function<void(Vertex&, unsigned int)> func) {
    for (int i = 0; i < points.size(); ++i) {
        func(points[i], i);
    }
    GenerateNormals();
}



void Grid::Render(Camera& camera) {
    this->mesh.Render(camera);
}