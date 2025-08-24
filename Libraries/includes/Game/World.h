#pragma once

#include "Camera.h"

#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"


#include "TerrainGenerator.h"
namespace lght
{
    enum LightType { DIRECTIONAL, POINT, SPOT };

    struct Light 
    {
        glm::vec3 position_direction;
        glm::vec3 color;
        float strength;
        LightType type;
    };

    struct AmbiantLight
    {
        glm::vec3 color;
        float strength;
    };
};





class World
{
public:
    World();

    void Init();
    void Update();

    void Render(Camera& camera);

private:
    TerrainGenerator terrain;
    std::vector<lght::Light> Light;
    lght::AmbiantLight AmbiantLight;
};