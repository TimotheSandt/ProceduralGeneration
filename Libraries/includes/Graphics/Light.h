#pragma once

#include "glm/glm.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

#include "Buffer.h"

namespace lght
{
enum LightType : std::uint8_t
{
    NONE,
    AMBIENT,
    DIRECTIONAL,
    POINT,
    SPOT
};

struct Light
{
    LightType type;

    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 color;

    float strength;

    float outerCone;
    float innerCone;
    float a;
    float b;

    Light(const Light &other) = default;

    bool operator==(const Light &other) const
    {
        if (type != other.type)
        {
            return false;
        }
        switch (type)
        {
            case AMBIENT:
                return (color == other.color && strength == other.strength);
            case DIRECTIONAL:
                return (direction == other.direction && color == other.color && strength == other.strength);
            case POINT:
                return (position == other.position && color == other.color && strength == other.strength && a == other.a && b == other.b);
            case SPOT:
                return (position == other.position && direction == other.direction && color == other.color && strength == other.strength &&
                        outerCone == other.outerCone && innerCone == other.innerCone && a == other.a && b == other.b);
            case NONE:
                return true;
            default:
                return false;
        }
    }

    Light()
    {
        this->type = NONE;
        this->position = glm::vec3(0.0f, 0.0f, 0.0f);
        this->direction = glm::vec3(0.0f, 0.0f, 0.0f);
        this->color = glm::vec3(0.0f, 0.0f, 0.0f);
        this->strength = 0.0f;
        this->outerCone = 0.0f;
        this->innerCone = 0.0f;
        this->a = 0.0f;
        this->b = 0.0f;
    }
};

struct alignas(16) AmbientLightBlock
{
    glm::vec4 color;
    float strength;
    float padding[3];

    AmbientLightBlock() = default;
    AmbientLightBlock(const lght::Light &light) { *this = light; }

    AmbientLightBlock &operator=(const lght::Light &light)
    {
        if (light.type != lght::AMBIENT)
        {
            return *this;
        }
        color = glm::vec4(light.color, 1.0f);
        strength = light.strength;
        return *this;
    }

    bool operator==(const lght::Light &other) const
    {
        if (other.type != lght::AMBIENT)
        {
            return false;
        }
        return this->color == glm::vec4(other.color, 0.0f) && this->strength == other.strength;
    }
};

// NOTE: layout matches the GLSL `LightStruct` declared in default.frag with std430 buffer rules.
// Under std430, vec3 fields occupy 12 bytes but force 16-byte alignment on the *following* member.
// We store them as glm::vec3 so subsequent floats sit at the offsets the shader actually reads from.
// (Earlier this was glm::vec4, which made the shader read `strength` from the unused .w slot of
// `color` — producing wildly wrong lighting values, including all-black meshes.)
struct alignas(16) LightBlock
{
    LightType type;
    float _pad0;     // pad type to 16-byte boundary required by following vec3
    float _pad0b[2];

    glm::vec3 position;
    float _padPositionTail;  // Pad position (vec3, 12B) to 16B before next vec3.

    glm::vec3 direction;
    float _padDirectionTail;

    glm::vec3 color;
    // No padding here: std430 packs `float strength` at offset 60 (immediately after vec3 color
    // at offset 48). The struct's natural ordering already places strength right after color in
    // memory (offset 60), matching the shader.
    float strength;

    float outerCone;
    float innerCone;
    float a;

    float b;
    float _pad2[3];  // Pad struct size to 16-byte boundary (largest member alignment).

    LightBlock() = default;
    LightBlock(const Light &light) { *this = light; }

    LightBlock operator=(const Light &light)
    {
        // Zero all bytes (including padding slots) so the shader doesn't read indeterminate
        // values out of the C++ struct's compiler-inserted padding.
        std::memset(this, 0, sizeof(*this));
        type = light.type;
        position = light.position;
        direction = light.direction;
        color = light.color;
        strength = light.strength;
        outerCone = light.outerCone;
        innerCone = light.innerCone;
        a = light.a;
        b = light.b;
        return *this;
    }

    bool operator==(const LightBlock &other) const
    {
        if (type != other.type)
        {
            return false;
        }
        switch (type)
        {
            case AMBIENT:
                return (color == other.color && strength == other.strength);
            case DIRECTIONAL:
                return (direction == other.direction && color == other.color && strength == other.strength);
            case POINT:
                return (position == other.position && color == other.color && strength == other.strength && a == other.a && b == other.b);
            case SPOT:
                return (position == other.position && direction == other.direction && color == other.color && strength == other.strength &&
                        outerCone == other.outerCone && innerCone == other.innerCone && a == other.a && b == other.b);
            case NONE:
                return true;
            default:
                return false;
        }
    }
};

// Factory functions - now with proper default initialization
inline Light NoneLight()
{
    Light light;
    light.type = LightType::NONE;
    return light;
}

inline Light DirectionalLight(glm::vec3 direction, glm::vec3 color, float strength)
{
    Light light;
    light.type = LightType::DIRECTIONAL;
    light.direction = direction;
    light.color = color;
    light.strength = strength;
    return light;
}

inline Light PointLight(glm::vec3 position, glm::vec3 color, float strength, float a, float b)
{
    Light light;
    light.type = LightType::POINT;
    light.position = position;
    light.color = color;
    light.strength = strength;
    light.a = a;
    light.b = b;
    return light;
}

inline Light SpotLight(glm::vec3 position, glm::vec3 direction, glm::vec3 color, float strength, float outerCone, float innerCone, float a,
                       float b)
{
    Light light;
    light.type = LightType::SPOT;
    light.position = position;
    light.direction = direction;
    light.color = color;
    light.strength = strength;
    light.outerCone = outerCone;
    light.innerCone = innerCone;
    light.a = a;
    light.b = b;
    return light;
}

inline Light AmbientLight(glm::vec3 color, float strength)
{
    Light light;
    light.type = LightType::AMBIENT;
    light.color = color;
    light.strength = strength;
    return light;
}
}; // namespace lght

class LightManager
{
  public:
    LightManager();
    LightManager(glm::vec3 color, float strength);

    LightManager(const LightManager &);
    LightManager &operator=(const LightManager &);

    LightManager(LightManager &&) noexcept;
    LightManager &operator=(LightManager &&) noexcept;

    ~LightManager();

    void Destroy();

    void Initialize();
    void UploadChanges();
    void Bind() const;

    void AddLight(lght::Light Light);
    void AddLight(std::vector<lght::Light> Light);
    void SetAmbientLight(glm::vec3 color, float strength);

    void RemoveLight(size_t index);
    void RemoveLight(lght::Light Light);
    void RemoveLight(std::vector<lght::Light> Light);
    void ResetAmbientLight();

    void SetLight(size_t index, lght::Light Light);

    lght::Light &operator[](size_t index);

    lght::Light &GetLight(size_t index);
    std::vector<lght::Light> &GetLight();
    lght::Light &GetAmbientLight();

  private:
    void Swap(LightManager &other);

  private:
    std::vector<lght::Light> lLight;
    lght::Light ambientLight;
    int size = 0;

    Buffer lightBuffer;

  private:
    std::vector<bool> LightChanged;
    bool LightsChanged = true;
    bool AmbientLightChanged = true;
};
