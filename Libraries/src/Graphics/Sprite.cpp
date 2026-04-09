#include "Sprite.h"

Sprite::Sprite() { mesh.Initialize({0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f}, {0, 1, 2, 2, 3, 0}, {2}); }

void Sprite::Draw(const SpriteDrawParams &params, const Texture *texture)
{
    if (!mesh.IsShaderCompiled())
    {
        return;
    }

    mesh.BindShader();
    mesh.BindVAO();

    mesh.InitUniform2f("offset", glm::value_ptr(params.offset));
    mesh.InitUniform2f("scale", glm::value_ptr(params.scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(params.containerSize));
    mesh.InitUniform2f("scrollOffset", glm::value_ptr(params.scrollOffset));
    mesh.InitUniform2f("contentSize", glm::value_ptr(params.contentSize));
    mesh.InitUniform4f("color", glm::value_ptr(params.color));

    if (texture != nullptr)
    {
        texture->Bind();
        const int textureSampler = 0;
        mesh.InitUniform1i("textureSampler", &textureSampler);
    }

    mesh.Draw();

    if (texture != nullptr)
    {
        texture->Unbind();
    }

    mesh.UnbindVAO();
    mesh.UnbindShader();
}
