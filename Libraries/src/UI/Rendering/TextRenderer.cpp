#include "UI/Rendering/TextRenderer.h"

#include "Graphics/Core/GraphicsRuntime.h"
#include "Logger.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <utility>

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

void main() {
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(textColor, 1.0) * sampled;
}
)";

constexpr std::size_t GlyphQuadVertexCount = 6;
constexpr std::size_t GlyphQuadFloatCount = GlyphQuadVertexCount * 4;
constexpr int GlyphAtlasWidth = 1024;
constexpr int GlyphAtlasPadding = 1;

struct GlyphBitmap
{
    char character = '\0';
    int width = 0;
    int height = 0;
    int bearingX = 0;
    int bearingY = 0;
    unsigned int advance = 0;
    int atlasX = 0;
    int atlasY = 0;
    std::vector<unsigned char> pixels;
};

void CopyGlyphBitmap(const FT_Bitmap &bitmap, std::vector<unsigned char> &pixels)
{
    const int width = static_cast<int>(bitmap.width);
    const int height = static_cast<int>(bitmap.rows);
    pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    if (bitmap.buffer == nullptr || width <= 0 || height <= 0)
    {
        return;
    }

    const int pitch = bitmap.pitch;
    for (int row = 0; row < height; ++row)
    {
        const unsigned char *source = pitch >= 0 ? bitmap.buffer + row * pitch : bitmap.buffer + (height - 1 - row) * (-pitch);
        std::memcpy(pixels.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(width), source,
                    static_cast<std::size_t>(width));
    }
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
    projection = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height));
    setupRenderData();
    projectionUniform = shaderProgram.GetUniformLocation("projection");
    textColorUniform = shaderProgram.GetUniformLocation("textColor");
    textSamplerUniform = shaderProgram.GetUniformLocation("text");

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
        createInfo.dynamicVertexData = true;
        createInfo.debugName = "ui_text_batch";
        glyphGeometry = device->CreateGeometry(createInfo);
    }
}

bool TextRenderer::loadFont(const std::string &fontPath, const std::string &fontName, unsigned int fontSize)
{
    if (ft == nullptr)
    {
        return false;
    }

    FT_Face face = nullptr;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face) != 0)
    {
        LOG_ERROR(1, "ERROR::FREETYPE: Failed to load font: ", fontPath);
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    FontData fontData;
    fontData.face = face;
    fontData.fontSize = fontSize;

    std::vector<GlyphBitmap> glyphs;
    glyphs.reserve(128);
    for (unsigned char c = 0; c < 128; ++c)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER) != 0)
        {
            LOG_ERROR(1, "ERROR::FREETYPE: Failed to load glyph '", static_cast<char>(c), "'");
            continue;
        }

        GlyphBitmap glyph;
        glyph.character = static_cast<char>(c);
        glyph.width = static_cast<int>(face->glyph->bitmap.width);
        glyph.height = static_cast<int>(face->glyph->bitmap.rows);
        glyph.bearingX = face->glyph->bitmap_left;
        glyph.bearingY = face->glyph->bitmap_top;
        glyph.advance = static_cast<unsigned int>(face->glyph->advance.x);
        CopyGlyphBitmap(face->glyph->bitmap, glyph.pixels);
        glyphs.push_back(std::move(glyph));
    }

    int cursorX = GlyphAtlasPadding;
    int cursorY = GlyphAtlasPadding;
    int rowHeight = 0;
    for (GlyphBitmap &glyph : glyphs)
    {
        if (glyph.width > 0 && cursorX + glyph.width + GlyphAtlasPadding > GlyphAtlasWidth)
        {
            cursorX = GlyphAtlasPadding;
            cursorY += rowHeight + GlyphAtlasPadding;
            rowHeight = 0;
        }

        glyph.atlasX = cursorX;
        glyph.atlasY = cursorY;
        cursorX += glyph.width + GlyphAtlasPadding;
        rowHeight = std::max(rowHeight, glyph.height);
    }

    fontData.atlasWidth = GlyphAtlasWidth;
    fontData.atlasHeight = std::max(1, cursorY + rowHeight + GlyphAtlasPadding);
    std::vector<unsigned char> atlasPixels(static_cast<std::size_t>(fontData.atlasWidth) * static_cast<std::size_t>(fontData.atlasHeight),
                                           0);

    for (const GlyphBitmap &glyph : glyphs)
    {
        Character character;
        character.size = {glyph.width, glyph.height};
        character.bearing = {glyph.bearingX, glyph.bearingY};
        character.advance = glyph.advance;
        if (glyph.width > 0 && glyph.height > 0)
        {
            character.uvMin = {static_cast<float>(glyph.atlasX) / static_cast<float>(fontData.atlasWidth),
                               static_cast<float>(glyph.atlasY) / static_cast<float>(fontData.atlasHeight)};
            character.uvMax = {static_cast<float>(glyph.atlasX + glyph.width) / static_cast<float>(fontData.atlasWidth),
                               static_cast<float>(glyph.atlasY + glyph.height) / static_cast<float>(fontData.atlasHeight)};

            for (int row = 0; row < glyph.height; ++row)
            {
                const std::size_t dstOffset = static_cast<std::size_t>(glyph.atlasY + row) * static_cast<std::size_t>(fontData.atlasWidth) +
                                              static_cast<std::size_t>(glyph.atlasX);
                const std::size_t srcOffset = static_cast<std::size_t>(row) * static_cast<std::size_t>(glyph.width);
                std::memcpy(atlasPixels.data() + dstOffset, glyph.pixels.data() + srcOffset, static_cast<std::size_t>(glyph.width));
            }
        }
        fontData.characters.emplace(glyph.character, std::move(character));
    }

    fontData.atlasTexture = Texture(atlasPixels.data(), fontData.atlasWidth, fontData.atlasHeight, "text", 0, TextureFormat::R8,
                                    TexturePixelType::UnsignedByte, TextureFilterMode::Linear);

    fonts[fontName] = std::move(fontData);
    if (activeFontName.empty())
    {
        activeFontName = fontName;
    }

    return true;
}

