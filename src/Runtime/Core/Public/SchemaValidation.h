#pragma once
#include <type_traits>

// Schema Validation - Provides better compile-time error messages for common mistakes

namespace SchemaValidation
{
    // Check if a type has a DefineSchema() method
    template <typename T, typename = void>
    struct HasDefineSchema : std::false_type
    {
    };

    template <typename T>
    struct HasDefineSchema<T, std::void_t<decltype(T::DefineSchema())>> : std::true_type
    {
    };

    // Check if a type is a valid component (POD-like, no vtable)
    template <typename T>
    struct IsValidComponent
    {
        static constexpr bool value = std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>;
    };
} // namespace SchemaValidation

// Helpful error message macros with better formatting
#define VALIDATE_ENTITY_HAS_SCHEMA(Type) \
    static_assert(SchemaValidation::HasDefineSchema<Type>::value, \
        "\n\n" \
        "================================================================\n" \
        "ERROR: Entity missing DefineSchema()!\n" \
        "================================================================\n" \
        "\n" \
        "Add this to your entity class:\n" \
        "\n" \
        "    static constexpr auto DefineSchema() {\n" \
        "        return Schema::Create(\n" \
        "            &YourEntity::component1,\n" \
        "            &YourEntity::Update\n" \
        "        );\n" \
        "    }\n" \
        "\n" \
        "================================================================\n")

#define VALIDATE_ENTITY_IS_STANDARD_LAYOUT(Type) \
    static_assert(std::is_standard_layout_v<Type>, \
        "\n\n" \
        "================================================================\n" \
        "ERROR: Entity must be standard layout!\n" \
        "================================================================\n" \
        "\n" \
        "Entity types cannot have:\n" \
        "  - Virtual functions\n" \
        "  - Complex inheritance\n" \
        "\n" \
        "Entities are lightweight data containers.\n" \
        "================================================================\n")

#define VALIDATE_COMPONENT_IS_POD(Type) \
    static_assert(SchemaValidation::IsValidComponent<Type>::value, \
        "\n\n" \
        "================================================================\n" \
        "ERROR: Component must be POD (plain old data)!\n" \
        "================================================================\n" \
        "\n" \
        "Components CANNOT have:\n" \
        "  - Virtual functions\n" \
        "  - Non-trivial constructors/destructors\n" \
        "  - std::string, std::vector, or complex types\n" \
        "  - Heap-allocated pointers\n" \
        "\n" \
        "Use simple structs with raw data only:\n" \
        "\n" \
        "    struct Transform {\n" \
        "        float PositionX, PositionY, PositionZ;\n" \
        "        float RotationX, RotationY, RotationZ;\n" \
        "    };\n" \
        "\n" \
        "================================================================\n")
