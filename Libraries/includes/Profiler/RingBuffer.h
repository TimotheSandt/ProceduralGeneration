#pragma once

#define MIN_CAPACITY 10
#define MAX_CAPACITY 0b10000000000
#define DEFAULT_CAPACITY 50

template <typename Type>
class RingBuffer 
{
public:
    RingBuffer() {
        this->Init(DEFAULT_CAPACITY);
    }

    RingBuffer(int capacity)
    {
        this->Init(capacity);
    }

    void Init(int capacity) {

        this->capacity = std::min(std::max(capacity, MIN_CAPACITY), MAX_CAPACITY);
        if (this->buffer != nullptr)
            delete[] this->buffer;
        this->buffer = new Type[this->capacity] { Type(0) };
    };

    void Destroy()
    {
        if (this->buffer != nullptr)
            delete[] this->buffer;
    };


    void push(Type value)
    {
        this->index = (this->index + 1) % this->capacity;
        this->buffer[this->index] = value;
    };

    void resize(int size) {
        size = std::min(std::max(size, MIN_CAPACITY), MAX_CAPACITY);
        Type *newBuffer = new Type[size] { Type(0) };
        for (int i = 0; i < this->capacity && i < size; i++) {
            newBuffer[i] = this->buffer[i];
        }

        if (this->buffer != nullptr)
            delete[] this->buffer;
        this->capacity = size;
        this->buffer = newBuffer;
    }

    int size()
    {
        return this->capacity;
    };

    Type get(int i)
    {
        return this->buffer[(this->index + i - 1) % this->capacity];
    };


    Type getAverage()
    {
        Type sum = this->buffer[0];
        for (int i = 1; i < capacity; i++) {
            sum += this->buffer[i];
        }

        return sum / this->size();
    };

    Type getSum()
    {
        Type sum = this->buffer[0];
        for (int i = 1; i < capacity; i++) {
            sum += this->buffer[i];
        }

        return sum;
    };

    Type getMin()
    {
        Type min = this->buffer[0];
        for (int i = 1; i < capacity; i++) {
            if (this->buffer[i] < min) {
                min = this->buffer[i];
            }
        }        

        return min;
    };

    Type getMax()
    {
        Type max = this->buffer[0];
        for (int i = 1; i < capacity; i++) {
            if (this->buffer[i] > max) {
                max = this->buffer[i];
            }
        }        

        return max;
    };

private:
    int capacity;
    int index = 0;
    Type *buffer = nullptr;
};