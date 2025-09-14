#include "World.h"

World::World()
{

}

void World::Init()
{
    this->lightManager.initSSBO();
    this->lightManager.SetAmbientLight(glm::vec3(1.0f, 1.0f, 1.0f), 0.2f);
    this->lightManager.AddLight(
                    lght::DirectionalLight(
                        glm::vec3(2.0f, -3.0f, 0.5f),
                        glm::vec3(0.99f, 0.76f, 0.81f),
                        1.0f
                    ));
    

    this->lightManager.updateSSBO();
    

    this->terrain.init(500.0f, 500.0f, 500, 500);
    LOG_DEBUGGING("Triangle count: ", this->terrain.GetGrid().GetTriangleCount());
    // this->terrain.GenerateRandomTerrain(2.0f);
    this->terrain.GenerateFractalTerrain(0.01f, 50.0f, 10, 0.5f, 2.0f);

}

void World::Update()
{

}


void World::Render(Camera& camera)
{
    this->lightManager.BindSSBO();
    this->terrain.Render(camera);
}