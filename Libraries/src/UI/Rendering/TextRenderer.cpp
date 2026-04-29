#include "UI/Rendering/TextRenderer.h"

#include "Graphics/Core/GraphicsRuntime.h"
#include "Logger.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <msdfgen/msdfgen.h>
#include <utility>

#include FT_OUTLINE_H

namespace UI
{

namespace
{

constexpr const char *VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec4 vertex;
out vec2 TexCoords;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

constexpr const char *FRAGMENT_SHADER = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;
uniform vec3 textColor;
uniform float msdfPixelRange;
uniform float msdfWeight;

float median(vec3 value) {
    return max(min(value.r, value.g), min(max(value.r, value.g), value.b));
}

void main() {
    vec4 sampleValue = texture(text, TexCoords);
    float signedDistance = median(sampleValue.rgb) - 0.5;
    vec2 unitRange = vec2(msdfPixelRange) / vec2(textureSize(text, 0));
    vec2 dx = dFdx(TexCoords);
    vec2 dy = dFdy(TexCoords);
    vec2 screenTexSize = inversesqrt(max(vec2(dot(dx, dx), dot(dy, dy)), vec2(0.0000001)));
    float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);
    float alpha = clamp((signedDistance + msdfWeight) * screenPxRange + 0.5, 0.0, 1.0);
    color = vec4(textColor, alpha);
}
)";

constexpr std::size_t GlyphQuadVertexCount = 6;
constexpr std::size_t GlyphQuadFloatCount = GlyphQuadVertexCount * 4;
constexpr unsigned int MinimumDynamicFontSize = 8;
constexpr unsigned int MaximumDynamicFontSize = 256;
constexpr float LargeTextOversampleFactor = 1.25f;
constexpr float SmallTextOversampleFactor = 2.0f;
constexpr float MinimumSmallTextRasterSize = 24.0f;
constexpr float SmallTextPixelSnapThreshold = 12.0f;
constexpr float SmallTextWeightPixelThreshold = 20.0f;
constexpr float SmallTextWeight = 0.015f;
constexpr float DefaultLineSpacingFactor = 1.2f;
constexpr float EmptyLineVisibleHeightFactor = 0.8f;
constexpr float MsdfPixelRange = 8.0f;
constexpr int MsdfPaddingPixels = 12;
constexpr double MsdfAngleThreshold = 3.0;
constexpr unsigned long long MsdfColoringSeed = 0;

struct TextLineMetrics
{
    float width = 0.0f;
};

float GetVisibleAscenderPixels(const Character &character)
{
    if (character.size.y <= 0)
    {
        return 0.0f;
    }

    return std::max(0.0f, static_cast<float>(character.bearing.y - MsdfPaddingPixels));
}

float GetVisibleDescenderPixels(const Character &character)
{
    if (character.size.y <= 0)
    {
        return 0.0f;
    }

    return std::max(0.0f, static_cast<float>(character.size.y - character.bearing.y - MsdfPaddingPixels));
}

std::size_t FindLineEnd(const std::string &text, std::size_t lineStart)
{
    const std::size_t newline = text.find('\n', lineStart);
    return newline == std::string::npos ? text.size() : newline;
}

float GetKerningPixels(FT_Face face, unsigned int previousGlyphIndex, unsigned int glyphIndex)
{
    if (face == nullptr || previousGlyphIndex == 0 || glyphIndex == 0 || !FT_HAS_KERNING(face))
    {
        return 0.0f;
    }

    FT_Vector kerning{};
    if (FT_Get_Kerning(face, previousGlyphIndex, glyphIndex, FT_KERNING_DEFAULT, &kerning) != 0)
    {
        return 0.0f;
    }

    return static_cast<float>(kerning.x) / 64.0f;
}

TextLineMetrics MeasureLineMetrics(FT_Face face, const std::unordered_map<char, Character> &characters, const std::string &text,
                                   std::size_t lineStart, std::size_t lineEnd)
{
    TextLineMetrics metrics;
    const std::size_t clampedEnd = std::min(lineEnd, text.size());
    unsigned int previousGlyphIndex = 0;

    for (std::size_t index = lineStart; index < clampedEnd; ++index)
    {
        const auto characterIt = characters.find(text[index]);
        if (characterIt == characters.end())
        {
            continue;
        }

        const Character &character = characterIt->second;
        metrics.width += GetKerningPixels(face, previousGlyphIndex, character.glyphIndex);
        metrics.width += static_cast<float>(character.advance >> 6U);
        previousGlyphIndex = character.glyphIndex;
    }

    return metrics;
}

