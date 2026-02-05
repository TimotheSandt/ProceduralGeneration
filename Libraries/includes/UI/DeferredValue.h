#pragma once

#include <optional>
#include <memory>
#include <functional>

namespace DeferredValueDetail {
    // Generic equality check: uses operator== if available, otherwise returns false (always update)
    template<typename T>
    bool IsEqual(const T& a, const T& b) {
        if constexpr (requires { a == b; }) {
            return a == b;
        } else {
            return false;
        }
    }

    // Optimization for std::weak_ptr which has equality semantics but no operator==
    template<typename T>
    bool IsEqual(const std::weak_ptr<T>& a, const std::weak_ptr<T>& b) {
        return !a.owner_before(b) && !b.owner_before(a);
    }
}

template<typename T>
class DeferredValue {
public:
    DeferredValue() = default;
    DeferredValue(T initialValue) : value(initialValue), newValue(initialValue) {}

    const T& Get() const { return value; }

    void Set(T v) {
        if (!DeferredValueDetail::IsEqual(value, v)) {
            newValue = v;
        }
    }

    bool ForceSet(T v) {
        bool hasChanged = !DeferredValueDetail::IsEqual(value, v);
        value = v;
        return hasChanged;
    }

    template<typename Func>
    void Modify(Func func) {
        T temp = value;
        func(temp);
        Set(temp);
    }

    template<typename Func>
    void ModifyForce(Func func) {
        T temp = value;
        func(temp);
        ForceSet(temp);
    }

    bool Apply() {
        if (newValue.has_value()) {
            value = newValue.value();
            newValue.reset();
            return true;
        }
        return false;
    }
    bool HasNewValue() const { return newValue.has_value(); }

    bool operator==(const DeferredValue<T>& other) const = delete;
    bool operator!=(const DeferredValue<T>& other) const = delete;
    DeferredValue<T>& operator=(T) = delete;
    DeferredValue<T>& operator=(const T&) = delete;
    DeferredValue<T>& operator=(T&&) = delete;
    DeferredValue<T>& operator=(const T&&) = delete;


private:
    T value;
    std::optional<T> newValue;
};
