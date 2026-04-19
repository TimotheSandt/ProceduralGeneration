#pragma once

#include <glm/vec2.hpp>
#include <string>

class Texture;

enum class UpscaleQualityMode : std::uint8_t
{
    Native = 0,
    Quality,
    Balanced,
    Performance,
    UltraPerformance
};

enum class UpscaleAlphaMode : std::uint8_t
{
    Opaque = 0,
    Straight,
    Premultiplied
};

enum class FrameGenerationQualityMode : std::uint8_t
{
    Off = 0,
    On
};

struct UpscaleRequirements
{
    bool needsColor = true;
    bool needsDepth = false;
    bool needsMotionVectors = false;
    bool needsExposure = false;
    bool needsReactiveMask = false;
    bool needsTransparencyMask = false;
    bool needsJitter = false;
    bool needsHistory = false;
};

struct UpscaleInput
{
    const Texture *color = nullptr;
    const Texture *depth = nullptr;
    const Texture *motionVectors = nullptr;
    const Texture *exposure = nullptr;
    const Texture *reactiveMask = nullptr;
    const Texture *transparencyMask = nullptr;
    const Texture *historyColor = nullptr;
    glm::vec2 renderResolution = {0.0f, 0.0f};
    glm::vec2 outputResolution = {0.0f, 0.0f};
    glm::vec2 jitter = {0.0f, 0.0f};
    float sharpness = 0.0f;
    float deltaTimeSeconds = 0.0f;
    UpscaleAlphaMode alphaMode = UpscaleAlphaMode::Opaque;
    bool resetHistory = false;
};

struct UpscaleOutput
{
    Texture *output = nullptr;
    Texture *newHistoryColor = nullptr;
};

struct FrameGenerationInput
{
    const Texture *currentColor = nullptr;
    const Texture *previousColor = nullptr;   // previous frame's color at render resolution
    const Texture *depth = nullptr;           // current frame depth (requires depthAsTexture)
    const Texture *motionVectors = nullptr;   // current frame motion vectors (requires motionVectors attachment)
    glm::vec2 renderResolution = {0.0f, 0.0f};
    glm::vec2 outputResolution = {0.0f, 0.0f};
    glm::vec2 jitter = {0.0f, 0.0f};         // sub-pixel jitter applied this frame
    float deltaTimeSeconds = 0.0f;
    bool resetHistory = false;                // true on scene cuts / camera teleports
};

struct FrameGenerationOutput
{
    Texture *generatedFrame = nullptr;
};

struct UpscaleModeDesc
{
    std::string name{};
    UpscaleQualityMode quality = UpscaleQualityMode::Native;
    UpscaleRequirements requirements{};
    bool supportsReactiveMask = false;
    bool supportsTransparencyMask = false;
    bool supportsAlphaComposition = false;
    bool supportsJitter = false;
};

struct FrameGenerationModeDesc
{
    std::string name{};
    FrameGenerationQualityMode quality = FrameGenerationQualityMode::Off;
    bool needsDepth = false;
    bool needsMotionVectors = true;
    bool needsHistory = true;
};
