#pragma once
#include <cstdint>

// Helper: Proxy for individual field access
template <typename FieldType>
struct FieldProxy
{
    FieldType* _restrict array;
    uint32_t* _restrict index;

    FieldProxy()
        : array(nullptr), index(nullptr){}

    operator FieldType() const { return array[*index]; }

    FieldProxy& operator=(FieldType value)
    {
        array[*index] = value;
        return *this;
    }

    FieldProxy& operator+=(FieldType value)
    {
        array[*index] += value;
        return *this;
    }

    FieldProxy& operator-=(FieldType value)
    {
        array[*index] -= value;
        return *this;
    }

    FieldProxy& operator*=(FieldType value)
    {
        array[*index] *= value;
        return *this;
    }

    FieldProxy& operator/=(FieldType value)
    {
        array[*index] /= value;
        return *this;
    }

    __forceinline void Bind(void* bindArray, uint32_t* idx)
    {
        array = (FieldType*)bindArray;
        index = idx;
    }
};

// Type trait helper
template <typename T>
struct IsFieldProxy : std::false_type
{
};

template <typename T>
struct IsFieldProxy<FieldProxy<T>> : std::true_type
{
    using ComponentType = T;
};