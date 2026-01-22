// Libraries/includes/UI/TextRenderer.h
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace UI {

// Forward declarations
class Shader;

// Ancrage du texte
enum class TextAnchor {
    TopLeft, TopCenter, TopRight,
    CenterLeft, Center, CenterRight,
    BottomLeft, BottomCenter, BottomRight
};

// Représentation d'un caractère
struct Character {
    unsigned int textureID;
    glm::ivec2 size;
    glm::ivec2 bearing;
    unsigned int advance;
};

// Options de débordement
enum class TextOverflow {
    Visible,    // Texte visible même en dehors des limites
    Hidden,     // Coupé aux limites
    Ellipsis,   // "..." à la fin
    Scroll      // Scrollable
};

// Alignement horizontal
enum class TextAlign {
    Left,
    Center,
    Right
};

// Alignement vertical
enum class VerticalAlign {
    Top,
    Middle,
    Bottom
};

// Paramètres de layout
struct TextLayoutParams {
    float maxWidth = 0.0f;          // 0 = illimité
    float maxHeight = 0.0f;         // 0 = illimité
    bool wordWrap = false;          // Retour à la ligne auto
    TextOverflow overflow = TextOverflow::Visible;
    TextAlign horizontalAlign = TextAlign::Left;
    VerticalAlign verticalAlign = VerticalAlign::Top;
    float lineSpacing = 1.2f;       // Espacement lignes
    glm::vec2 padding = {0, 0};     // Padding interne
};

// Ligne de texte calculée
struct TextLine {
    std::string text;
    float width;
    float height;
    float x, y;  // Position relative
};

// Résultat du calcul de layout
struct TextLayout {
    std::vector<TextLine> lines;
    float totalWidth = 0.0f;
    float totalHeight = 0.0f;
    bool hasOverflow = false;
    glm::vec2 scrollOffset = {0, 0};
    glm::vec2 maxScroll = {0, 0};
};

// Classe principale
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    // Initialisation (après contexte OpenGL)
    bool init(unsigned int screenWidth, unsigned int screenHeight);

    // Chargement de polices
    bool loadFont(const std::string& fontPath, const std::string& fontName,
                  unsigned int fontSize = 48);
    void setActiveFont(const std::string& fontName);

    // Mise à jour taille écran
    void updateScreenSize(unsigned int width, unsigned int height);

    // === RENDU SIMPLE ===
    void renderText(const std::string& text, float x, float y, float scale = 1.0f,
                   const glm::vec3& color = glm::vec3(1.0f),
                   TextAnchor anchor = TextAnchor::TopLeft);

    // === RENDU AVANCÉ AVEC LAYOUT ===
    void renderTextAdvanced(const std::string& text, float x, float y,
                           const TextLayoutParams& params,
                           const glm::vec3& color = glm::vec3(1.0f),
                           float scale = 1.0f);

    // === MESURES ===
    float measureTextWidth(const std::string& text, float scale = 1.0f);
    glm::vec2 measureText(const std::string& text, float scale = 1.0f);

    // Calcul du layout complet
    TextLayout calculateLayout(const std::string& text, float scale,
                              const TextLayoutParams& params);

    // Calcul taille minimale pour contenir le texte
    glm::vec2 calculateMinSize(const std::string& text, float scale = 1.0f,
                              bool wordWrap = false);

    // === HELPERS POUR UI ===
    // Centrer texte dans un rectangle
    glm::vec2 getCenteredPosition(const std::string& text, float scale,
                                  float rectX, float rectY,
                                  float rectWidth, float rectHeight);

    // Trouver la taille de police optimale pour un rectangle
    float findOptimalScale(const std::string& text, float maxWidth, float maxHeight,
                          float minScale = 0.3f, float maxScale = 2.0f);

private:
    FT_Library ft;

    struct FontData {
        FT_Face face;
        std::unordered_map<char, Character> characters;
        unsigned int fontSize;
    };

    std::unordered_map<std::string, FontData> fonts;
    std::string activeFontName;

    // OpenGL
    unsigned int VAO, VBO;
    std::unique_ptr<Shader> shader;
    glm::mat4 projection;
    unsigned int screenWidth, screenHeight;

    // Méthodes privées
    void setupRenderData();
    Character loadCharacter(FT_Face face, char c);
    glm::vec2 calculateAnchorOffset(const std::string& text, float scale, TextAnchor anchor);

    // Layout helpers
    std::vector<std::string> wrapText(const std::string& text, float scale,
                                     float maxWidth);
    float measureLineWidth(const std::string& line, float scale);
    void applyHorizontalAlignment(TextLayout& layout, const TextLayoutParams& params);
    void applyVerticalAlignment(TextLayout& layout, const TextLayoutParams& params);
    void handleOverflow(TextLayout& layout, const TextLayoutParams& params);
};

// Shader helper simple (vous pouvez utiliser votre classe Shader existante)
class Shader {
public:
    Shader(const char* vertexSource, const char* fragmentSource);
    ~Shader();

    void use();
    void setMat4(const std::string& name, const glm::mat4& mat);
    void setVec3(const std::string& name, const glm::vec3& vec);
    void setInt(const std::string& name, int value);

    unsigned int ID;
};

} // namespace UI