struct MsdfShapeBuildContext
{
    msdfgen::Shape shape;
    msdfgen::Contour *currentContour = nullptr;
    msdfgen::Point2 startPoint = {};
    msdfgen::Point2 currentPoint = {};
    bool hasCurrentContour = false;
};

msdfgen::Point2 FtPointToMsdfPoint(const FT_Vector &point)
{
    return {static_cast<double>(point.x) / 64.0, static_cast<double>(point.y) / 64.0};
}

bool AreClose(const msdfgen::Point2 &a, const msdfgen::Point2 &b)
{
    constexpr double Epsilon = 0.001;
    return std::abs(a.x - b.x) < Epsilon && std::abs(a.y - b.y) < Epsilon;
}

void CloseCurrentContour(MsdfShapeBuildContext &context)
{
    if (context.currentContour == nullptr)
    {
        return;
    }

    if (context.hasCurrentContour && !AreClose(context.currentPoint, context.startPoint))
    {
        context.currentContour->addEdge(msdfgen::EdgeHolder(context.currentPoint, context.startPoint));
    }

    if (context.currentContour->edges.empty() && !context.shape.contours.empty())
    {
        context.shape.contours.pop_back();
    }

    context.currentContour = nullptr;
    context.hasCurrentContour = false;
}

int ShapeMoveToCallback(const FT_Vector *to, void *user)
{
    auto &context = *static_cast<MsdfShapeBuildContext *>(user);
    CloseCurrentContour(context);

    context.currentContour = &context.shape.addContour();
    context.startPoint = FtPointToMsdfPoint(*to);
    context.currentPoint = context.startPoint;
    context.hasCurrentContour = true;
    return 0;
}

int ShapeLineToCallback(const FT_Vector *to, void *user)
{
    auto &context = *static_cast<MsdfShapeBuildContext *>(user);
    if (context.currentContour == nullptr)
    {
        return 0;
    }

    const msdfgen::Point2 end = FtPointToMsdfPoint(*to);
    if (AreClose(context.currentPoint, end))
    {
        return 0;
    }

    context.currentContour->addEdge(msdfgen::EdgeHolder(context.currentPoint, end));
    context.currentPoint = end;
    return 0;
}

int ShapeConicToCallback(const FT_Vector *control, const FT_Vector *to, void *user)
{
    auto &context = *static_cast<MsdfShapeBuildContext *>(user);
    if (context.currentContour == nullptr)
    {
        return 0;
    }

    const msdfgen::Point2 controlPoint = FtPointToMsdfPoint(*control);
    const msdfgen::Point2 end = FtPointToMsdfPoint(*to);
    if (AreClose(context.currentPoint, end) && AreClose(context.currentPoint, controlPoint))
    {
        return 0;
    }

    context.currentContour->addEdge(msdfgen::EdgeHolder(context.currentPoint, controlPoint, end));
    context.currentPoint = end;
    return 0;
}

int ShapeCubicToCallback(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user)
{
    auto &context = *static_cast<MsdfShapeBuildContext *>(user);
    if (context.currentContour == nullptr)
    {
        return 0;
    }

    const msdfgen::Point2 firstControlPoint = FtPointToMsdfPoint(*control1);
    const msdfgen::Point2 secondControlPoint = FtPointToMsdfPoint(*control2);
    const msdfgen::Point2 end = FtPointToMsdfPoint(*to);
    if (AreClose(context.currentPoint, end) && AreClose(context.currentPoint, firstControlPoint) &&
        AreClose(context.currentPoint, secondControlPoint))
    {
        return 0;
    }

    context.currentContour->addEdge(msdfgen::EdgeHolder(context.currentPoint, firstControlPoint, secondControlPoint, end));
    context.currentPoint = end;
    return 0;
}

msdfgen::Shape BuildShapeFromOutline(const FT_Outline &outline)
{
    MsdfShapeBuildContext context{};
    context.shape.setYAxisOrientation(msdfgen::Y_UPWARD);
    FT_Outline outlineCopy = outline;
    FT_Outline_Funcs funcs{};
    funcs.move_to = ShapeMoveToCallback;
    funcs.line_to = ShapeLineToCallback;
    funcs.conic_to = ShapeConicToCallback;
    funcs.cubic_to = ShapeCubicToCallback;
    funcs.shift = 0;
    funcs.delta = 0;

    if (FT_Outline_Decompose(&outlineCopy, &funcs, &context) != 0)
    {
        return {};
    }

    CloseCurrentContour(context);
    context.shape.normalize();
    return context.shape;
}

