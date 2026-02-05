#pragma once
#include <cstdint>

// 16KB Chunks fits perfectly in L1/L2 cache lines
constexpr uint32_t CHUNK_SIZE = 16 * 1024; 

using ComponentTypeID = uint32_t;
using EntityID = uint64_t;