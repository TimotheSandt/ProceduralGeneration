#pragma once

#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "BindValue.h"

namespace UI
{
namespace detail
{

template <typename T>
struct IsBindValue : std::false_type
{
};

template <typename T>
struct IsBindValue<BindValue<T>> : std::true_type
{
};

template <typename T>
inline constexpr bool IsBindValue_v =
    IsBindValue<std::remove_cv_t<std::remove_reference_t<T>>>::value;

template <typename T>
struct IsReferenceWrapper : std::false_type
{
};

template <typename T>
struct IsReferenceWrapper<std::reference_wrapper<T>> : std::true_type
{
};

template <typename T>
inline constexpr bool IsReferenceWrapper_v =
    IsReferenceWrapper<std::remove_cv_t<std::remove_reference_t<T>>>::value;

template <typename T>
auto MakeBoundValue(T &&value)
{
    if constexpr (IsBindValue_v<T>)
    {
        return std::forward<T>(value);
    }
    else if constexpr (IsReferenceWrapper_v<T>)
    {
        if constexpr (std::is_const_v<std::remove_reference_t<decltype(value.get())>>)
        {
            return BindStatic(value.get());
        }
        else
        {
            return BindDynamic(value.get());
        }
    }
    else
    {
        return BindStatic(std::forward<T>(value));
    }
}

template <typename Param, typename T>
auto MakeBoundValueForParam(T &&value)
{
    if constexpr (IsBindValue_v<T>)
    {
        return std::forward<T>(value);
    }
    else if constexpr (IsReferenceWrapper_v<T>)
    {
        if constexpr (std::is_const_v<std::remove_reference_t<decltype(value.get())>>)
        {
            return BindStatic(value.get());
        }
        else
        {
            return BindDynamic(value.get());
        }
    }
    else if constexpr (std::is_lvalue_reference_v<Param> &&
                       std::is_lvalue_reference_v<T> &&
                       !std::is_const_v<std::remove_reference_t<T>>)
    {
        return BindDynamic(value);
    }
    else
    {
        return BindStatic(std::forward<T>(value));
    }
}

template <typename T>
decltype(auto) ResolveBoundArg(T &value)
{
    return value;
}

template <typename T>
decltype(auto) ResolveBoundArg(const T &value)
{
    return value;
}

template <typename T>
decltype(auto) ResolveBoundArg(BindValue<T> &value)
{
    return value.Get();
}

template <typename T>
decltype(auto) ResolveBoundArg(const BindValue<T> &value)
{
    return value.Get();
}

template <typename T>
decltype(auto) ResolveBoundArg(BindValue<T> &&value)
{
    return std::move(value).Get();
}

template <typename T>
bool IsDirtyBoundArg(const T &)
{
    return false;
}

template <typename T>
bool IsDirtyBoundArg(const BindValue<T> &value)
{
    return value.IsDirty();
}

template <typename... Args, std::size_t... Indexes>
bool AnyBoundArgsDirtyImpl(const std::tuple<Args...> &args,
                           std::index_sequence<Indexes...>)
{
    return (... || IsDirtyBoundArg(std::get<Indexes>(args)));
}

template <typename... Args>
bool AnyBoundArgsDirty(const std::tuple<Args...> &args)
{
    return AnyBoundArgsDirtyImpl(args, std::index_sequence_for<Args...>{});
}

template <typename T>
void CommitBoundArg(T &)
{
}

template <typename T>
void CommitBoundArg(const T &)
{
}

template <typename T>
void CommitBoundArg(BindValue<T> &value)
{
    value.Commit();
}

template <typename T>
void CommitBoundArg(const BindValue<T> &value)
{
    value.Commit();
}

template <typename... Args, std::size_t... Indexes>
void CommitBoundArgsImpl(std::tuple<Args...> &args,
                         std::index_sequence<Indexes...>)
{
    (CommitBoundArg(std::get<Indexes>(args)), ...);
}

template <typename... Args>
void CommitBoundArgs(std::tuple<Args...> &args)
{
    CommitBoundArgsImpl(args, std::index_sequence_for<Args...>{});
}

template <typename... Args, std::size_t... Indexes>
void CommitBoundArgsImpl(const std::tuple<Args...> &args,
                         std::index_sequence<Indexes...>)
{
    (CommitBoundArg(std::get<Indexes>(args)), ...);
}

template <typename... Args>
void CommitBoundArgs(const std::tuple<Args...> &args)
{
    CommitBoundArgsImpl(args, std::index_sequence_for<Args...>{});
}

template <typename Func, typename Tuple, std::size_t... Indexes>
decltype(auto) InvokeTuple(Func &&func, Tuple &&args,
                           std::index_sequence<Indexes...>)
{
    return std::invoke(std::forward<Func>(func),
                       ResolveBoundArg(std::get<Indexes>(std::forward<Tuple>(args)))...);
}

template <typename Func, typename Tuple>
decltype(auto) InvokeTuple(Func &&func, Tuple &&args)
{
    using TupleType = std::remove_reference_t<Tuple>;
    return InvokeTuple(std::forward<Func>(func), std::forward<Tuple>(args),
                       std::make_index_sequence<std::tuple_size_v<TupleType>>{});
}

template <typename Result, bool IsVoid = std::is_void_v<Result>>
struct ResultCache;

template <typename Result>
struct ResultCache<Result, false>
{
    using StoredType = std::remove_cv_t<std::remove_reference_t<Result>>;

    bool hasValue = false;
    std::optional<StoredType> value;

    bool HasValue() const
    {
        return hasValue;
    }

    template <typename U>
    void Store(U &&storedValue)
    {
        value.emplace(std::forward<U>(storedValue));
        hasValue = true;
    }

    StoredType &Get()
    {
        return *value;
    }

    const StoredType &Get() const
    {
        return *value;
    }
};

template <typename Result>
struct ResultCache<Result, true>
{
    bool hasValue = false;

    bool HasValue() const
    {
        return hasValue;
    }

    void Store()
    {
        hasValue = true;
    }
};

} // namespace detail
} // namespace UI
