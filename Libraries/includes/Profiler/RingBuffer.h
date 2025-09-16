#pragma once

#include <chrono>
#include <type_traits>
#include "Logger.h"

#define MIN_CAPACITY 10ULL
#define MAX_CAPACITY 0x8000ULL
#define DEFAULT_CAPACITY 50ULL

template <typename Type>
class RingBuffer
{
public:
    RingBuffer() noexcept {
        this->Init(DEFAULT_CAPACITY);
    }

    RingBuffer(size_t capacity) noexcept
    {
        this->Init(capacity);
    }

    ~RingBuffer() noexcept {
        this->Destroy();
    }

    void Init(size_t capacity) noexcept {
        capacity = std::min(std::max(capacity, MIN_CAPACITY), MAX_CAPACITY);
        if (this->buffer != nullptr)
            delete[] this->buffer;
        this->buffer = new Type[capacity]();
        this->capacity = capacity;
        this->size = 0;
        this->index = 0;
    }

    void Destroy() noexcept
    {
        if (this->buffer != nullptr) {
            delete[] this->buffer;
            this->buffer = nullptr;
        }
        this->capacity = 0;
        this->size = 0;
        this->index = 0;
    }

    void Clear() noexcept {
        if (this->size == 0) return;
        for (size_t i = 0; i < this->size; ++i) {
            this->buffer[(this->index - i) % this->capacity] = Type();
        }
        this->size = 0;
        this->index = 0;
    }


    void Push(Type value) noexcept
    {
        if (this->size < this->capacity) {
            ++this->size;
        }
        this->index = (this->index + 1) % this->capacity;
        this->buffer[this->index] = value;
    }

    void Resize(size_t newCapacity) noexcept {
        newCapacity = std::min(std::max(newCapacity, MIN_CAPACITY), MAX_CAPACITY);
        if (newCapacity == this->capacity) return;

        Type *newBuffer = new Type[newCapacity]();
        size_t copySize = std::min(this->size, newCapacity);
        for (size_t i = 0; i < copySize; ++i) {
            newBuffer[newCapacity - i - 1] = Get(i);
        }

        delete[] this->buffer;
        this->buffer = newBuffer;
        this->capacity = newCapacity;
        this->size = copySize;
        this->index = (this->capacity - this->size) % this->capacity;
    }

    [[nodiscard]] size_t GetCapacity() const noexcept
    {
        return this->capacity;
    }

    [[nodiscard]] size_t GetSize() const noexcept
    {
        return this->size;
    }

    [[nodiscard]] Type Get(size_t i) const noexcept
    {
        if (this->size == 0) return Type();
        i = i % this->size;
        return this->buffer[(this->index - i + this->capacity) % this->capacity];
    };


    [[nodiscard]] Type GetAverage() const noexcept
    {
        if (this->size == 0) return Type();
        if constexpr (
            std::is_same_v<Type, std::chrono::nanoseconds> || 
            std::is_same_v<Type, std::chrono::microseconds> || 
            std::is_same_v<Type, std::chrono::milliseconds> || 
            std::is_same_v<Type, std::chrono::seconds> || 
            std::is_same_v<Type, std::chrono::minutes> || 
            std::is_same_v<Type, std::chrono::hours>) 
        {
            using rep_t = long long;
            rep_t sum_rep = this->Get(0).count();
            for (size_t i = 1; i < this->size; ++i) {
                sum_rep += this->Get(i).count();
            }
            return Type(sum_rep / static_cast<rep_t>(this->size));
        } else if constexpr (std::is_arithmetic_v<Type>) {
            Type sum = GetSum();
            return sum / static_cast<Type>(this->size);
        }
        LOG_ERROR(-1, "Unsupported type");
        return Type();
    };

    [[nodiscard]] Type GetSum() const noexcept
    {
        if (this->size == 0)
            return Type();
            
        Type sum = this->Get(0);
        for (size_t i = 1; i < this->size; i++) {
            sum += this->Get(i);
        }

        return sum;
    };

    [[nodiscard]] Type GetMin() const noexcept
    {
        if (this->size == 0) return Type();
        Type min = this->Get(0);
        for (size_t i = 1; i < this->size; i++) {
            if (this->Get(i) < min) {
                min = this->Get(i);
            }
        }

        return min;
    };

    [[nodiscard]] Type GetMax() const noexcept
    {
        if (this->size == 0) return Type();
        Type max = this->Get(0);
        for (size_t i = 1; i < this->size; i++) {
            if (this->Get(i) > max) {
                max = this->Get(i);
            }
        }

        return max;
    };

private:
    size_t index = 0;
    size_t capacity = 0;
    size_t size = 0;
    Type *buffer = nullptr;
};