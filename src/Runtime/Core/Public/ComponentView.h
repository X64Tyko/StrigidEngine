// Field Decomposition Traits - CRTP-based automatic field extraction
#pragma once
#include <tuple>
#include <type_traits>
#include <vector>

// CRTP Base for components that support field decomposition
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

    // Get field info at runtime
    template <size_t Index>
    static constexpr auto GetField()
    {
        return std::get<Index>(Derived::DefineFields());
    }

    // TODO: Roll componentTypeID into the base class
};

// Field metadata
struct FieldMeta
{
    size_t size;
    size_t alignment;
    size_t offsetInStruct; // For debugging/validation
    const char* name; // For debugging
};

// Generate field metadata at compile time
template <typename T, typename FieldType>
constexpr FieldMeta MakeFieldMeta(FieldType T::* member, const char* name)
{
    return FieldMeta{
        sizeof(FieldType),
        alignof(FieldType),
        0, // offsetof doesn't work in constexpr context
        name
    };
}

// Type trait: Check if component supports field decomposition
template <typename T>
concept HasFieldDecomposition = requires
{
    { T::DefineFields() } -> std::same_as<decltype(T::DefineFields())>;
    std::is_base_of_v<SoAComponent<T>, T>;
};

// Runtime helper: Get field count for any component type
template <typename T>
constexpr size_t GetComponentFieldCount()
{
    if constexpr (HasFieldDecomposition<T>)
    {
        return T::GetFieldCount();
    }
    else
    {
        return 1; // Non-decomposable components are treated as single field
    }
}

// Helper: Apply function to each field
template <typename T, typename Func>
void ForEachField(Func&& func)
{
    if constexpr (HasFieldDecomposition<T>)
    {
        constexpr auto fields = T::DefineFields();
        [&]<size_t... Is>(std::index_sequence<Is...>)
        {
            (func(std::get<Is>(fields), Is), ...);
        }(std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{});
    }
    else
    {
        // Non-decomposable component - treat as single unit
        func(nullptr, 0);
    }
}

// Example usage:
// static_assert(HasFieldDecomposition<Transform>);
// static_assert(Transform::GetFieldCount() == 12);
// ForEachField<Transform>([](auto field, size_t index) {
//     // Process each field
// });


#define STRIGID_COMPONENT(Name, ...)                                    \
    struct Name {                                                       \
        struct Data { STRIGID_MEMBERS(__VA_ARGS__) };                   \
        using PtrTuple = std::tuple<STRIGID_PTRS(__VA_ARGS__)>;         \
        struct Shadow {                                                 \
            STRIGID_REFS(__VA_ARGS__)                                   \
            Shadow(STRIGID_CTOR_PARAMS(__VA_ARGS__))                    \
                : STRIGID_CTOR_INIT(__VA_ARGS__) {}                     \
        };                                                              \
    };

// Example expansion for (float, x, float, y):
// Data { float x; float y; };
// PtrTuple = std::tuple<float*, float*>;
// Shadow { float& x; float& y; Shadow(float& x_, float& y_) : x(x_), y(y_) {} };
