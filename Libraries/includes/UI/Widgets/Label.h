#pragma once

#include "Text.h"

namespace UI
{

/**
 * @cui-alias Text
 * @cui-factory UI::CreateLabel
 */
using Label = Text;

inline std::shared_ptr<Label> CreateLabel(Bounds bounds = Bounds(), std::string text = {}, float scale = 1.0f)
{
    return CreateText(bounds, TextContent(std::move(text)), scale, IdentifierKind::TEXT_MUTED);
}

inline std::shared_ptr<Label> CreateLabel(Bounds bounds, TextContent content, float scale = 1.0f)
{
    return CreateText(bounds, std::move(content), scale, IdentifierKind::TEXT_MUTED);
}

} // namespace UI
