#pragma once
#include "Types.h"

class Archetype; // Forward declaration (changed from struct to class)

struct Chunk
{
    static constexpr size_t DATA_SIZE = CHUNK_SIZE;

    alignas(64) uint8_t Data[DATA_SIZE]; // 64-byte alignment for cache line optimization

    inline uint8_t* GetBuffer(uint32_t Offset)
    {
        return Data + Offset;
    }
};
