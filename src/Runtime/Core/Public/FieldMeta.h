#pragma once
#include <cstddef>
#include <vector>
#include "Types.h"

// Field metadata for SoA decomposition
struct FieldMeta
{
    size_t Size; // sizeof(field) - e.g., sizeof(float) = 4
    size_t Alignment; // alignof(field) - e.g., alignof(float) = 4
    size_t OffsetInStruct; // offsetof(Component, field) - for validation
    size_t OffsetInChunk; // Where this field array starts in the chunk (computed by BuildLayout)
    const char* Name; // Field name for debugging
};

// Enhanced component metadata with field decomposition support
struct ComponentMetaEx
{
    ComponentTypeID TypeID; // Numeric ID (0-255) for this component type
    size_t Size; // sizeof(Component) - total struct size
    size_t Alignment; // alignof(Component)
    size_t OffsetInChunk; // Where this component's data starts in the chunk
    bool IsFieldDecomposed; // True if stored as field arrays (SoA)
    std::vector<FieldMeta> Fields; // Field layout if decomposed
};

// Component field registry - static storage for field decomposition info
// TODO: Should this stay separate or should it be roleld into the MetaRegistry?
// Also need to double check how the .data and compile times are looking with all this "Reflection"
class ComponentFieldRegistry
{
public:
    static ComponentFieldRegistry& Get()
    {
        static ComponentFieldRegistry instance;
        return instance;
    }

    // Register field decomposition for a component type
    void RegisterFields(ComponentTypeID typeID, std::vector<FieldMeta>&& fields)
    {
        FieldData[typeID] = std::move(fields);
    }

    // Get field layout for a component
    const std::vector<FieldMeta>* GetFields(ComponentTypeID typeID) const
    {
        auto it = FieldData.find(typeID);
        return it != FieldData.end() ? &it->second : nullptr;
    }

    // Check if component has field decomposition
    bool IsDecomposed(ComponentTypeID typeID) const
    {
        return FieldData.contains(typeID);
    }

    // Get field count
    size_t GetFieldCount(ComponentTypeID typeID) const
    {
        auto it = FieldData.find(typeID);
        return it != FieldData.end() ? it->second.size() : 0;
    }

private:
    std::unordered_map<ComponentTypeID, std::vector<FieldMeta>> FieldData;
};
