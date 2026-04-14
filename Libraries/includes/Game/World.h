#pragma once

#include "Camera.h"

#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"

#include "Renderer3D.h"
#include "TerrainGenerator.h"
#include "Light.h"

class World
{
  public:
    World();
    ~World();

    void Init();
    void Destroy();

    void Update();

    void Render(Renderer3D &renderer3D, Camera &camera);

  private:
    TerrainGenerator terrain;
    LightManager lightManager;
};
