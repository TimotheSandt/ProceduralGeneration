#include "Sprite.h"

Sprite::Sprite() { mesh.Initialize({0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f}, {0, 1, 2, 2, 3, 0}, {2}); }

void Sprite::Draw(const SpriteDrawParams &params, const Texture *texture)
{
    if (!mesh.IsShaderCompiled())
    {
        return;
    }

    mesh.BindShader();
    mesh.BindGeometry();

    mesh.SetUniform2f("offset", glm::value_ptr(params.offset));
    mesh.SetUniform2f("scale", glm::value_ptr(params.scale));
    mesh.SetUniform2f("containerSize", glm::value_ptr(params.containerSize));
    mesh.SetUniform2f("scrollOffset", glm::value_ptr(params.scrollOffset));
    mesh.SetUniform2f("contentSize", glm::value_ptr(params.contentSize));
    mesh.SetUniform4f("color", glm::value_ptr(params.color));

    if (texture != nullptr)
    {
        texture->Bind();
        const int textureSampler = 0;
        mesh.SetUniform1i("textureSampler", &textureSampler);
    }

    mesh.Draw();

    if (texture != nullptr)
    {
        texture->Unbind();
    }

    mesh.UnbindGeometry();
    mesh.UnbindShader();
}