std::vector<std::uint8_t> GenerateMtsdfBitmap(msdfgen::Shape shape, int width, int height, double minX, double minY)
{
    std::vector<std::uint8_t> bitmap(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0);
    if (width <= 0 || height <= 0 || shape.contours.empty() || shape.edgeCount() <= 0 || !shape.validate())
    {
        return bitmap;
    }

    msdfgen::edgeColoringInkTrap(shape, MsdfAngleThreshold, MsdfColoringSeed);
    msdfgen::Bitmap<float, 4> mtsdf(width, height);
    const msdfgen::Projection projection({1.0, 1.0}, {-minX, -minY});
    const msdfgen::Range range(-static_cast<double>(MsdfPixelRange), static_cast<double>(MsdfPixelRange));
    const msdfgen::ErrorCorrectionConfig errorCorrection(msdfgen::ErrorCorrectionConfig::EDGE_PRIORITY,
                                                         msdfgen::ErrorCorrectionConfig::ALWAYS_CHECK_DISTANCE);
    const msdfgen::MSDFGeneratorConfig generatorConfig(true, errorCorrection);
    msdfgen::generateMTSDF(mtsdf, shape, projection, range, generatorConfig);
    msdfgen::distanceSignCorrection(mtsdf, shape, projection, 0.5f, msdfgen::FILL_NONZERO);
    msdfgen::msdfErrorCorrection(mtsdf, shape, projection, range, generatorConfig);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const float *pixel = mtsdf(x, height - 1 - y);
            const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
            for (int channel = 0; channel < 4; ++channel)
            {
                const float value = std::clamp(pixel[channel], 0.0f, 1.0f);
                bitmap[index + static_cast<std::size_t>(channel)] = static_cast<std::uint8_t>(std::round(value * 255.0f));
            }
        }
    }

    return bitmap;
}

} // namespace

TextRenderer::TextRenderer() : ft(nullptr), screenWidth(800), screenHeight(600) {}

TextRenderer::~TextRenderer()
{
    for (auto &[name, fontData] : fonts)
    {
        static_cast<void>(name);
        fontData.characters.clear();
        FT_Done_Face(fontData.face);
    }

    if (ft != nullptr)
    {
        FT_Done_FreeType(ft);
    }

    glyphGeometry.reset();
    shaderProgram.Destroy();
}

bool TextRenderer::init(unsigned int width, unsigned int height)
{
    screenWidth = width;
    screenHeight = height;

    if (FT_Init_FreeType(&ft) != 0)
    {
        LOG_ERROR(1, "ERROR::FREETYPE: Could not init FreeType Library");
        return false;
    }

    shaderProgram.SetShaderCode(VERTEX_SHADER, FRAGMENT_SHADER);
    // Match the rest of the UI: origin is top-left, Y grows downward.
    projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
    setupRenderData();

    return shaderProgram.IsCompiled() && glyphGeometry != nullptr;
}

void TextRenderer::setupRenderData()
{
    glyphGeometry.reset();

    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr)
    {
        GeometryCreateInfo createInfo{};
        createInfo.layout.vertexAttributes = {4};
        createInfo.vertexData.assign(GlyphQuadFloatCount, 0.0f);
        createInfo.indexData = {0, 1, 2, 3, 4, 5};
        createInfo.dynamicVertexData = true;
        createInfo.debugName = "ui_text_glyph_quad";
        glyphGeometry = device->CreateGeometry(createInfo);
    }
}

void TextRenderer::computeFontMetrics(FontData &fontData)
{
    float ascender = 0.0f;
    float descender = 0.0f;

    for (const auto &[glyph, character] : fontData.characters)
    {
        static_cast<void>(glyph);
        if (character.size.y <= 0)
        {
            continue;
        }

        ascender = std::max(ascender, GetVisibleAscenderPixels(character));
        descender = std::max(descender, GetVisibleDescenderPixels(character));
    }

    const float fallbackLineHeight = static_cast<float>(fontData.fontSize) * EmptyLineVisibleHeightFactor;
    fontData.visibleAscender = ascender > 0.0f ? ascender : fallbackLineHeight;
    fontData.visibleDescender = descender;
    fontData.visibleLineHeight = std::max(fontData.visibleAscender + fontData.visibleDescender, 1.0f);
}

bool TextRenderer::loadFont(const std::string &fontPath, const std::string &fontName, unsigned int fontSize)
{
    if (ft == nullptr)
    {
        return false;
    }

    fontRegistrations[fontName] = FontRegistration{fontPath, fontSize};
    if (activeFontName.empty())
    {
        activeFontName = fontName;
    }

    return ensureFontLoaded(fontName, fontSize);
}

