#include "Sprite.h"

Sprite::Sprite() { mesh.Initialize({0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f}, {0, 1, 2, 2, 3, 0}, {2}); }

void Sprite::Shrink(float amount)
{
    const glm::vec2 minimumScale(0.001f);
    scale = glm::max(scale - glm::vec2(amount), minimumScale);
}

void Sprite::Draw(const SpriteDrawParams &params, const Texture *texture)
{
    if (!mesh.IsShaderCompiled())
    {
        return;
    }

    SpriteDrawParams resolvedParams = params;
    resolvedParams.offset += offset;
    resolvedParams.scale *= scale;

    mesh.BindShader();
    mesh.BindGeometry();

    mesh.SetUniform2f("offset", glm::value_ptr(resolvedParams.offset));
    mesh.SetUniform2f("scale", glm::value_ptr(resolvedParams.scale));
    mesh.SetUniform2f("containerSize", glm::value_ptr(resolvedParams.containerSize));
    mesh.SetUniform2f("scrollOffset", glm::value_ptr(resolvedParams.scrollOffset));
    mesh.SetUniform2f("contentSize", glm::value_ptr(resolvedParams.contentSize));
    mesh.SetUniform4f("color", glm::value_ptr(resolvedParams.color));

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
