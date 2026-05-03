#pragma once

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "BindTraits.h"

namespace UI
{

template <typename Func, typename... Args>
struct BindFunc
{
    Func func;
    std::tuple<Args...> args;
    mutable detail::ResultCache<
        decltype(std::invoke(std::declval<Func &>(),
                             detail::ResolveBoundArg(std::declval<Args &>())...))>
        lastResult;

    bool IsDirty() const
    {
        if constexpr (sizeof...(Args) == 0)
        {
            return true;
        }

        return detail::AnyBoundArgsDirty(args);
    }

    bool isDirty() const
    {
        return IsDirty();
    }

    decltype(auto) GetValue() &
    {
        return apply();
    }

    decltype(auto) GetValue() const &
    {
        return apply();
    }

    decltype(auto) GetValue() &&
    {
        return static_cast<BindFunc &>(*this).apply();
    }

    using InvokeResult = decltype(std::invoke(std::declval<Func &>(),
                                              detail::ResolveBoundArg(std::declval<Args &>())...));
    using ApplyResult = std::conditional_t<std::is_void_v<InvokeResult>,
                                           void,
                                           std::remove_cv_t<std::remove_reference_t<InvokeResult>>>;

    auto apply() & -> ApplyResult
    {
        return ApplyImpl(*this);
    }

    auto apply() const & -> ApplyResult
    {
        return ApplyImpl(*this);
    }

    auto apply() && -> ApplyResult
    {
        return static_cast<BindFunc &>(*this).apply();
    }

    auto operator()() & -> ApplyResult
    {
        return apply();
    }

    auto operator()() const & -> ApplyResult
    {
        return apply();
    }

    auto operator()() && -> ApplyResult
    {
        return static_cast<BindFunc &>(*this).apply();
    }

  private:
    template <typename Self>
    static ApplyResult ApplyImpl(Self &self)
    {
        if (!self.lastResult.HasValue() || self.IsDirty())
        {
            if constexpr (std::is_void_v<InvokeResult>)
            {
                detail::InvokeTuple(self.func, self.args);
                detail::CommitBoundArgs(self.args);
                self.lastResult.Store();
                return;
            }
            else
            {
                ApplyResult result = static_cast<ApplyResult>(
                    detail::InvokeTuple(self.func, self.args));
                detail::CommitBoundArgs(self.args);
                self.lastResult.Store(std::move(result));
                return result;
            }
        }

        if constexpr (!std::is_void_v<InvokeResult>)
        {
            return self.lastResult.Get();
        }
    }
};

template <typename R, typename... Params, typename... Args>
auto Call(R (*f)(Params...), Args &&...args)
{
    static_assert(sizeof...(Params) == sizeof...(Args),
                  "UI::Call: the number of bound arguments must match the function signature");

    return BindFunc<R (*)(Params...),
                    decltype(detail::MakeBoundValueForParam<Params>(std::forward<Args>(args)))...>{
        f,
        std::tuple{detail::MakeBoundValueForParam<Params>(std::forward<Args>(args))...}};
}

template <typename Func, typename... Args>
auto Call(Func &&f, Args &&...args)
{
    return BindFunc<std::decay_t<Func>,
                    decltype(detail::MakeBoundValue(std::forward<Args>(args)))...>{
        std::forward<Func>(f),
        std::tuple{detail::MakeBoundValue(std::forward<Args>(args))...}};
}

// Always re-evaluates every tick regardless of arguments.
// Use for calls to external functions that read global state (Profiler, timers, RNG).
template <typename Callable>
auto BindAlways(Callable &&callable)
{
    return Call(std::forward<Callable>(callable));
}

} // namespace UI