bool TextRenderer::ensureFontLoaded(const std::string &fontName, unsigned int fontSize)
{
    if (ft == nullptr)
    {
        return false;
    }

    const auto registrationIt = fontRegistrations.find(fontName);
    if (registrationIt == fontRegistrations.end())
    {
        return false;
    }

    fontSize = std::clamp(fontSize, MinimumDynamicFontSize, MaximumDynamicFontSize);
    const std::string cacheKey = makeFontCacheKey(fontName, fontSize);
    if (fonts.find(cacheKey) != fonts.end())
    {
        return true;
    }

    FT_Face face = nullptr;
    if (FT_New_Face(ft, registrationIt->second.path.c_str(), 0, &face) != 0)
    {
        LOG_ERROR(1, "ERROR::FREETYPE: Failed to load font: ", registrationIt->second.path);
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    FontData fontData;
    fontData.face = face;
    fontData.fontSize = fontSize;

    for (unsigned char c = 0; c < 128; ++c)
    {
        fontData.characters.emplace(static_cast<char>(c), loadCharacter(face, static_cast<char>(c)));
    }
    computeFontMetrics(fontData);

    fonts[cacheKey] = std::move(fontData);
    return true;
}

Character TextRenderer::loadCharacter(FT_Face face, char c)
{
    if (FT_Load_Char(face, c, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) != 0)
    {
        LOG_ERROR(1, "ERROR::FREETYPE: Failed to load glyph '", c, "'");
        return {};
    }

    Character character;
    character.advance = static_cast<unsigned int>(face->glyph->advance.x);
    character.glyphIndex = static_cast<unsigned int>(FT_Get_Char_Index(face, static_cast<FT_ULong>(static_cast<unsigned char>(c))));

    if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE || face->glyph->outline.n_contours <= 0)
    {
        static constexpr std::uint8_t EmptyMsdfPixel[4] = {0, 0, 0, 0};
        character.texture = Texture(const_cast<std::uint8_t *>(EmptyMsdfPixel), 1, 1, "text", 0, TextureFormat::RGBA8,
                                    TexturePixelType::UnsignedByte, TextureFilterMode::LinearNoMipmaps);
        character.size = {0, 0};
        character.bearing = {0, 0};
        return character;
    }

    msdfgen::Shape shape = BuildShapeFromOutline(face->glyph->outline);
    if (shape.contours.empty() || shape.edgeCount() <= 0 || !shape.validate())
    {
        static constexpr std::uint8_t EmptyMsdfPixel[4] = {0, 0, 0, 0};
        character.texture = Texture(const_cast<std::uint8_t *>(EmptyMsdfPixel), 1, 1, "text", 0, TextureFormat::RGBA8,
                                    TexturePixelType::UnsignedByte, TextureFilterMode::LinearNoMipmaps);
        character.size = {0, 0};
        character.bearing = {0, 0};
        return character;
    }

    FT_BBox boundingBox{};
    FT_Outline_Get_CBox(&face->glyph->outline, &boundingBox);
    const float minX = std::floor(static_cast<float>(boundingBox.xMin) / 64.0f) - static_cast<float>(MsdfPaddingPixels);
    const float minY = std::floor(static_cast<float>(boundingBox.yMin) / 64.0f) - static_cast<float>(MsdfPaddingPixels);
    const float maxX = std::ceil(static_cast<float>(boundingBox.xMax) / 64.0f) + static_cast<float>(MsdfPaddingPixels);
    const float maxY = std::ceil(static_cast<float>(boundingBox.yMax) / 64.0f) + static_cast<float>(MsdfPaddingPixels);
    const int width = std::max(1, static_cast<int>(std::ceil(maxX - minX)));
    const int height = std::max(1, static_cast<int>(std::ceil(maxY - minY)));

    std::vector<std::uint8_t> bitmap = GenerateMtsdfBitmap(std::move(shape), width, height, minX, minY);
    character.texture = Texture(bitmap.data(), width, height, "text", 0, TextureFormat::RGBA8, TexturePixelType::UnsignedByte,
                                TextureFilterMode::LinearNoMipmaps);
    character.size = {width, height};
    character.bearing = {static_cast<int>(std::floor(static_cast<float>(boundingBox.xMin) / 64.0f)) - MsdfPaddingPixels,
                         static_cast<int>(std::ceil(static_cast<float>(boundingBox.yMax) / 64.0f)) + MsdfPaddingPixels};
    return character;
}

void TextRenderer::setActiveFont(const std::string &fontName)
{
    if (fontRegistrations.find(fontName) != fontRegistrations.end())
    {
        activeFontName = fontName;
    }
}

std::string TextRenderer::makeFontCacheKey(const std::string &fontName, unsigned int fontSize) const
{
    return fontName + "#" + std::to_string(fontSize);
}

