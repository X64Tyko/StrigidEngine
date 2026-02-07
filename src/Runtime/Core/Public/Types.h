#pragma once
#include <bitset>
#include <cstdint>
#include <functional>

// Disable MSVC warning for anonymous structs in unions (C++11 standard feature)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
#endif

// 16KB Chunks fits perfectly in L1/L2 cache lines
constexpr uint32_t CHUNK_SIZE = 16 * 1024;

// Component type ID - numeric identifier for each component type (0-255)
using ComponentTypeID = uint32_t;

// Component Signature definition as a bitset - tracks which components are present
static constexpr size_t MAX_COMPONENTS = 256;
typedef std::bitset<MAX_COMPONENTS> ComponentSignature;
typedef uint16_t ClassID;

// Component metadata - describes how a component is laid out in memory
struct ComponentMeta
{
    ComponentTypeID TypeID;  // Numeric ID (0-255) for this component type
    size_t Size;           // sizeof(Component)
    size_t Alignment;      // alignof(Component)
    size_t OffsetInChunk;  // Where this component's array starts in the chunk
};

// Global counter (hidden in cpp)
namespace Internal {
    extern uint32_t g_GlobalComponentCounter;
    extern ClassID g_GlobalClassCounter; // TODO: if the user changes the "Generation" bits for the Entity ID and has more than... 2B classes... nvm
}

template <typename T>
ComponentTypeID GetComponentTypeID() {
    // THIS LINE RUNS ONCE PER TYPE (T)
    // The first time you call GetTypeID<Transform>(), it grabs a number.
    // Every subsequent time, it skips this and just returns 'id'.
    static ComponentTypeID id = Internal::g_GlobalComponentCounter++;
    return id;
}

template <typename T>
ClassID GetClassID() {
    // THIS LINE RUNS ONCE PER TYPE (T)
    // The first time you call GetTypeID<Transform>(), it grabs a number.
    // Every subsequent time, it skips this and just returns 'id'.
    static ClassID id = Internal::g_GlobalClassCounter++;
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
        uint64_t Index      : 20; // 1 Million entities (array slot)
        uint64_t Generation : 16; // 65k recycles (server-grade stability)
        uint64_t TypeID     : 12; // 4k class types (function dispatch)
        uint64_t OwnerID    : 8;  // 256 owners (network routing)
        uint64_t MetaFlags  : 8;  // Reserved for future use
    };

    // Required interface (for swappability)
    inline uint32_t GetIndex() const { return static_cast<uint32_t>(Index); }
    inline uint16_t GetGeneration() const { return static_cast<uint16_t>(Generation); }
    inline uint16_t GetTypeID() const { return static_cast<uint16_t>(TypeID); }
    inline uint8_t GetOwnerID() const { return static_cast<uint8_t>(OwnerID); }

    inline bool IsValid() const { return Value != 0; }
    static EntityID Invalid() { EntityID Id; Id.Value = 0; return Id; }

    // Comparison operators
    inline bool operator==(const EntityID& Other) const { return Value == Other.Value; }
    inline bool operator!=(const EntityID& Other) const { return Value != Other.Value; }

    // Network/ownership helpers
    inline bool IsServer() const { return OwnerID == 0; }
    inline bool IsLocal(uint8_t LocalClientID) const { return OwnerID == LocalClientID; }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Hash specialization for std::unordered_map
namespace std
{
    template<>
    struct hash<EntityID>
    {
        size_t operator()(const EntityID& Id) const noexcept
        {
            return hash<uint64_t>()(Id.Value);
        }
    };
}
