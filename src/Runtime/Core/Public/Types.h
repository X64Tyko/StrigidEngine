#pragma once
#include <bitset>
#include <cstdint>
#include <functional>
#include <cmath>

// Disable MSVC warning for anonymous structs in unions (C++11 standard feature)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
#endif

// Math types
struct Vector3
{
    float x, y, z;

    Vector3() : x(0), y(0), z(0)
    {
    }

    Vector3(float x, float y, float z) : x(x), y(y), z(z)
    {
    }

    Vector3 operator+(const Vector3& other) const { return Vector3(x + other.x, y + other.y, z + other.z); }
    Vector3 operator-(const Vector3& other) const { return Vector3(x - other.x, y - other.y, z - other.z); }
    Vector3 operator*(float scalar) const { return Vector3(x * scalar, y * scalar, z * scalar); }

    float Length() const { return std::sqrt(x * x + y * y + z * z); }

    Vector3 Normalized() const
    {
        float len = Length();
        return len > 0 ? Vector3(x / len, y / len, z / len) : Vector3();
    }
};

struct Matrix4
{
    float m[16]; // Column-major order

    Matrix4()
    {
        for (int i = 0; i < 16; i++) m[i] = 0;
        m[0] = m[5] = m[10] = m[15] = 1.0f; // Identity
    }

    static Matrix4 Identity() { return Matrix4(); }
};

// 16KB Chunks fits perfectly in L1/L2 cache lines
constexpr uint32_t CHUNK_SIZE = 64 * 1024;

// Component type ID - numeric identifier for each component type (0-255)
using ComponentTypeID = uint32_t;

// Component Signature definition as a bitset - tracks which components are present
static constexpr size_t MAX_COMPONENTS = 256;
using ComponentSignature = std::bitset<MAX_COMPONENTS>;
using ClassID = uint16_t;

// Component metadata - describes how a component is laid out in memory
struct ComponentMeta
{
    ComponentTypeID TypeID; // Numeric ID (0-255) for this component type
    size_t Size; // sizeof(Component)
    size_t Alignment; // alignof(Component)
    size_t OffsetInChunk; // Where this component's array starts in the chunk
    bool IsHot = false; // Should this component go in the Hot Sparse set.

    bool operator==(const ComponentMeta& Other) const
    {
        return TypeID == Other.TypeID
            && Size == Other.Size
            && Alignment == Other.Alignment
            && OffsetInChunk == Other.OffsetInChunk
            && IsHot == Other.IsHot;
    }
};

// Global counter (hidden in cpp)
namespace Internal
{
    extern uint32_t g_GlobalComponentCounter;
}

template <typename T>
ComponentTypeID GetComponentTypeID()
{
    // THIS LINE RUNS ONCE PER TYPE (T)
    // The first time you call GetTypeID<Transform>(), it grabs a number.
    // Every subsequent time, it skips this and just returns 'id'.
    static ComponentTypeID id = Internal::g_GlobalComponentCounter++;
    return id;
}

// EntityID - 64-bit smart handle with embedded metadata
// Swappable design: Implement GetIndex(), IsValid(), operator== for custom implementations
union EntityID
{
    uint64_t Value;

    // Bitfield layout
    struct
    {
        uint64_t Index : 20; // 1 Million entities (array slot)
        uint64_t Generation : 16; // 65k recycles (server-grade stability)
        uint64_t TypeID : 12; // 4k class types (function dispatch)
        uint64_t OwnerID : 8; // 256 owners (network routing)
        uint64_t IsStatic : 1; // Static Entity Flag
        uint64_t MetaFlags : 7; // Reserved for future use
    };

    // Required interface (for swappability)
    uint32_t GetIndex() const { return static_cast<uint32_t>(Index); }
    uint16_t GetGeneration() const { return static_cast<uint16_t>(Generation); }
    uint16_t GetTypeID() const { return static_cast<uint16_t>(TypeID); }
    uint8_t GetOwnerID() const { return static_cast<uint8_t>(OwnerID); }
    bool GetIsStatic() const { return IsStatic; }

    bool IsValid() const { return Value != 0; }

    static EntityID Invalid()
    {
        EntityID Id;
        Id.Value = 0;
        return Id;
    }

    // Comparison operators
    bool operator==(const EntityID& Other) const { return Value == Other.Value; }
    bool operator!=(const EntityID& Other) const { return Value != Other.Value; }

    // Network/ownership helpers
    bool IsServer() const { return OwnerID == 0; }
    bool IsLocal(uint8_t LocalClientID) const { return OwnerID == LocalClientID; }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Hash specialization for std::unordered_map
namespace std
{
    template <>
    struct hash<EntityID>
    {
        size_t operator()(const EntityID& Id) const noexcept
        {
            return hash<uint64_t>()(Id.Value);
        }
    };
}

// Hash specialization for std::unordered_set
namespace std
{
    template <>
    struct hash<ComponentMeta>
    {
        size_t operator()(const ComponentMeta& Meta) const noexcept
        {
            return hash<uint64_t>()(Meta.TypeID);
        }
    };
}