TextRenderer::ResolvedFont TextRenderer::resolveFont(float scale)
{
    ResolvedFont resolved{};
    if (activeFontName.empty())
    {
        return resolved;
    }

    const auto registrationIt = fontRegistrations.find(activeFontName);
    if (registrationIt == fontRegistrations.end())
    {
        return resolved;
    }

    const float normalizedScale = std::max(scale, 0.01f);
    const float basePixelSize = static_cast<float>(registrationIt->second.baseSize);
    const float desiredPixelSize = static_cast<float>(registrationIt->second.baseSize) * normalizedScale;
    float rasterPixelSize = desiredPixelSize;
    if (desiredPixelSize <= basePixelSize)
    {
        // MSDF does not need a 3x+ downscale for small UI text; too much filtering
        // softens glyph edges before the distance field shader can reconstruct them.
        rasterPixelSize = std::clamp(desiredPixelSize * SmallTextOversampleFactor, MinimumSmallTextRasterSize, basePixelSize);
    }
    else
    {
        rasterPixelSize *= LargeTextOversampleFactor;
    }

    unsigned int requestedFontSize = static_cast<unsigned int>(std::round(rasterPixelSize));
    requestedFontSize = std::clamp(requestedFontSize, MinimumDynamicFontSize, MaximumDynamicFontSize);

    if (!ensureFontLoaded(activeFontName, requestedFontSize))
    {
        requestedFontSize = std::clamp(registrationIt->second.baseSize, MinimumDynamicFontSize, MaximumDynamicFontSize);
        if (!ensureFontLoaded(activeFontName, requestedFontSize))
        {
            return resolved;
        }
    }

    const auto fontIt = fonts.find(makeFontCacheKey(activeFontName, requestedFontSize));
    if (fontIt == fonts.end())
    {
        return resolved;
    }

    resolved.data = &fontIt->second;
    resolved.scale = desiredPixelSize / static_cast<float>(fontIt->second.fontSize);
    return resolved;
}

void TextRenderer::updateScreenSize(unsigned int width, unsigned int height)
{
    screenWidth = width;
    screenHeight = height;
    // Match the rest of the UI: origin is top-left, Y grows downward.
    projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
}

glm::vec2 TextRenderer::calculateAnchorOffset(const std::string &text, float scale, TextAnchor anchor)
{
    glm::vec2 textSize = measureText(text, scale);
    glm::vec2 offset = {0.0f, 0.0f};

    switch (anchor)
    {
        case TextAnchor::TopCenter:
        case TextAnchor::Center:
        case TextAnchor::BottomCenter:
            offset.x = -textSize.x / 2.0f;
            break;
        case TextAnchor::TopRight:
        case TextAnchor::CenterRight:
        case TextAnchor::BottomRight:
            offset.x = -textSize.x;
            break;
        default:
            break;
    }

    switch (anchor)
    {
        case TextAnchor::CenterLeft:
        case TextAnchor::Center:
        case TextAnchor::CenterRight:
            offset.y = -textSize.y / 2.0f;
            break;
        case TextAnchor::BottomLeft:
        case TextAnchor::BottomCenter:
        case TextAnchor::BottomRight:
            offset.y = -textSize.y;
            break;
        default:
            break;
    }

    return offset;
}

