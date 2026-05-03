#pragma once

#include "BoundTypes/BindFunc.h"
#include "BoundTypes/BindMethods.h"
#include "BoundTypes/BindValue.h"

#include <concepts>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace UI
{
namespace textdetail
{

template <typename T>
std::string ToString(T &&value)
{
    using ValueType = std::remove_cv_t<std::remove_reference_t<T>>;

    if constexpr (std::is_same_v<ValueType, std::string>)
    {
        return std::forward<T>(value);
    }
    else if constexpr (std::is_constructible_v<std::string, T &&>)
    {
        return std::string(std::forward<T>(value));
    }
    else
    {
        std::ostringstream output;
        output << std::forward<T>(value);
        return output.str();
    }
}

template <typename T>
constexpr bool IsTextLike_v = std::is_constructible_v<std::string, T &&>;

template <typename T>
constexpr bool IsCharPointerLike_v = std::is_pointer_v<std::remove_reference_t<T>> &&
                                     std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>, char>;

template <typename T>
concept TextBinding = requires(const T &value) {
    { value.GetValue() };
    { value.IsDirty() } -> std::convertible_to<bool>;
} && !std::is_void_v<std::remove_cv_t<std::remove_reference_t<decltype(std::declval<const T &>().GetValue())>>>;

template <typename T>
bool ValuesEqual(const T &lhs, const T &rhs)
{
    if constexpr (requires { { lhs == rhs } -> std::convertible_to<bool>; })
    {
        return lhs == rhs;
    }
    else if constexpr (requires { { lhs != rhs } -> std::convertible_to<bool>; })
    {
        return !(lhs != rhs);
    }
    else
    {
        return false;
    }
}

template <typename T>
void CommitIfAvailable(T &value)
{
    if constexpr (requires { value.Commit(); })
    {
        value.Commit();
    }
}

} // namespace textdetail

class TextContent
{
  private:
    struct PartBase
    {
        virtual ~PartBase() = default;
        virtual std::unique_ptr<PartBase> Clone() const = 0;
        virtual bool IsDirty() const = 0;
        virtual std::string GetText() const = 0;
    };

    struct StaticTextPart final : PartBase
    {
        explicit StaticTextPart(std::string value) : text(std::move(value)) {}

        std::unique_ptr<PartBase> Clone() const override
        {
            return std::make_unique<StaticTextPart>(*this);
        }

        bool IsDirty() const override
        {
            return false;
        }

        std::string GetText() const override
        {
            return text;
        }

        std::string text;
    };

    template <typename T>
    struct StaticValuePart final : PartBase
    {
        using ValueType = std::remove_cv_t<std::remove_reference_t<T>>;

        explicit StaticValuePart(ValueType value) : storedValue(std::move(value)) {}

        std::unique_ptr<PartBase> Clone() const override
        {
            return std::make_unique<StaticValuePart>(*this);
        }

        bool IsDirty() const override
        {
            return false;
        }

        std::string GetText() const override
        {
            if (!cachedText.has_value())
            {
                cachedText.emplace(textdetail::ToString(storedValue));
            }

            return *cachedText;
        }

        ValueType storedValue;
        mutable std::optional<std::string> cachedText;
    };

    template <typename T>
    struct DynamicValuePart final : PartBase
    {
        using ValueType = std::remove_cv_t<std::remove_reference_t<T>>;

        explicit DynamicValuePart(const ValueType *value) : sourceValue(value) {}

        std::unique_ptr<PartBase> Clone() const override
        {
            return std::make_unique<DynamicValuePart>(*this);
        }

        bool IsDirty() const override
        {
            if (sourceValue == nullptr)
            {
                return false;
            }

            if (!lastValue.has_value())
            {
                return true;
            }

            return !textdetail::ValuesEqual(*lastValue, *sourceValue);
        }

        std::string GetText() const override
        {
            if (sourceValue == nullptr)
            {
                lastValue.reset();
                cachedText.clear();
                return cachedText;
            }

            if (IsDirty())
            {
                lastValue = *sourceValue;
                cachedText = textdetail::ToString(*sourceValue);
            }

            return cachedText;
        }

        const ValueType *sourceValue = nullptr;
        mutable std::optional<ValueType> lastValue;
        mutable std::string cachedText;
    };

    template <textdetail::TextBinding T>
    struct BindingPart final : PartBase
    {
        explicit BindingPart(T value) : source(std::move(value)) {}

        std::unique_ptr<PartBase> Clone() const override
        {
            return std::make_unique<BindingPart>(*this);
        }

        bool IsDirty() const override
        {
            return source.IsDirty();
        }

        std::string GetText() const override
        {
            if (!cachedText.has_value() || source.IsDirty())
            {
                cachedText = textdetail::ToString(source.GetValue());
                textdetail::CommitIfAvailable(source);
            }

            return *cachedText;
        }

        T source;
        mutable std::optional<std::string> cachedText;
    };

    std::vector<std::unique_ptr<PartBase>> parts;
    mutable std::string cachedText;
    mutable bool cacheValid = false;

    void InvalidateCache()
    {
        cacheValid = false;
        cachedText.clear();
    }

    template <typename PartType, typename... Args>
    TextContent &EmplacePart(Args &&...args)
    {
        auto part = std::make_unique<PartType>(std::forward<Args>(args)...);
        parts.push_back(std::move(part));
        InvalidateCache();
        return *this;
    }

    template <typename T>
    TextContent &AppendPartImpl(T &&part)
    {
        using PartType = std::remove_cv_t<std::remove_reference_t<T>>;

        if constexpr (textdetail::IsTextLike_v<T &&>)
        {
            return AppendText(std::string(std::forward<T>(part)));
        }
        else if constexpr (textdetail::TextBinding<PartType>)
        {
            return AppendBinding(std::forward<T>(part));
        }
        else if constexpr (std::is_pointer_v<PartType> && !textdetail::IsCharPointerLike_v<T &&>)
        {
            return AppendValue(std::forward<T>(part));
        }
        else
        {
            return AppendValue(std::forward<T>(part));
        }
    }

  public:
    TextContent() = default;

    TextContent(const TextContent &other)
    {
        parts.reserve(other.parts.size());
        for (const auto &part : other.parts)
        {
            parts.push_back(part->Clone());
        }

        cachedText = other.cachedText;
        cacheValid = other.cacheValid;
    }

    TextContent(TextContent &&) noexcept = default;

    TextContent &operator=(const TextContent &other)
    {
        if (this == &other)
        {
            return *this;
        }

        parts.clear();
        parts.reserve(other.parts.size());
        for (const auto &part : other.parts)
        {
            parts.push_back(part->Clone());
        }

        cachedText = other.cachedText;
        cacheValid = other.cacheValid;
        return *this;
    }

    TextContent &operator=(TextContent &&) noexcept = default;

    template <typename... Args>
    explicit TextContent(Args &&...args)
    {
        Append(std::forward<Args>(args)...);
    }

    bool Empty() const
    {
        return parts.empty();
    }

    bool IsDirty() const
    {
        if (!cacheValid)
        {
            return true;
        }

        for (const auto &part : parts)
        {
            if (part->IsDirty())
            {
                return true;
            }
        }

        return false;
    }

    bool isDirty() const
    {
        return IsDirty();
    }

    TextContent &Clear()
    {
        parts.clear();
        InvalidateCache();
        return *this;
    }

    TextContent &ClearParts()
    {
        return Clear();
    }

    TextContent &SetText(std::string text)
    {
        Clear();
        return AppendText(std::move(text));
    }

    TextContent &AppendText(std::string text)
    {
        return EmplacePart<StaticTextPart>(std::move(text));
    }

    TextContent &AppendText(const char *text)
    {
        return AppendText(text == nullptr ? std::string{} : std::string(text));
    }

    TextContent &AppendText(char *text)
    {
        return AppendText(static_cast<const char *>(text));
    }

    template <typename T>
    TextContent &AppendValue(const T *value)
        requires(!std::is_same_v<std::remove_cv_t<T>, char>)
    {
        if (value == nullptr)
        {
            return AppendText(std::string{});
        }

        return EmplacePart<DynamicValuePart<T>>(value);
    }

    TextContent &AppendValue(const char *text)
    {
        return AppendText(text);
    }

    TextContent &AppendValue(char *text)
    {
        return AppendText(text);
    }

    template <typename T>
    TextContent &AppendValue(T value)
        requires(!std::is_pointer_v<std::remove_cv_t<std::remove_reference_t<T>>> &&
                 !textdetail::TextBinding<std::remove_cv_t<std::remove_reference_t<T>>> &&
                 !textdetail::IsTextLike_v<T &&>)
    {
        using ValueType = std::remove_cv_t<std::remove_reference_t<T>>;
        return EmplacePart<StaticValuePart<ValueType>>(std::move(value));
    }

    template <textdetail::TextBinding T>
    TextContent &AppendBinding(T value)
    {
        using BindingType = std::remove_cv_t<std::remove_reference_t<T>>;
        return EmplacePart<BindingPart<BindingType>>(std::move(value));
    }

    template <typename TObject, typename Method, typename... Args>
    TextContent &AppendMethod(TObject *object, Method method, Args &&...args)
    {
        return AppendBinding(UI::Bind(object, method, std::forward<Args>(args)...));
    }

    template <typename TObject, typename Method, typename... Args>
    TextContent &AppendMethod(TObject &object, Method method, Args &&...args)
    {
        return AppendMethod(&object, method, std::forward<Args>(args)...);
    }

    template <typename Func, typename... Args>
    TextContent &AppendFunction(Func &&func, Args &&...args)
    {
        return AppendBinding(UI::Call(std::forward<Func>(func), std::forward<Args>(args)...));
    }

    template <typename T>
    TextContent &AppendPart(T &&part)
    {
        return AppendPartImpl(std::forward<T>(part));
    }

    template <typename... Args>
    TextContent &Append(Args &&...args)
    {
        (AppendPart(std::forward<Args>(args)), ...);
        return *this;
    }

    std::string BuildText() const
    {
        if (!IsDirty())
        {
            return cachedText;
        }

        std::string text;
        for (const auto &part : parts)
        {
            text += part->GetText();
        }

        cachedText = std::move(text);
        cacheValid = true;
        return cachedText;
    }

    std::string GetValue() const
    {
        return BuildText();
    }

    std::string GetText() const
    {
        return BuildText();
    }

    std::string apply() const
    {
        return BuildText();
    }
};

} // namespace UI
