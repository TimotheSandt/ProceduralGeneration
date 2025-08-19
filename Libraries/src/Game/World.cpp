#include "World.h"

World::World()
{

}

void World::Init()
{
    this->Light.push_back({glm::vec3(0.7f, 0.8f, 0.7f), 
                    glm::vec3(0.99f, 0.98f, 1.0f),
                    5.0f });
    this->AmbiantLight = {glm::vec3(1.0f, 1.0f, 1.0f), 0.15f};

    this->grid.init(10.0f, 10.0f, 20, 20);
    this->grid.GenerateMesh();

}

void World::Update()
{

}


void World::Render(Camera& camera)
{
    this->grid.Render(camera);
}