void TextRenderer::renderText(const std::string &text, float x, float y, float scale, const glm::vec3 &color, TextAnchor anchor)
{
    if (fontRegistrations.empty() || activeFontName.empty() || glyphGeometry == nullptr || !shaderProgram.IsCompiled())
    {
        return;
    }

    ResolvedFont resolvedFont = resolveFont(scale);
    if (resolvedFont.data == nullptr)
    {
        return;
    }

    auto &fontData = *resolvedFont.data;
    const float renderScale = resolvedFont.scale;
    const float lineHeight = static_cast<float>(fontData.fontSize) * renderScale;
    const float lineAscender = fontData.visibleAscender * renderScale;
    const float lineAdvance = fontData.visibleLineHeight * renderScale * DefaultLineSpacingFactor;
    const glm::vec2 anchorOffset = calculateAnchorOffset(text, scale, anchor);
    const auto registrationIt = fontRegistrations.find(activeFontName);
    const float desiredPixelSize =
        registrationIt != fontRegistrations.end() ? static_cast<float>(registrationIt->second.baseSize) * std::max(scale, 0.01f) : lineHeight;
    const bool snapToPixels = desiredPixelSize >= SmallTextPixelSnapThreshold;
    const auto maybeRound = [snapToPixels](float value) { return snapToPixels ? std::round(value) : value; };

    const float startX = x + anchorOffset.x;
    const float startY = y + anchorOffset.y;

    shaderProgram.Bind();
    shaderProgram.SetUniformMatrix4(shaderProgram.GetUniformLocation("projection"), &projection[0][0]);
    shaderProgram.SetUniformFloats(shaderProgram.GetUniformLocation("textColor"), &color[0], 3);
    shaderProgram.SetUniformFloats(shaderProgram.GetUniformLocation("msdfPixelRange"), &MsdfPixelRange, 1);
    const float msdfWeight = desiredPixelSize < SmallTextWeightPixelThreshold ? SmallTextWeight : 0.0f;
    shaderProgram.SetUniformFloats(shaderProgram.GetUniformLocation("msdfWeight"), &msdfWeight, 1);
    const int textureSlot = 0;
    shaderProgram.SetUniformInts(shaderProgram.GetUniformLocation("text"), &textureSlot, 1);

    glyphGeometry->Bind();
    float cursorX = maybeRound(startX);
    float cursorY = maybeRound(startY);
    unsigned int previousGlyphIndex = 0;

    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const char c = text[index];
        if (c == '\n')
        {
            cursorY = maybeRound(cursorY + lineAdvance);
            cursorX = maybeRound(startX);
            previousGlyphIndex = 0;
            continue;
        }

        const auto characterIt = fontData.characters.find(c);
        if (characterIt == fontData.characters.end())
        {
            continue;
        }

        Character &ch = characterIt->second;
        cursorX += GetKerningPixels(fontData.face, previousGlyphIndex, ch.glyphIndex) * renderScale;

        const float xpos = maybeRound(cursorX + static_cast<float>(ch.bearing.x) * renderScale);
        const float baselineY = maybeRound(cursorY + lineAscender);
        const float glyphTop = maybeRound(baselineY - static_cast<float>(ch.bearing.y) * renderScale);
        const float w =
            std::max(1.0f, snapToPixels ? std::round(static_cast<float>(ch.size.x) * renderScale) : static_cast<float>(ch.size.x) * renderScale);
        const float h =
            std::max(1.0f, snapToPixels ? std::round(static_cast<float>(ch.size.y) * renderScale) : static_cast<float>(ch.size.y) * renderScale);
        const float top = glyphTop;
        const float bottom = top + h;

        const std::array<float, GlyphQuadFloatCount> vertices = {
            xpos, top, 0.0f, 0.0f, xpos, bottom, 0.0f, 1.0f, xpos + w, bottom, 1.0f, 1.0f,
            xpos, top, 0.0f, 0.0f, xpos + w, bottom, 1.0f, 1.0f, xpos + w, top, 1.0f, 0.0f,
        };

        glyphGeometry->UpdateVertexData(vertices.data(), vertices.size(), 0);
        ch.texture.Bind();
        glyphGeometry->DrawIndexed();

        cursorX += static_cast<float>(ch.advance >> 6U) * renderScale;
        previousGlyphIndex = ch.glyphIndex;
    }

    glyphGeometry->Unbind();
    shaderProgram.Unbind();
}

float TextRenderer::measureTextWidth(const std::string &text, float scale)
{
    if (fontRegistrations.empty() || activeFontName.empty())
    {
        return 0.0f;
    }

    ResolvedFont resolvedFont = resolveFont(scale);
    if (resolvedFont.data == nullptr)
    {
        return 0.0f;
    }

    auto &fontData = *resolvedFont.data;
    const float renderScale = resolvedFont.scale;
    float width = 0.0f;
    unsigned int previousGlyphIndex = 0;

    for (char c : text)
    {
        if (c == '\n')
        {
            break;
        }

        const auto characterIt = fontData.characters.find(c);
        if (characterIt == fontData.characters.end())
        {
            continue;
        }

        const Character &character = characterIt->second;
        width += GetKerningPixels(fontData.face, previousGlyphIndex, character.glyphIndex) * renderScale;
        width += static_cast<float>(character.advance >> 6U) * renderScale;
        previousGlyphIndex = character.glyphIndex;
    }

    return std::round(width);
}

glm::vec2 TextRenderer::measureText(const std::string &text, float scale)
{
    if (fontRegistrations.empty() || activeFontName.empty())
    {
        return {0, 0};
    }

    ResolvedFont resolvedFont = resolveFont(scale);
    if (resolvedFont.data == nullptr)
    {
        return {0, 0};
    }

    auto &fontData = *resolvedFont.data;
    const float renderScale = resolvedFont.scale;
    float maxWidth = 0.0f;
    int lineCount = 0;

    for (std::size_t lineStart = 0; lineStart <= text.size();)
    {
        const std::size_t lineEnd = FindLineEnd(text, lineStart);
        const TextLineMetrics lineMetrics = MeasureLineMetrics(fontData.face, fontData.characters, text, lineStart, lineEnd);
        const float lineWidth = lineMetrics.width * renderScale;
        maxWidth = std::max(maxWidth, lineWidth);
        ++lineCount;

        if (lineEnd == text.size())
        {
            break;
        }

        lineStart = lineEnd + 1U;
    }

    const float lineHeight = fontData.visibleLineHeight * renderScale;
    const float totalHeight =
        lineCount > 0 ? lineHeight + static_cast<float>(lineCount - 1) * lineHeight * DefaultLineSpacingFactor : 0.0f;
    return {std::round(maxWidth), std::round(totalHeight)};
}

