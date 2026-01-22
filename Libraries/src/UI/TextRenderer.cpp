// Libraries/src/UI/TextRenderer.cpp
#include "UI/TextRenderer.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>

namespace UI {

// Shaders
const char* VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
out vec2 TexCoords;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

const char* FRAGMENT_SHADER = R"(
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

// ============================================
// Shader Implementation
// ============================================

Shader::Shader(const char* vertexSource, const char* fragmentSource) {
    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertexSource, nullptr);
    glCompileShader(vertex);

    int success;
    char infoLog[512];
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex, 512, nullptr, infoLog);
        std::cerr << "Vertex Shader Error: " << infoLog << std::endl;
    }

    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragmentSource, nullptr);
    glCompileShader(fragment);

    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment, 512, nullptr, infoLog);
        std::cerr << "Fragment Shader Error: " << infoLog << std::endl;
    }

    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);

    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(ID, 512, nullptr, infoLog);
        std::cerr << "Shader Program Error: " << infoLog << std::endl;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader() {
    glDeleteProgram(ID);
}

void Shader::use() {
    glUseProgram(ID);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setVec3(const std::string& name, const glm::vec3& vec) {
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &vec[0]);
}

void Shader::setInt(const std::string& name, int value) {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

// ============================================
// TextRenderer Implementation
// ============================================

TextRenderer::TextRenderer()
    : ft(nullptr), VAO(0), VBO(0), screenWidth(800), screenHeight(600) {}

TextRenderer::~TextRenderer() {
    for (auto& [name, fontData] : fonts) {
        for (auto& [c, character] : fontData.characters) {
            glDeleteTextures(1, &character.textureID);
        }
        FT_Done_Face(fontData.face);
    }

    if (ft) {
        FT_Done_FreeType(ft);
    }

    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
}

bool TextRenderer::init(unsigned int width, unsigned int height) {
    screenWidth = width;
    screenHeight = height;

    // Initialisation FreeType
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return false;
    }

    // Création du shader
    shader = std::make_unique<Shader>(VERTEX_SHADER, FRAGMENT_SHADER);

    // Projection orthographique
    projection = glm::ortho(0.0f, static_cast<float>(width),
                           0.0f, static_cast<float>(height));

    setupRenderData();

    return true;
}

void TextRenderer::setupRenderData() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

bool TextRenderer::loadFont(const std::string& fontPath, const std::string& fontName,
                           unsigned int fontSize) {
    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font: " << fontPath << std::endl;
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    FontData fontData;
    fontData.face = face;
    fontData.fontSize = fontSize;

    // Charger les 128 premiers caractères ASCII
    for (unsigned char c = 0; c < 128; c++) {
        fontData.characters[c] = loadCharacter(face, c);
    }

    fonts[fontName] = fontData;

    if (activeFontName.empty()) {
        activeFontName = fontName;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    return true;
}

Character TextRenderer::loadCharacter(FT_Face face, char c) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
        std::cerr << "ERROR::FREETYPE: Failed to load Glyph '" << c << "'" << std::endl;
        return Character();
    }

    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        face->glyph->bitmap.width,
        face->glyph->bitmap.rows,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        face->glyph->bitmap.buffer
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Character character = {
        texture,
        glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
        glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
        static_cast<unsigned int>(face->glyph->advance.x)
    };

    return character;
}

void TextRenderer::setActiveFont(const std::string& fontName) {
    if (fonts.find(fontName) != fonts.end()) {
        activeFontName = fontName;
    }
}

void TextRenderer::updateScreenSize(unsigned int width, unsigned int height) {
    screenWidth = width;
    screenHeight = height;
    projection = glm::ortho(0.0f, static_cast<float>(width),
                           0.0f, static_cast<float>(height));
}

