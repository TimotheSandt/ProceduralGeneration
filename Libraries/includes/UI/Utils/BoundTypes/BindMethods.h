#pragma once

#include <concepts>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "BindTraits.h"

namespace UI
{
namespace detail
{

template <typename T>
inline constexpr bool CanTrackMethodObject_v =
    std::is_copy_constructible_v<T> &&
    requires(const T &lhs, const T &rhs) {
        { lhs == rhs } -> std::convertible_to<bool>;
    };

template <typename T, bool Track = CanTrackMethodObject_v<T>>
struct MethodObjectState;

template <typename T>
struct MethodObjectState<T, true>
{
    static constexpr bool Tracked = true;

    BindValue<T> value;

    MethodObjectState() = default;

    explicit MethodObjectState(BindValue<T> objectValue) : value(std::move(objectValue)) {}

    bool IsDirty() const
    {
        return value.IsDirty();
    }

    void Commit() const
    {
        value.Commit();
    }
};

template <typename T>
struct MethodObjectState<T, false>
{
    static constexpr bool Tracked = false;

    bool IsDirty() const
    {
        return true;
    }

    void Commit() const
    {
    }
};

template <typename TObject>
auto MakeMethodObjectState(TObject &object)
{
    using ObjectType = std::remove_cv_t<std::remove_reference_t<TObject>>;

    if constexpr (MethodObjectState<ObjectType>::Tracked)
    {
        if constexpr (std::is_const_v<std::remove_reference_t<TObject>>)
        {
            return MethodObjectState<ObjectType>{BindStatic(object)};
        }
        else
        {
            return MethodObjectState<ObjectType>{BindDynamic(object)};
        }
    }
    else
    {
        return MethodObjectState<ObjectType>{};
    }
}

template <typename TObject>
auto MakeMethodObjectState(TObject *object)
{
    using ObjectType = std::remove_cv_t<std::remove_reference_t<TObject>>;

    if constexpr (MethodObjectState<ObjectType>::Tracked)
    {
        if constexpr (std::is_const_v<TObject>)
        {
            return MethodObjectState<ObjectType>{BindStatic(*object)};
        }
        else
        {
            return MethodObjectState<ObjectType>{BindDynamic(*object)};
        }
    }
    else
    {
        return MethodObjectState<ObjectType>{};
    }
}

template <typename T, bool Track>
bool IsDirtyBoundArg(const MethodObjectState<T, Track> &value)
{
    return value.IsDirty();
}

template <typename T, bool Track>
void CommitBoundArg(MethodObjectState<T, Track> &value)
{
    value.Commit();
}

template <typename T, bool Track>
void CommitBoundArg(const MethodObjectState<T, Track> &value)
{
    value.Commit();
}

} // namespace detail

template <typename TObject, typename Method, typename... Args>
struct BindMethod
{
    using ObjectType = std::remove_cv_t<std::remove_reference_t<TObject>>;
    using ObjectState = detail::MethodObjectState<ObjectType>;

    TObject *object;
    Method method;
    ObjectState objectValue;
    std::tuple<Args...> args;
    mutable detail::ResultCache<
        decltype(std::invoke(std::declval<Method &>(),
                             std::declval<TObject &>(),
                             detail::ResolveBoundArg(std::declval<Args &>())...))>
        lastResult;

    bool IsDirty() const
    {
        return detail::IsDirtyBoundArg(objectValue) || detail::AnyBoundArgsDirty(args);
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
        return static_cast<BindMethod &>(*this).apply();
    }

