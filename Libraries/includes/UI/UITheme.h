#pragma once
#include <glm/glm.hpp>

#include <unordered_map>
#include <string>
#include <memory>

namespace UI {

enum class IdentifierKind {
    BACKGROUND,
    PRIMARY,
    SECONDARY,
    TEXT,
    TEXT_MUTED,
    HOVER,
    PRESSED,
    DISABLED,
    ERROR,
    SUCCESS
};

enum class PresetTheme {
    LIGHT,
    DARK,
};

struct UIColors {
    glm::vec4 background = {0.15f, 0.15f, 0.2f, 1.0f};
    glm::vec4 primary = {0.3f, 0.6f, 1.0f, 1.0f};
    glm::vec4 secondary = {0.5f, 0.5f, 0.6f, 1.0f};
    glm::vec4 text = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 textMuted = {0.7f, 0.7f, 0.7f, 1.0f};
    glm::vec4 hover = {0.4f, 0.7f, 1.0f, 1.0f};
    glm::vec4 pressed = {0.2f, 0.5f, 0.9f, 1.0f};
    glm::vec4 disabled = {0.3f, 0.3f, 0.3f, 1.0f};
    glm::vec4 error = {1.0f, 0.3f, 0.3f, 1.0f};
    glm::vec4 success = {0.3f, 1.0f, 0.5f, 1.0f};
};

class UITheme {
public:
    UIColors GetColors() const { return colors; }
    glm::vec4 GetColor(IdentifierKind kind) const;
    std::string GetName() const { return name; }
    float GetCornerRadius() const { return cornerRadius; }
    float GetPadding() const { return padding; }
    float GetSpacing() const { return spacing; }

public:
    static std::weak_ptr<UITheme> GetTheme(std::string themeName);
    static void CreateTheme(std::string name, UIColors colors, float cornerRadius, float padding, float spacing);

private:
    std::string name;
    UIColors colors;
    float cornerRadius;
    float padding;
    float spacing;

    UITheme(std::string name, UIColors colors, float cornerRadius, float padding, float spacing);

    static std::unordered_map<std::string, std::shared_ptr<UITheme>> themes;
};

}
