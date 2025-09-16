#pragma once

#include "glm/glm.hpp"

#include <vector>

#include "SSBO.h"
#include "UBO.h"

namespace lght
{
    enum LightType { NONE, AMBIENT, DIRECTIONAL, POINT, SPOT };

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

        Light(const Light& other) = default;

        bool operator==(const Light& other) const
        {
            if (type != other.type) return false;
            switch (type)
            {
                case AMBIENT: 
                    return (
                        color == other.color && 
                        strength == other.strength
                    );
                case DIRECTIONAL: 
                    return (
                        direction == other.direction && 
                        color == other.color && 
                        strength == other.strength
                    );
                case POINT: 
                    return (
                        position == other.position && 
                        color == other.color && 
                        strength == other.strength && 
                        a == other.a && 
                        b == other.b
                    );
                case SPOT: 
                    return (
                        position == other.position && 
                        direction == other.direction && 
                        color == other.color && 
                        strength == other.strength && 
                        outerCone == other.outerCone && 
                        innerCone == other.innerCone && 
                        a == other.a && 
                        b == other.b
                    );
                case NONE: return true;
                default: return false;
            }
        }

        Light() {
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

    struct alignas(16) AmbientLightBlock {
        glm::vec4 color;
        float strength;
        float padding[3];

        AmbientLightBlock() = default;
        AmbientLightBlock(const lght::Light& light) { *this = light; }

        AmbientLightBlock& operator=(const lght::Light& light) { 
            if (light.type != lght::AMBIENT) return *this;           
            color = glm::vec4(light.color, 1.0f);
            strength = light.strength;
            return *this;
        }

        bool operator==(const lght::Light& other) const {
            if (other.type != lght::AMBIENT) return false;
            return this->color == glm::vec4(other.color, 0.0f) && this->strength == other.strength;
        }
    };

    
    struct alignas(16) LightBlock {
        LightType type;
        float _pad1[3]; 
        
        glm::vec4 position;
        glm::vec4 direction;
        glm::vec4 color;

        float strength;
        float outerCone;
        float innerCone;
        
        float a;
        float b;
        float _pad2[3];       

        LightBlock() = default;
        LightBlock(const Light& light) { *this = light; }

        LightBlock operator=(const Light& light)
        {
            type = light.type;
            position = glm::vec4(light.position, 1.0f);
            direction = glm::vec4(light.direction, 0.0f);
            color = glm::vec4(light.color, 1.0f);
            strength = light.strength;
            outerCone = light.outerCone;
            innerCone = light.innerCone;
            a = light.a;
            b = light.b;
            return *this;
        }

        bool operator==(const LightBlock& other) const
        {
            if (type != other.type) return false;
            switch (type)
            {
                case AMBIENT: 
                    return (
                        color == other.color && 
                        strength == other.strength
                    );
                case DIRECTIONAL: 
                    return (
                        direction == other.direction && 
                        color == other.color && 
                        strength == other.strength
                    );
                case POINT: 
                    return (
                        position == other.position && 
                        color == other.color && 
                        strength == other.strength && 
                        a == other.a && 
                        b == other.b
                    );
                case SPOT: 
                    return (
                        position == other.position && 
                        direction == other.direction && 
                        color == other.color && 
                        strength == other.strength && 
                        outerCone == other.outerCone && 
                        innerCone == other.innerCone && 
                        a == other.a && 
                        b == other.b
                    );
                case NONE: return true;
                default: return false;
            }
        }
    };

    // Factory functions - now with proper default initialization
    inline Light NoneLight() {
        Light light;
        light.type = LightType::NONE;
        return light;
    }

    inline Light DirectionalLight(glm::vec3 direction, glm::vec3 color, float strength) {
        Light light;
        light.type = LightType::DIRECTIONAL;
        light.direction = direction;
        light.color = color;
        light.strength = strength;
        return light;
    }

    inline Light PointLight(glm::vec3 position, glm::vec3 color, float strength, float a, float b) {
        Light light;
        light.type = LightType::POINT;
        light.position = position;
        light.color = color;
        light.strength = strength;
        light.a = a;
        light.b = b;
        return light;
    }

    inline Light SpotLight(glm::vec3 position, glm::vec3 direction, glm::vec3 color, float strength, float outerCone, float innerCone, float a, float b) {
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

    inline Light AmbientLight(glm::vec3 color, float strength) {
        Light light;
        light.type = LightType::AMBIENT;
        light.color = color;
        light.strength = strength;
        return light;
    }
};


class LightManager
{
public:
    LightManager();
    LightManager(glm::vec3 color, float strength);
    ~LightManager();


    void initSSBO();
    void updateSSBO();
    void BindSSBO();

    void AddLight(lght::Light Light);
    void AddLight(std::vector<lght::Light> Light);
    void SetAmbientLight(glm::vec3 color, float strength);

    void RemoveLight(size_t index);
    void RemoveLight(lght::Light Light);
    void RemoveLight(std::vector<lght::Light> Light);
    void ResetAmbientLight();

    void SetLight(size_t index, lght::Light Light);

    lght::Light& operator[](size_t index);
    

    lght::Light& GetLight(size_t index);
    std::vector<lght::Light> GetLight();
    lght::Light& GetAmbientLight();

private:
    std::vector<lght::Light> lLight;
    lght::Light ambientLight;
    int size = 0;

    SSBO LightSSBO;

private:
    std::vector<bool> LightChanged;
    bool LightsChanged = true;
    bool AmbientLightChanged = true;
};