Character TextRenderer::loadCharacter(FT_Face face, char c)
{
    if (FT_Load_Char(face, c, FT_LOAD_RENDER) != 0)
    {
        LOG_ERROR(1, "ERROR::FREETYPE: Failed to load glyph '", c, "'");
        return {};
    }

    Character character;
    character.texture =
        Texture(face->glyph->bitmap.buffer, static_cast<int>(face->glyph->bitmap.width), static_cast<int>(face->glyph->bitmap.rows), "text",
                0, TextureFormat::R8, TexturePixelType::UnsignedByte, TextureFilterMode::Linear);
    character.size = {face->glyph->bitmap.width, face->glyph->bitmap.rows};
    character.bearing = {face->glyph->bitmap_left, face->glyph->bitmap_top};
    character.advance = static_cast<unsigned int>(face->glyph->advance.x);
    return character;
}

void TextRenderer::setActiveFont(const std::string &fontName)
{
    if (fonts.find(fontName) != fonts.end())
    {
        activeFontName = fontName;
    }
}

void TextRenderer::updateScreenSize(unsigned int width, unsigned int height)
{
    if (screenWidth == width && screenHeight == height)
    {
        return;
    }
    screenWidth = width;
    screenHeight = height;
    projection = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height));
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
    if (fonts.empty() || activeFontName.empty() || glyphGeometry == nullptr || !shaderProgram.IsCompiled())
    {
        return;
    }

    auto &fontData = fonts[activeFontName];
    const float lineHeight = static_cast<float>(fontData.fontSize) * scale;
    const glm::vec2 anchorOffset = calculateAnchorOffset(text, scale, anchor);

    const float startX = x + anchorOffset.x;
    const float startY = y + anchorOffset.y;

    batchedVertices.clear();
    batchedVertices.reserve(text.size() * GlyphQuadFloatCount);

    float cursorX = startX;
    float cursorY = startY;

    for (char c : text)
    {
        if (c == '\n')
        {
            cursorY += lineHeight * 1.2f;
            cursorX = startX;
            continue;
        }

        const auto characterIt = fontData.characters.find(c);
        if (characterIt == fontData.characters.end())
        {
            continue;
        }

        Character &ch = characterIt->second;
        const float advance = static_cast<float>(ch.advance >> 6U) * scale;
        if (ch.size.x <= 0 || ch.size.y <= 0)
        {
            cursorX += advance;
            continue;
        }

        const float xpos = cursorX + static_cast<float>(ch.bearing.x) * scale;
        const float baselineY = cursorY + static_cast<float>(fontData.fontSize) * scale;
        const float glyphTop = baselineY - static_cast<float>(ch.bearing.y) * scale;
        const float glyphBottom = glyphTop + static_cast<float>(ch.size.y) * scale;
        const float ypos = static_cast<float>(screenHeight) - glyphBottom;
        const float w = static_cast<float>(ch.size.x) * scale;
        const float h = static_cast<float>(ch.size.y) * scale;
        const float u0 = ch.uvMin.x;
        const float v0 = ch.uvMin.y;
        const float u1 = ch.uvMax.x;
        const float v1 = ch.uvMax.y;

        const std::array<float, GlyphQuadFloatCount> vertices = {
            xpos, ypos + h, u0, v0, xpos,     ypos, u0, v1, xpos + w, ypos,     u1, v1,
            xpos, ypos + h, u0, v0, xpos + w, ypos, u1, v1, xpos + w, ypos + h, u1, v0,
        };

        batchedVertices.insert(batchedVertices.end(), vertices.begin(), vertices.end());
        cursorX += advance;
    }

    if (batchedVertices.empty())
    {
        return;
    }

    shaderProgram.Bind();
    shaderProgram.SetUniformMatrix4(projectionUniform, &projection[0][0]);
    shaderProgram.SetUniformFloats(textColorUniform, &color[0], 3);
    const int textureSlot = 0;
    shaderProgram.SetUniformInts(textSamplerUniform, &textureSlot, 1);

    fontData.atlasTexture.Bind();
    glyphGeometry->Bind();
    glyphGeometry->UpdateVertexData(batchedVertices.data(), batchedVertices.size(), 0);
    glyphGeometry->DrawVertices(batchedVertices.size() / 4);
    glyphGeometry->Unbind();
    shaderProgram.Unbind();
}

