#pragma once

#include "Camera.h"

#include <cstdint>

#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"

#include "Renderer3D.h"
#include "TerrainGenerator.h"
#include "Light.h"

enum class WorldMode : std::uint8_t
{
    Terrain,
    Empty
};

const char *WorldModeToString(WorldMode mode) noexcept;

class World
{
  public:
    World();
    ~World();

    void Init(WorldMode requestedMode = WorldMode::Terrain);
    void Destroy();

    void Update();

    void Render(Renderer3D &renderer3D, Camera &camera);

    unsigned int GetRenderedChunkCount() const;
    unsigned int GetRenderedTriangleCount() const;

  private:
    WorldMode mode = WorldMode::Terrain;
    TerrainGenerator terrain;
    LightManager lightManager;
};
