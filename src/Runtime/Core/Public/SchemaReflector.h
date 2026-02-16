#pragma once
#include "Ref.h"
#include "Schema.h"
#include "SchemaValidation.h"
#include <bitset>
#include <tuple>
#include <type_traits>

// --- TYPE TRAITS ---
// Strips Ref<T> down to T
template <typename T>
struct StripRef;

template <typename T>
struct StripRef<SoARef<T>>
{
    using Type = T;
};

// Strips Ref<T> Class::* down to T
template <typename C, typename M>
struct StripRef<M C::*>
{
    using Type = StripRef<M>::Type;
};

template <typename Class>
struct PrefabReflector
{
    // --- 1. REGISTRATION (Static Init) ---
    static bool Register()
    {
        // Compile-time validation of entity type
        VALIDATE_ENTITY_HAS_SCHEMA(Class);
        //VALIDATE_ENTITY_IS_STANDARD_LAYOUT(Class);

        MetaRegistry::Get().RegisterPrefab<Class>();

        constexpr auto schema = Class::DefineSchema();

        // Register Components by iterating the tuple
        std::apply([](auto... args)
        {
            (ProcessSchemaItem(args), ...);
        }, schema.members);

        return true;
    }

    template <typename T>
    static void ProcessSchemaItem(T Class::* memberPtr)
    {
        // Register component members (Ref<T>)
        if constexpr (std::is_member_object_pointer_v<decltype(memberPtr)>)
        {
            using CompType = StripRef<decltype(memberPtr)>::Type;

            // Compile-time validation of component type
            VALIDATE_COMPONENT_IS_POD(CompType);

            MetaRegistry::Get().RegisterPrefabComponent<Class, CompType>();
        }
    }
};

// --- MACRO ---
#define STRIGID_REGISTER_ENTITY(CLASS) \
    class CLASS; \
    namespace { \
        static const bool g_Reflect_##CLASS = []() { \
            PrefabReflector<CLASS>::Register(); \
            return true; \
        }(); \
    }