// Système de coordonnées UI : (0,0) en haut à gauche, Y positif vers le bas
// TextAnchor définit quel point du texte est aligné sur (x, y)
glm::vec2 TextRenderer::calculateAnchorOffset(const std::string& text, float scale, TextAnchor anchor) {
    glm::vec2 textSize = measureText(text, scale);
    glm::vec2 offset = {0.0f, 0.0f};

    // Offset horizontal
    switch (anchor) {
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
        default: break; // Left anchors: offset.x = 0
    }

    // Offset vertical
    // TopLeft/TopCenter/TopRight: le HAUT du texte est à Y, donc offset = 0
    // Center: le CENTRE du texte est à Y, donc on remonte de height/2
    // Bottom: le BAS du texte est à Y, donc on remonte de height
    switch (anchor) {
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
        default: break; // Top anchors: offset.y = 0
    }

    return offset;
}

void TextRenderer::renderText(const std::string& text, float x, float y,
                             float scale, const glm::vec3& color, TextAnchor anchor) {
    if (fonts.empty() || activeFontName.empty()) return;

    auto& fontData = fonts[activeFontName];
    float lineHeight = fontData.fontSize * scale;

    // Calculer l'offset basé sur l'ancre
    glm::vec2 anchorOffset = calculateAnchorOffset(text, scale, anchor);

    // Position de départ en coordonnées UI (0,0 en haut à gauche, Y vers le bas)
    float startX = x + anchorOffset.x;
    float startY = y + anchorOffset.y;

    shader->use();
    shader->setMat4("projection", projection);
    shader->setVec3("textColor", color);
    shader->setInt("text", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float cursorX = startX;
    float cursorY = startY;

    for (char c : text) {
        if (c == '\n') {
            cursorY += lineHeight * 1.2f;
            cursorX = startX;
            continue;
        }

        if (fontData.characters.find(c) == fontData.characters.end()) continue;

        Character ch = fontData.characters[c];

        // Position X en pixels écran
        float xpos = cursorX + ch.bearing.x * scale;

        // Position Y:
        // - cursorY est la position du HAUT de la ligne en coordonnées UI
        // - On veut une baseline commune pour tous les caractères
        // - La baseline est typiquement à environ fontSize depuis le haut
        // - bearing.y = distance de la baseline vers le HAUT du glyphe
        // - En OpenGL, Y=0 est en BAS, donc on convertit
        //
        // En coordonnées UI: baseline = cursorY + fontSize * scale
        // Position du HAUT du glyphe en UI = baseline - bearing.y
        // Position du BAS du glyphe en UI = baseline - bearing.y + size.y
        //
        // Conversion en OpenGL (origine en bas):
        // ypos_opengl = screenHeight - ypos_ui

        float baselineY_UI = cursorY + fontData.fontSize * scale;  // Baseline en UI
        float glyphTop_UI = baselineY_UI - ch.bearing.y * scale;   // Haut du glyphe en UI
        float glyphBottom_UI = glyphTop_UI + ch.size.y * scale;    // Bas du glyphe en UI

        // Conversion en OpenGL
        float glyphBottom_GL = screenHeight - glyphBottom_UI;      // Bas du glyphe en OpenGL
        float ypos = glyphBottom_GL;                               // ypos = bas du quad

        float w = ch.size.x * scale;
        float h = ch.size.y * scale;

        // OpenGL: ypos est le bas du quad, ypos + h est le haut
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        cursorX += (ch.advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

float TextRenderer::measureTextWidth(const std::string& text, float scale) {
    if (fonts.empty() || activeFontName.empty()) return 0.0f;

    auto& fontData = fonts[activeFontName];
    float width = 0.0f;

    for (char c : text) {
        if (c == '\n') break;

        if (fontData.characters.find(c) == fontData.characters.end()) {
            continue;
        }

        Character ch = fontData.characters[c];
        width += (ch.advance >> 6) * scale;
    }

    return width;
}

glm::vec2 TextRenderer::measureText(const std::string& text, float scale) {
    if (fonts.empty() || activeFontName.empty()) return {0, 0};

    auto& fontData = fonts[activeFontName];
    float maxWidth = 0.0f;
    float currentWidth = 0.0f;
    int lineCount = 1;

    for (char c : text) {
        if (c == '\n') {
            maxWidth = std::max(maxWidth, currentWidth);
            currentWidth = 0.0f;
            lineCount++;
            continue;
        }

        if (fontData.characters.find(c) != fontData.characters.end()) {
            Character ch = fontData.characters[c];
            currentWidth += (ch.advance >> 6) * scale;
        }
    }

    maxWidth = std::max(maxWidth, currentWidth);
    float height = fontData.fontSize * scale * lineCount * 1.2f;

    return {maxWidth, height};
}

std::vector<std::string> TextRenderer::wrapText(const std::string& text,
                                               float scale, float maxWidth) {
    std::vector<std::string> lines;
    if (text.empty()) return lines;

    auto& fontData = fonts[activeFontName];

    size_t start = 0;
    size_t newlinePos;

    while ((newlinePos = text.find('\n', start)) != std::string::npos) {
        std::string segment = text.substr(start, newlinePos - start);

        while (!segment.empty()) {
            float width = 0.0f;
            size_t i = 0;
            size_t lastSpace = 0;

            for (; i < segment.length(); ++i) {
                char c = segment[i];
                if (c == ' ') lastSpace = i;

                if (fontData.characters.find(c) != fontData.characters.end()) {
                    Character ch = fontData.characters[c];
                    float charWidth = (ch.advance >> 6) * scale;

                    if (width + charWidth > maxWidth && i > 0) {
                        break;
                    }
                    width += charWidth;
                }
            }

            if (i == 0) i = 1;

            if (i < segment.length() && lastSpace > 0) {
                i = lastSpace;
            }

            lines.push_back(segment.substr(0, i));
            segment = segment.substr(i);

            if (!segment.empty() && segment[0] == ' ') {
                segment = segment.substr(1);
            }
        }

        start = newlinePos + 1;
    }

    std::string remaining = text.substr(start);
    while (!remaining.empty()) {
        float width = 0.0f;
        size_t i = 0;
        size_t lastSpace = 0;

        for (; i < remaining.length(); ++i) {
            char c = remaining[i];
            if (c == ' ') lastSpace = i;

            if (fontData.characters.find(c) != fontData.characters.end()) {
                Character ch = fontData.characters[c];
                float charWidth = (ch.advance >> 6) * scale;

                if (width + charWidth > maxWidth && i > 0) {
                    break;
                }
                width += charWidth;
            }
        }

        if (i == 0) i = 1;

        if (i < remaining.length() && lastSpace > 0) {
            i = lastSpace;
        }

        lines.push_back(remaining.substr(0, i));
        remaining = remaining.substr(i);

        if (!remaining.empty() && remaining[0] == ' ') {
            remaining = remaining.substr(1);
        }
    }

    return lines;
}

float TextRenderer::measureLineWidth(const std::string& line, float scale) {
    return measureTextWidth(line, scale);
}

void TextRenderer::applyHorizontalAlignment(TextLayout& layout,
                                           const TextLayoutParams& params) {
    for (auto& line : layout.lines) {
        switch (params.horizontalAlign) {
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

void TextRenderer::applyVerticalAlignment(TextLayout& layout,
                                         const TextLayoutParams& params) {
    if (params.maxHeight <= 0.0f) return;

    float offsetY = 0.0f;

    switch (params.verticalAlign) {
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

    for (auto& line : layout.lines) {
        line.y += offsetY;
    }
}

void TextRenderer::handleOverflow(TextLayout& layout, const TextLayoutParams& params) {
    if (params.overflow == TextOverflow::Visible) return;

    if (params.maxHeight > 0.0f) {
        auto it = layout.lines.begin();
        while (it != layout.lines.end()) {
            if (it->y + it->height > params.maxHeight) {
                layout.hasOverflow = true;

                if (params.overflow == TextOverflow::Hidden) {
                    layout.lines.erase(it, layout.lines.end());
                    break;
                } else if (params.overflow == TextOverflow::Ellipsis && it != layout.lines.begin()) {
                    auto prev = std::prev(it);
                    prev->text += "...";
                    layout.lines.erase(it, layout.lines.end());
                    break;
                }
            }
            ++it;
        }
    }

    if (layout.hasOverflow && params.overflow == TextOverflow::Scroll) {
        layout.maxScroll.y = std::max(0.0f, layout.totalHeight - params.maxHeight);
    }
}

TextLayout TextRenderer::calculateLayout(const std::string& text, float scale,
                                        const TextLayoutParams& params) {
    TextLayout layout;

    if (fonts.empty() || activeFontName.empty() || text.empty()) {
        return layout;
    }

    auto& fontData = fonts[activeFontName];
    float lineHeight = fontData.fontSize * scale * params.lineSpacing;

    std::vector<std::string> textLines;
    if (params.wordWrap && params.maxWidth > 0.0f) {
        float wrapWidth = params.maxWidth - params.padding.x * 2;
        textLines = wrapText(text, scale, wrapWidth);
    } else {
        size_t start = 0;
        size_t end;
        while ((end = text.find('\n', start)) != std::string::npos) {
            textLines.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        textLines.push_back(text.substr(start));
    }

    float y = params.padding.y;
    for (const auto& lineText : textLines) {
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

    if (params.maxWidth > 0.0f && layout.totalWidth > params.maxWidth) {
        layout.hasOverflow = true;
    }
    if (params.maxHeight > 0.0f && layout.totalHeight > params.maxHeight) {
        layout.hasOverflow = true;
    }

    applyHorizontalAlignment(layout, params);
    applyVerticalAlignment(layout, params);
    handleOverflow(layout, params);

    return layout;
}

void TextRenderer::renderTextAdvanced(const std::string& text, float x, float y,
                                     const TextLayoutParams& params,
                                     const glm::vec3& color, float scale) {
    // Le layout calcule des positions relatives positives (ex: ligne 1 à y=0, ligne 2 à y=20)
    TextLayout layout = calculateLayout(text, scale, params);

    // Scissor test pour le clipping (Hidden / Scroll)
    bool useScissor = (params.overflow == TextOverflow::Hidden ||
                       params.overflow == TextOverflow::Scroll) &&
                       params.maxWidth > 0.0f && params.maxHeight > 0.0f;

    if (useScissor) {
        glEnable(GL_SCISSOR_TEST);
        // En OpenGL, scissor commence en bas à gauche
        int scissorY = static_cast<int>(screenHeight - (y + params.maxHeight));
        glScissor(
            static_cast<int>(x),
            scissorY,
            static_cast<int>(params.maxWidth),
            static_cast<int>(params.maxHeight)
        );
    }

    for (const auto& line : layout.lines) {
        // line.x et line.y sont des offsets calculés par calculateLayout
        float drawX = x + line.x - layout.scrollOffset.x;
        float drawY = y + line.y - layout.scrollOffset.y;

        // On appelle renderText en TopLeft car le layout a déjà fait le travail de positionnement
        renderText(line.text, drawX, drawY, scale, color, TextAnchor::TopLeft);
    }

    if (useScissor) {
        glDisable(GL_SCISSOR_TEST);
    }
}

glm::vec2 TextRenderer::calculateMinSize(const std::string& text, float scale,
                                        bool wordWrap) {
    if (wordWrap) {
        std::string longestWord;
        float maxWordWidth = 0.0f;

        size_t start = 0;
        for (size_t i = 0; i <= text.length(); ++i) {
            if (i == text.length() || text[i] == ' ' || text[i] == '\n') {
                std::string word = text.substr(start, i - start);
                float width = measureTextWidth(word, scale);
                if (width > maxWordWidth) {
                    maxWordWidth = width;
                    longestWord = word;
                }
                start = i + 1;
            }
        }

        return {maxWordWidth, fonts[activeFontName].fontSize * scale};
    }

    return measureText(text, scale);
}

glm::vec2 TextRenderer::getCenteredPosition(const std::string& text, float scale,
                                           float rectX, float rectY,
                                           float rectWidth, float rectHeight) {
    glm::vec2 textSize = measureText(text, scale);
    return {
        rectX + (rectWidth - textSize.x) / 2.0f,
        rectY + (rectHeight - textSize.y) / 2.0f
    };
}

float TextRenderer::findOptimalScale(const std::string& text, float maxWidth,
                                     float maxHeight, float minScale, float maxScale) {
    float bestScale = minScale;

    for (float scale = minScale; scale <= maxScale; scale += 0.1f) {
        glm::vec2 size = measureText(text, scale);

        if (size.x <= maxWidth && size.y <= maxHeight) {
            bestScale = scale;
        } else {
            break;
        }
    }

    return bestScale;
}

} // namespace UI
