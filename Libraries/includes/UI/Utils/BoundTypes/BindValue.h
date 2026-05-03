#pragma once

#include <optional>
#include <type_traits>
#include <utility>

namespace UI
{
namespace detail
{

template <typename T>
bool EqualValues(const T &lhs, const T &rhs)
{
    if constexpr (requires { lhs == rhs; })
    {
        return lhs == rhs;
    }
    else if constexpr (requires { lhs != rhs; })
    {
        return !(lhs != rhs);
    }
    else
    {
        return false;
    }
}

} // namespace detail

template <typename T>
class BindValue
{
  public:
    using ValueType = std::remove_cv_t<std::remove_reference_t<T>>;

    BindValue() = default;

    static BindValue Static(ValueType value)
    {
        BindValue boundValue;
        boundValue.ownedValue.emplace(std::move(value));
        return boundValue;
    }

    static BindValue Dynamic(ValueType &value)
    {
        BindValue boundValue;
        boundValue.ownedValue.emplace(value);
        boundValue.referenceValue = &value;
        return boundValue;
    }

    bool IsStatic() const
    {
        return referenceValue == nullptr;
    }

    bool IsDynamic() const
    {
        return referenceValue != nullptr;
    }

    bool IsDirty() const
    {
        return IsDynamic() && (!ownedValue.has_value() ||
                               !detail::EqualValues(*ownedValue, *referenceValue));
    }

    bool isDirty() const
    {
        return IsDirty();
    }

    decltype(auto) apply() &
    {
        return Get();
    }

    decltype(auto) apply() const &
    {
        return Get();
    }

    decltype(auto) apply() &&
    {
        return std::move(*this).Get();
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
        return std::move(*this).apply();
    }

    decltype(auto) Get() &
    {
        return IsDynamic() ? *referenceValue : *ownedValue;
    }

    decltype(auto) Get() const &
    {
        return IsDynamic() ? static_cast<const ValueType &>(*referenceValue)
                           : static_cast<const ValueType &>(*ownedValue);
    }

    decltype(auto) Get() &&
    {
        return IsDynamic() ? static_cast<ValueType &>(*referenceValue)
                           : std::move(*ownedValue);
    }

    decltype(auto) operator()() &
    {
        return apply();
    }

    decltype(auto) operator()() const &
    {
        return apply();
    }

    decltype(auto) operator()() &&
    {
        return std::move(*this).apply();
    }

    void Commit() const
    {
        if (IsDynamic())
            ownedValue.emplace(*referenceValue);
    }

  private:
    mutable std::optional<ValueType> ownedValue;
    ValueType *referenceValue = nullptr;
};

template <typename T>
auto BindStatic(T &&value)
{
    return BindValue<std::remove_cv_t<std::remove_reference_t<T>>>::Static(
        std::forward<T>(value));
}

template <typename T>
auto BindDynamic(T &value)
{
    return BindValue<std::remove_cv_t<std::remove_reference_t<T>>>::Dynamic(value);
}

} // namespace UI