std::vector<std::string> TextRenderer::wrapText(const std::string &text, float scale, float maxWidth)
{
    std::vector<std::string> lines;
    if (text.empty())
    {
        return lines;
    }

    ResolvedFont resolvedFont = resolveFont(scale);
    if (resolvedFont.data == nullptr)
    {
        return lines;
    }

    auto &fontData = *resolvedFont.data;
    const float renderScale = resolvedFont.scale;
    std::size_t start = 0;
    std::size_t newlinePos = 0;

    while ((newlinePos = text.find('\n', start)) != std::string::npos)
    {
        std::string segment = text.substr(start, newlinePos - start);

        while (!segment.empty())
        {
            float width = 0.0f;
            std::size_t i = 0;
            std::size_t lastSpace = 0;

            for (; i < segment.length(); ++i)
            {
                const char c = segment[i];
                if (c == ' ')
                {
                    lastSpace = i;
                }

                const auto characterIt = fontData.characters.find(c);
                if (characterIt != fontData.characters.end())
                {
                    const float charWidth = static_cast<float>(characterIt->second.advance >> 6U) * renderScale;
                    if (width + charWidth > maxWidth && i > 0)
                    {
                        break;
                    }
                    width += charWidth;
                }
            }

            if (i == 0)
            {
                i = 1;
            }

            if (i < segment.length() && lastSpace > 0)
            {
                i = lastSpace;
            }

            lines.push_back(segment.substr(0, i));
            segment = segment.substr(i);

            if (!segment.empty() && segment[0] == ' ')
            {
                segment = segment.substr(1);
            }
        }

        start = newlinePos + 1;
    }

    std::string remaining = text.substr(start);
    while (!remaining.empty())
    {
        float width = 0.0f;
        std::size_t i = 0;
        std::size_t lastSpace = 0;

        for (; i < remaining.length(); ++i)
        {
            const char c = remaining[i];
            if (c == ' ')
            {
                lastSpace = i;
            }

            const auto characterIt = fontData.characters.find(c);
            if (characterIt != fontData.characters.end())
            {
                const float charWidth = static_cast<float>(characterIt->second.advance >> 6U) * renderScale;
                if (width + charWidth > maxWidth && i > 0)
                {
                    break;
                }
                width += charWidth;
            }
        }

        if (i == 0)
        {
            i = 1;
        }

        if (i < remaining.length() && lastSpace > 0)
        {
            i = lastSpace;
        }

        lines.push_back(remaining.substr(0, i));
        remaining = remaining.substr(i);

        if (!remaining.empty() && remaining[0] == ' ')
        {
            remaining = remaining.substr(1);
        }
    }

    return lines;
}

float TextRenderer::measureLineWidth(const std::string &line, float scale) { return measureTextWidth(line, scale); }

void TextRenderer::applyHorizontalAlignment(TextLayout &layout, const TextLayoutParams &params)
{
    for (auto &line : layout.lines)
    {
        switch (params.horizontalAlign)
        {
            case TextAlign::Center:
                line.x = (params.maxWidth - line.width) / 2.0f;
                break;
            case TextAlign::Right:
                line.x = params.maxWidth - line.width - params.padding.x;
                break;
            default:
                line.x = params.padding.x;
                break;
        }
    }
}

void TextRenderer::applyVerticalAlignment(TextLayout &layout, const TextLayoutParams &params)
{
    if (params.maxHeight <= 0.0f)
    {
        return;
    }

    float offsetY = 0.0f;
    switch (params.verticalAlign)
    {
        case VerticalAlign::Middle:
            offsetY = (params.maxHeight - layout.totalHeight) / 2.0f;
            break;
        case VerticalAlign::Bottom:
            offsetY = params.maxHeight - layout.totalHeight - params.padding.y;
            break;
        default:
            offsetY = params.padding.y;
            break;
    }

    for (auto &line : layout.lines)
    {
        line.y += offsetY;
    }
}

