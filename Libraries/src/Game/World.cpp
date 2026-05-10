#include "World.h"

World::World() {}

World::~World() { this->Destroy(); }

const char *WorldModeToString(WorldMode mode) noexcept
{
    switch (mode)
    {
        case WorldMode::Terrain:
            return "current";
        case WorldMode::Empty:
            return "empty";
    }
    return "unknown";
}

void World::Init(WorldMode requestedMode)
{
    this->mode = requestedMode;
    if (this->mode == WorldMode::Empty)
    {
        LOG_INFO("World initialized in empty benchmark mode");
        return;
    }

    this->lightManager.Initialize();
    this->lightManager.SetAmbientLight(glm::vec3(1.0f, 1.0f, 1.0f), 0.2f);
    this->lightManager.AddLight(lght::DirectionalLight(glm::vec3(2.0f, -3.0f, 0.5f), glm::vec3(0.99f, 0.76f, 0.81f), 1.0f));

    this->lightManager.UploadChanges();

    this->terrain.init(500.0f, 500.0f, 500, 500);
    LOG_DEBUGGING("Triangle count: ", this->terrain.GetGrid().GetTriangleCount());
    // this->terrain.GenerateRandomTerrain(2.0f);
    this->terrain.GenerateFractalTerrain(0.01f, 50.0f, 10, 0.5f, 2.0f);
    LOG_INFO("World terrain ready: chunks=", this->terrain.GetGrid().GetChunkCount(),
             ", triangles=", this->terrain.GetGrid().GetTriangleCount());
}

void World::Destroy()
{
    this->terrain.Destroy();
    this->lightManager.Destroy();
}

void World::Update() {}

void World::Render(Renderer3D &renderer3D, Camera &camera)
{
    if (this->mode == WorldMode::Empty)
    {
        return;
    }

    this->lightManager.Bind();
    this->terrain.Render(renderer3D, camera);
}

unsigned int World::GetRenderedChunkCount() const
{
    if (this->mode == WorldMode::Empty)
    {
        return 0;
    }
    return this->terrain.GetGrid().GetLastRenderedChunkCount();
}

unsigned int World::GetRenderedTriangleCount() const
{
    if (this->mode == WorldMode::Empty)
    {
        return 0;
    }
    return this->terrain.GetGrid().GetLastRenderedTriangleCount();
}
