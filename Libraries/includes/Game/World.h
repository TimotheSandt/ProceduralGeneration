#pragma once

#include "Camera.h"

#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"


#include "Grid.h"


struct Light 
{
    glm::ivec3 position;
    glm::vec3 color;
    float strength;
};

struct AmbiantLight
{
    glm::vec3 color;
    float strength;
};


class World
{
public:
    World();

    void Init();
    void Update();

    void Render(Camera& camera);

private:
    Grid grid = Grid{};
    std::vector<Light> Light;
    AmbiantLight AmbiantLight;
};