void TextRenderer::handleOverflow(TextLayout &layout, const TextLayoutParams &params)
{
    if (params.overflow == TextOverflow::Visible)
    {
        return;
    }

    if (params.maxHeight > 0.0f)
    {
        auto it = layout.lines.begin();
        while (it != layout.lines.end())
        {
            if (it->y + it->height > params.maxHeight)
            {
                layout.hasOverflow = true;

                if (params.overflow == TextOverflow::Hidden)
                {
                    layout.lines.erase(it, layout.lines.end());
                    break;
                }

                if (params.overflow == TextOverflow::Ellipsis && it != layout.lines.begin())
                {
                    auto prev = std::prev(it);
                    prev->text += "...";
                    layout.lines.erase(it, layout.lines.end());
                    break;
                }
            }
            ++it;
        }
    }

    if (layout.hasOverflow && params.overflow == TextOverflow::Scroll)
    {
        layout.maxScroll.y = std::max(0.0f, layout.totalHeight - params.maxHeight);
    }
}

TextLayout TextRenderer::calculateLayout(const std::string &text, float scale, const TextLayoutParams &params)
{
    TextLayout layout;
    if (fontRegistrations.empty() || activeFontName.empty() || text.empty())
    {
        return layout;
    }

    ResolvedFont resolvedFont = resolveFont(scale);
    if (resolvedFont.data == nullptr)
    {
        return layout;
    }

    auto &fontData = *resolvedFont.data;
    const float lineHeight = static_cast<float>(fontData.fontSize) * resolvedFont.scale * params.lineSpacing;

    std::vector<std::string> textLines;
    if (params.wordWrap && params.maxWidth > 0.0f)
    {
        textLines = wrapText(text, scale, params.maxWidth - params.padding.x * 2.0f);
    }
    else
    {
        std::size_t start = 0;
        std::size_t end = 0;
        while ((end = text.find('\n', start)) != std::string::npos)
        {
            textLines.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        textLines.push_back(text.substr(start));
    }

    float y = params.padding.y;
    for (const std::string &lineText : textLines)
    {
        TextLine line;
        line.text = lineText;
        line.width = measureLineWidth(lineText, scale);
        line.height = lineHeight;
        line.x = params.padding.x;
        line.y = y;

        layout.lines.push_back(line);
        layout.totalWidth = std::max(layout.totalWidth, line.width);
        y += lineHeight;
    }

    layout.totalHeight = y;
    if (params.maxWidth > 0.0f && layout.totalWidth > params.maxWidth)
    {
        layout.hasOverflow = true;
    }
    if (params.maxHeight > 0.0f && layout.totalHeight > params.maxHeight)
    {
        layout.hasOverflow = true;
    }

    applyHorizontalAlignment(layout, params);
    applyVerticalAlignment(layout, params);
    handleOverflow(layout, params);
    return layout;
}

void TextRenderer::renderTextAdvanced(const std::string &text, float x, float y, const TextLayoutParams &params, const glm::vec3 &color,
                                      float scale)
{
    TextLayout layout = calculateLayout(text, scale, params);

    for (const auto &line : layout.lines)
    {
        renderText(line.text, x + line.x - layout.scrollOffset.x, y + line.y - layout.scrollOffset.y, scale, color, TextAnchor::TopLeft);
    }
}

glm::vec2 TextRenderer::calculateMinSize(const std::string &text, float scale, bool wordWrap)
{
    if (wordWrap)
    {
        float maxWordWidth = 0.0f;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= text.length(); ++i)
        {
            if (i == text.length() || text[i] == ' ' || text[i] == '\n')
            {
                const std::string word = text.substr(start, i - start);
                maxWordWidth = std::max(maxWordWidth, measureTextWidth(word, scale));
                start = i + 1;
            }
        }

        ResolvedFont resolvedFont = resolveFont(scale);
        if (resolvedFont.data == nullptr)
        {
            return {maxWordWidth, 0.0f};
        }

        return {maxWordWidth, static_cast<float>(resolvedFont.data->fontSize) * resolvedFont.scale};
    }

    return measureText(text, scale);
}

glm::vec2 TextRenderer::getCenteredPosition(const std::string &text, float scale, float rectX, float rectY, float rectWidth,
                                            float rectHeight)
{
    const glm::vec2 textSize = measureText(text, scale);
    return {rectX + (rectWidth - textSize.x) / 2.0f, rectY + (rectHeight - textSize.y) / 2.0f};
}

float TextRenderer::findOptimalScale(const std::string &text, float maxWidth, float maxHeight, float minScale, float maxScale)
{
    float bestScale = minScale;
    for (float scale = minScale; scale <= maxScale; scale += 0.1f)
    {
        const glm::vec2 size = measureText(text, scale);
        if (size.x <= maxWidth && size.y <= maxHeight)
        {
            bestScale = scale;
        }
        else
        {
            break;
        }
    }

    return bestScale;
}

} // namespace UI
