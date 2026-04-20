#pragma once

#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace UI
{

template <typename TObject, typename Method, typename... Args>
struct BoundMethod
{
    TObject *object;
    Method method;
    std::tuple<Args...> args;
};

template <typename TObject, typename Method, typename... Args>
auto Bind(TObject &obj, Method m, Args &&...args)
{
    return BoundMethod<TObject, Method, std::decay_t<Args>...>{&obj, m, {std::forward<Args>(args)...}};
}

template <typename TObject, typename Method, typename... Args>
auto Bind(TObject *obj, Method m, Args &&...args)
{
    return BoundMethod<TObject, Method, std::decay_t<Args>...>{obj, m, {std::forward<Args>(args)...}};
}

class TextContent
{
    using Part = std::variant<std::string, std::function<void(std::ostream &)>>;
    std::vector<Part> parts;

    template <typename T>
    auto AddPart(T &&text) -> std::enable_if_t<std::is_convertible_v<T, std::string>>
    {
        if (std::string s(std::forward<T>(text)); !s.empty())
            parts.push_back(std::move(s));
    }

    template <typename T>
    auto AddPart(const T *ptr) -> std::enable_if_t<!std::is_same_v<T, char> && !std::is_function_v<T>>
    {
        if (ptr)
            parts.push_back([ptr](std::ostream &s) { s << *ptr; });
    }

    template <typename F>
    auto AddPart(F &&fn) -> std::enable_if_t<std::is_invocable_v<F, std::ostream &>
                                             && !std::is_convertible_v<F, std::string>>
    {
        parts.push_back(std::forward<F>(fn));
    }

    template <typename O, typename M, typename... A>
    void AddPart(BoundMethod<O, M, A...> b)
    {
        if (b.object)
            parts.push_back([b](std::ostream &s)
            {
                s << std::apply([&](const auto &...args)
                { return (b.object->*b.method)(args...); }, b.args);
            });
    }

  public:
    TextContent() = default;

    template <typename... Args>
    explicit TextContent(Args &&...args)
    {
        parts.reserve(sizeof...(args));
        (AddPart(std::forward<Args>(args)), ...);
    }

    bool Empty() const { return parts.empty(); }

    std::string BuildText() const
    {
        if (parts.empty())
            return {};

        std::ostringstream out;
        for (const auto &p : parts)
            std::visit([&out](const auto &v)
            {
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>)
                    out << v;
                else if (v)
                    v(out);
            }, p);

        return out.str();
    }
};

} // namespace UI
