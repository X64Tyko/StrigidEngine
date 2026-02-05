#pragma once
#include "Types.h"

struct Archetype; // Forward declaration

struct Chunk {
    static constexpr size_t DATA_SIZE = CHUNK_SIZE;
    
    alignas(16) uint8_t Data[DATA_SIZE];

    inline uint8_t* GetBuffer(uint32_t offset) {
        return Data + offset;
    }
};