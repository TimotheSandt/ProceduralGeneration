#pragma once

#include "Mesh.h"

#include <glm/glm.hpp>

struct SpriteDrawParams
{
    glm::vec2 offset = {0.0f, 0.0f};
    glm::vec2 scale = {1.0f, 1.0f};
    glm::vec2 containerSize = {0.0f, 0.0f};
    glm::vec2 scrollOffset = {0.0f, 0.0f};
    glm::vec2 contentSize = {0.0f, 0.0f};
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

class Sprite
{
  public:
    Sprite();

    void SetShader(const char *vertexPath, const char *fragmentPath) { mesh.SetShader(vertexPath, fragmentPath); }
    bool IsShaderCompiled() const { return mesh.IsShaderCompiled(); }

    void Draw(const SpriteDrawParams &params, const Texture *texture = nullptr);

  private:
    Mesh mesh;
};