    using InvokeResult = decltype(std::invoke(std::declval<Method &>(),
                                              std::declval<TObject &>(),
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
        return static_cast<BindMethod &>(*this).apply();
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
        return static_cast<BindMethod &>(*this).apply();
    }

  private:
    template <typename Self>
    static ApplyResult ApplyImpl(Self &self)
    {
        if (!self.lastResult.HasValue() || self.IsDirty())
        {
            if constexpr (std::is_void_v<InvokeResult>)
            {
                detail::InvokeTuple(
                    [&self](auto &&...unpacked) -> decltype(auto) {
                        return std::invoke(self.method, self.object,
                                            std::forward<decltype(unpacked)>(unpacked)...);
                    },
                    self.args);
                detail::CommitBoundArg(self.objectValue);
                detail::CommitBoundArgs(self.args);
                self.lastResult.Store();
                return;
            }
            else
            {
                ApplyResult result = static_cast<ApplyResult>(detail::InvokeTuple(
                    [&self](auto &&...unpacked) -> decltype(auto) {
                        return std::invoke(self.method, self.object,
                                            std::forward<decltype(unpacked)>(unpacked)...);
                    },
                    self.args));
                detail::CommitBoundArg(self.objectValue);
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

template <typename TObject, typename R, typename... Params, typename... Args>
auto Bind(TObject &obj, R (TObject::*m)(Params...), Args &&...args)
{
    static_assert(sizeof...(Params) == sizeof...(Args),
                  "UI::Bind: the number of bound arguments must match the method signature");

    return BindMethod<TObject, R (TObject::*)(Params...),
                      decltype(detail::MakeBoundValueForParam<Params>(std::forward<Args>(args)))...>{
        &obj,
        m,
        detail::MakeMethodObjectState(obj),
        std::tuple{detail::MakeBoundValueForParam<Params>(std::forward<Args>(args))...}};
}

template <typename TObject, typename R, typename... Params, typename... Args>
auto Bind(TObject &obj, R (TObject::*m)(Params...) const, Args &&...args)
{
    static_assert(sizeof...(Params) == sizeof...(Args),
                  "UI::Bind: the number of bound arguments must match the method signature");

    return BindMethod<TObject, R (TObject::*)(Params...) const,
                      decltype(detail::MakeBoundValueForParam<Params>(std::forward<Args>(args)))...>{
        &obj,
        m,
        detail::MakeMethodObjectState(obj),
        std::tuple{detail::MakeBoundValueForParam<Params>(std::forward<Args>(args))...}};
}

template <typename TObject, typename R, typename... Params, typename... Args>
auto Bind(TObject *obj, R (TObject::*m)(Params...), Args &&...args)
{
    static_assert(sizeof...(Params) == sizeof...(Args),
                  "UI::Bind: the number of bound arguments must match the method signature");

    return BindMethod<TObject, R (TObject::*)(Params...),
                      decltype(detail::MakeBoundValueForParam<Params>(std::forward<Args>(args)))...>{
        obj,
        m,
        detail::MakeMethodObjectState(obj),
        std::tuple{detail::MakeBoundValueForParam<Params>(std::forward<Args>(args))...}};
}

template <typename TObject, typename R, typename... Params, typename... Args>
auto Bind(TObject *obj, R (TObject::*m)(Params...) const, Args &&...args)
{
    static_assert(sizeof...(Params) == sizeof...(Args),
                  "UI::Bind: the number of bound arguments must match the method signature");

    return BindMethod<TObject, R (TObject::*)(Params...) const,
                      decltype(detail::MakeBoundValueForParam<Params>(std::forward<Args>(args)))...>{
        obj,
        m,
        detail::MakeMethodObjectState(obj),
        std::tuple{detail::MakeBoundValueForParam<Params>(std::forward<Args>(args))...}};
}

template <typename TObject, typename Method, typename... Args>
auto Bind(TObject &obj, Method m, Args &&...args)
{
    return BindMethod<TObject, Method, decltype(detail::MakeBoundValue(std::forward<Args>(args)))...>{
        &obj,
        m,
        detail::MakeMethodObjectState(obj),
        std::tuple{detail::MakeBoundValue(std::forward<Args>(args))...}};
}

template <typename TObject, typename Method, typename... Args>
auto Bind(TObject *obj, Method m, Args &&...args)
{
    return BindMethod<TObject, Method, decltype(detail::MakeBoundValue(std::forward<Args>(args)))...>{
        obj,
        m,
        detail::MakeMethodObjectState(obj),
        std::tuple{detail::MakeBoundValue(std::forward<Args>(args))...}};
}

} // namespace UI
