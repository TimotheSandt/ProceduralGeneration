#include "Rendering/Theme.h"

#include <utility>

namespace UI
{

std::unordered_map<std::string, std::shared_ptr<Theme>> Theme::themes = {
    std::make_pair("default", std::shared_ptr<Theme>(new Theme("default", Colors(), 5.0f, 15.0f, 15.0f))),
    std::make_pair("dark",
                   std::shared_ptr<Theme>(new Theme("dark",
                                                        Colors(glm::vec4(0.15f, 0.15f, 0.2f, 1.0f), glm::vec4(0.3f, 0.6f, 1.0f, 1.0f),
                                                                 glm::vec4(0.5f, 0.5f, 0.6f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                                                                 glm::vec4(0.7f, 0.7f, 0.7f, 1.0f), glm::vec4(0.4f, 0.7f, 1.0f, 1.0f),
                                                                 glm::vec4(0.2f, 0.5f, 0.9f, 1.0f), glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
                                                                 glm::vec4(1.0f, 0.3f, 0.3f, 1.0f), glm::vec4(0.3f, 1.0f, 0.5f, 1.0f)),
                                                        5.0f, 15.0f, 15.0f)))};

Theme::Theme(std::string name, Colors colors, float cornerRadius, float padding, float spacing)
    : name(std::move(name)), colors(colors), cornerRadius(cornerRadius), padding(padding), spacing(spacing)
{
}

void Theme::CreateTheme(const std::string &name, Colors colors, float cornerRadius, float padding, float spacing)
{
    themes[name] = std::shared_ptr<Theme>(new Theme(name, colors, cornerRadius, padding, spacing));
}

std::weak_ptr<Theme> Theme::GetTheme(const std::string &themeName)
{
    auto it = Theme::themes.find(themeName);
    if (it != Theme::themes.end())
    {
        return it->second;
    }
    return std::weak_ptr<Theme>();
}

glm::vec4 Theme::GetColor(IdentifierKind kind) const
{
    switch (kind)
    {
        case IdentifierKind::TRANSPARENT:
            return colors.transparent;
        case IdentifierKind::BACKGROUND:
            return colors.background;
        case IdentifierKind::PRIMARY:
            return colors.primary;
        case IdentifierKind::SECONDARY:
            return colors.secondary;
        case IdentifierKind::TEXT:
            return colors.text;
        case IdentifierKind::TEXT_MUTED:
            return colors.textMuted;
        case IdentifierKind::HOVER:
            return colors.hover;
        case IdentifierKind::PRESSED:
            return colors.pressed;
        case IdentifierKind::DISABLED:
            return colors.disabled;
        case IdentifierKind::ERROR:
            return colors.error;
        case IdentifierKind::SUCCESS:
            return colors.success;
        default:
            return colors.primary;
    }
}

} // namespace UI
