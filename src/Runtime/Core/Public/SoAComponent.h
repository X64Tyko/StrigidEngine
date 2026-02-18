#pragma once
#include "FieldMeta.h"
#include <tuple>
#include <array>

// CRTP Base for components that support automatic field decomposition
// All components should inherit from this to get SoA layout
template <typename Derived>
struct SoAComponent
{
    // Get compile-time field list
    static constexpr auto GetFieldPointers()
    {
        return Derived::DefineFields();
    }

    // Get runtime field count
    static constexpr size_t GetFieldCount()
    {
        return std::tuple_size_v<decltype(Derived::DefineFields())>;
    }

    // Static registration - called once during static initialization
    static bool RegisterFieldsStatic()
    {
        RegisterFieldsImpl(std::make_index_sequence<GetFieldCount()>{});
        return true;
    }

private:
    // Helper to register fields at runtime
    template <size_t... Is>
    static void RegisterFieldsImpl(std::index_sequence<Is...>)
    {
        constexpr auto fields = Derived::DefineFields();
        std::vector<FieldMeta> fieldMetas;
        fieldMetas.reserve(sizeof...(Is));

        // Extract field metadata for each member pointer
        (..., fieldMetas.push_back(ExtractFieldMeta<Is>(std::get<Is>(fields))));

        // Register with global registry
        ComponentTypeID typeID = GetComponentTypeID<Derived>();
        ComponentFieldRegistry::Get().RegisterFields(typeID, std::move(fieldMetas));
    }

    // Extract metadata from a member pointer
    template <size_t Index, typename FieldType>
    static FieldMeta ExtractFieldMeta(FieldType Derived::* member)
    {
        // Create temporary to get offset
        Derived temp{};
        size_t offset = reinterpret_cast<size_t>(&(temp.*member)) - reinterpret_cast<size_t>(&temp);

        const char* name = (Index < Derived::FieldNames.size()) ? Derived::FieldNames[Index] : "unknown";

        return FieldMeta{
            sizeof(FieldType),
            alignof(FieldType),
            offset,
            0, // OffsetInChunk computed later by Archetype::BuildLayout
            name
        };
    }
};

// Type trait: Check if component supports field decomposition
template <typename T>
concept HasFieldDecomposition = requires
{
    { T::DefineFields() } -> std::same_as<decltype(T::DefineFields())>;
    requires std::is_base_of_v<SoAComponent<T>, T>;
};

// Macro to auto-register fields during static initialization
#define STRIGID_REGISTER_COMPONENT_FIELDS(ComponentType) \
    namespace { \
        static bool _##ComponentType##_FieldsRegistered = ComponentType::RegisterFieldsStatic(); \
    }

#define STRIGID_HOT_COMPONENT() \
    alignas(4) static inline bool bHotComp = true;
