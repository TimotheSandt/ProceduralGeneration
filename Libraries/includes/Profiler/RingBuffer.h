#pragma once

#define MIN_CAPACITY 10ULL
#define MAX_CAPACITY 0x8000ULL
#define DEFAULT_CAPACITY 50ULL

template <typename Type>
class RingBuffer 
{
public:
    RingBuffer() {
        this->init(DEFAULT_CAPACITY);
    }

    RingBuffer(size_t capacity)
    {
        this->init(capacity);
    }

    void init(size_t capacity) {

        this->capacity = std::min(std::max(capacity, MIN_CAPACITY), MAX_CAPACITY);
        if (this->buffer != nullptr)
            delete[] this->buffer;
        this->buffer = new Type[this->capacity] { Type(0) };
    };

    void destroy()
    {
        if (this->buffer != nullptr)
            delete[] this->buffer;
    };

    void clear() {
        for (size_t i = 0; i < this->capacity; i++) {
            this->buffer[i] = Type(0);
        }
        this->index = 0;
    }


    void push(Type value)
    {
        this->size = std::min(this->size + 1, this->capacity);
        this->index = (this->index + 1) % this->capacity;
        this->buffer[this->index] = value;
    };

    void resize(size_t size) {
        size = std::min(std::max(size, MIN_CAPACITY), MAX_CAPACITY);
        Type *newBuffer = new Type[size] { Type(0) };
        for (size_t i = 0; i < this->capacity && i < size; i++) {
            newBuffer[std::min(size, this->capacity) - i - 1] = this->get(i);
        }

        if (this->buffer != nullptr)
            delete[] this->buffer;
        this->capacity = size;
        this->size = std::min(this->size, this->capacity); 
        this->buffer = newBuffer;
    }

    size_t getCapacity()
    {
        return this->capacity;
    };

    size_t getSize()
    {
        return this->size;
    };

    Type get(size_t i)
    {
        i = i % this->size;
        return this->buffer[(this->index - i - 1) % this->capacity];
    };


    Type getAverage()
    {
        if (this->size == 0)
            return Type(0);
            
        Type sum = this->buffer[0];
        for (size_t i = 1; i < capacity; i++) {
            sum += this->buffer[i];
        }

        return sum / this->size;
    };

    Type getSum()
    {
        Type sum = this->buffer[0];
        for (size_t i = 1; i < capacity; i++) {
            sum += this->buffer[i];
        }

        return sum;
    };

    Type getMin()
    {
        Type min = this->buffer[0];
        for (size_t i = 1; i < capacity; i++) {
            if (this->buffer[i] < min) {
                min = this->buffer[i];
            }
        }        

        return min;
    };

    Type getMax()
    {
        Type max = this->buffer[0];
        for (size_t i = 1; i < size; i++) {
            if (this->buffer[i] > max) {
                max = this->buffer[i];
            }
        }        

        return max;
    };

private:
    size_t index = 0;
    size_t capacity;
    size_t size = 0;
    Type *buffer = nullptr;
};