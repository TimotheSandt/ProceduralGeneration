#pragma once

#include "Camera.h"

#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"


#include "TerrainGenerator.h"
#include "Light.h"




class World
{
public:
    World();

    void Init();
    void Update();

    void Render(Camera& camera);

private:
    TerrainGenerator terrain;
    LightManager lightManager;
};