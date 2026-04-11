#pragma once

#include "Graphics/Core/GraphicsResources.h"

#include <glm/vec2.hpp>
#include <string_view>

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
    const ITextureResource *color = nullptr;
    const ITextureResource *depth = nullptr;
    const ITextureResource *motionVectors = nullptr;
    const ITextureResource *exposure = nullptr;
    const ITextureResource *reactiveMask = nullptr;
    const ITextureResource *transparencyMask = nullptr;
    const ITextureResource *historyColor = nullptr;
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
    ITextureResource *output = nullptr;
    ITextureResource *newHistoryColor = nullptr;
};

struct FrameGenerationInput
{
    const ITextureResource *currentColor = nullptr;
    const ITextureResource *previousColor = nullptr;
    const ITextureResource *depth = nullptr;
    const ITextureResource *motionVectors = nullptr;
    glm::vec2 renderResolution = {0.0f, 0.0f};
    glm::vec2 outputResolution = {0.0f, 0.0f};
    float deltaTimeSeconds = 0.0f;
    bool resetHistory = false;
};

struct FrameGenerationOutput
{
    ITextureResource *generatedFrame = nullptr;
};

struct UpscaleModeDesc
{
    std::string_view name{};
    UpscaleQualityMode quality = UpscaleQualityMode::Native;
    UpscaleRequirements requirements{};
    bool supportsReactiveMask = false;
    bool supportsTransparencyMask = false;
    bool supportsAlphaComposition = false;
    bool supportsJitter = false;
};

struct FrameGenerationModeDesc
{
    std::string_view name{};
    FrameGenerationQualityMode quality = FrameGenerationQualityMode::Off;
    bool needsDepth = false;
    bool needsMotionVectors = true;
    bool needsHistory = true;
};