float TextRenderer::measureTextWidth(const std::string &text, float scale)
{
    if (fonts.empty() || activeFontName.empty())
    {
        return 0.0f;
    }

    auto &fontData = fonts[activeFontName];
    float width = 0.0f;

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

        width += static_cast<float>(characterIt->second.advance >> 6U) * scale;
    }

    return width;
}

glm::vec2 TextRenderer::measureText(const std::string &text, float scale)
{
    if (fonts.empty() || activeFontName.empty())
    {
        return {0, 0};
    }

    auto &fontData = fonts[activeFontName];
    float maxWidth = 0.0f;
    float currentWidth = 0.0f;
    int lineCount = 1;

    for (char c : text)
    {
        if (c == '\n')
        {
            maxWidth = std::max(maxWidth, currentWidth);
            currentWidth = 0.0f;
            ++lineCount;
            continue;
        }

        const auto characterIt = fontData.characters.find(c);
        if (characterIt != fontData.characters.end())
        {
            currentWidth += static_cast<float>(characterIt->second.advance >> 6U) * scale;
        }
    }

    maxWidth = std::max(maxWidth, currentWidth);
    const float height = static_cast<float>(fontData.fontSize) * scale * static_cast<float>(lineCount) * 1.2f;
    return {maxWidth, height};
}

std::vector<std::string> TextRenderer::wrapText(const std::string &text, float scale, float maxWidth)
{
    std::vector<std::string> lines;
    if (text.empty())
    {
        return lines;
    }

    auto &fontData = fonts[activeFontName];
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
                    const float charWidth = static_cast<float>(characterIt->second.advance >> 6U) * scale;
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
                const float charWidth = static_cast<float>(characterIt->second.advance >> 6U) * scale;
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
    if (fonts.empty() || activeFontName.empty() || text.empty())
    {
        return layout;
    }

    auto &fontData = fonts[activeFontName];
    const float lineHeight = static_cast<float>(fontData.fontSize) * scale * params.lineSpacing;

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

        return {maxWordWidth, static_cast<float>(fonts[activeFontName].fontSize) * scale};
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
