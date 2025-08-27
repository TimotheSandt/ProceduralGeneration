#include "World.h"

World::World()
{

}

void World::Init()
{
    this->Light.push_back(
                {
                    glm::vec3(1.0f, -3.0f, 0.5f),
                    glm::vec3(0.99f, 0.98f, 1.0f),
                    1.0f,
                    lght::LightType::DIRECTIONAL
                }
            );
    this->AmbiantLight = {glm::vec3(1.0f, 1.0f, 1.0f), 0.15f};

    this->terrain.init(500.0f, 500.0f, 500, 500);
    // this->terrain.GenerateRandomTerrain(2.0f);
    this->terrain.GenerateFractalTerrain(0.01f, 50.0f, 10, 0.5f, 2.0f);

}

void World::Update()
{

}


void World::Render(Camera& camera)
{
    if (this->Light[0].type == lght::LightType::DIRECTIONAL) {
        this->terrain.GetMesh().InitUniform3f("SunDirection", glm::value_ptr(this->Light[0].position_direction));
        this->terrain.GetMesh().InitUniform3f("SunColor", glm::value_ptr(this->Light[0].color));
        this->terrain.GetMesh().InitUniform1f("SunIntensity", &this->Light[0].strength);
    }    
    this->terrain.GetMesh().InitUniform3f("ambientColor", glm::value_ptr(this->AmbiantLight.color * this->AmbiantLight.strength));
    this->terrain.GetMesh().InitUniform3f("camPos", glm::value_ptr(camera.GetPosition()));

    this->terrain.Render(camera);
}