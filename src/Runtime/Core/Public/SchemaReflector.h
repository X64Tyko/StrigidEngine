#pragma once
#include "Registry.h"
#include "Ref.h"
#include "Types.h"
#include "Schema.h"
#include "SchemaValidation.h"
#include <bitset>
#include <iostream>
#include <tuple>
#include <utility>
#include <type_traits>

// --- TYPE TRAITS ---
// Strips Ref<T> down to T
template <typename T> struct StripRef;
template <typename T> struct StripRef<Ref<T>> { using Type = T; };

// Strips Ref<T> Class::* down to T
template <typename C, typename M> struct StripRef<M C::*> { using Type = typename StripRef<M>::Type; };

// Helper to check if T is a member function pointer
template<typename T>
struct IsMemberFunction : std::false_type {};

template<typename C, typename Ret, typename... Args>
struct IsMemberFunction<Ret(C::*)(Args...)> : std::true_type {};

template <typename Class>
struct PrefabReflector {

    // --- 1. REGISTRATION (Static Init) ---
    static bool Register(const char* name) {
        // Compile-time validation of entity type
        VALIDATE_ENTITY_HAS_SCHEMA(Class);
        VALIDATE_ENTITY_IS_STANDARD_LAYOUT(Class);

        MetaRegistry::Get().RegisterPrefab(name);

        constexpr auto schema = Class::DefineSchema();
        
        // Register Components by iterating the tuple
        std::apply([](auto... args) {
            (ProcessSchemaItem(args), ...);
        }, schema.members);

        // Register the System Loop
        //MetaRegistry::Get().RegisterSystem(name, &SystemLoop);
        
        return true;
    }

    template <typename T>
    static void ProcessSchemaItem(T Class::* memberPtr)
    {
        // Register component members (Ref<T>)
        if constexpr (std::is_member_object_pointer_v<decltype(memberPtr)>)
        {
            using CompType = typename StripRef<decltype(memberPtr)>::Type;

            // Compile-time validation of component type
            VALIDATE_COMPONENT_IS_POD(CompType);

            MetaRegistry::Get().RegisterPrefabComponent<Class, CompType>();
        }
        // Register lifecycle functions
        else if constexpr (std::is_member_function_pointer_v<decltype(memberPtr)>)
        {
            RegisterLifecycleFunction<Class>(memberPtr);
        }
    }

    // Helper to register lifecycle functions by detecting their signature
    template <typename C, typename Ret, typename... Args>
    static void RegisterLifecycleFunction(Ret(C::*funcPtr)(Args...))
    {
        // Detect lifecycle type by signature
        LifecycleType type;
        constexpr bool hasDoubleParam = (sizeof...(Args) == 1) && (std::is_same_v<double, Args> && ...);
        constexpr bool hasNoParams = (sizeof...(Args) == 0);

        if constexpr (hasNoParams)
        {
            // OnCreate() or OnDestroy() - we'll detect by name later if needed
            // For now, assume OnCreate
            type = LifecycleType::OnCreate;
        }
        else if constexpr (hasDoubleParam)
        {
            // Update(double) or FixedUpdate(double)
            // Default to Update for now - we can extend this later
            type = LifecycleType::Update;
        }
        else
        {
            return; // Unknown signature, skip
        }

        // Create an invoker that uses a cached entity instance passed as parameter
        auto invoker = [](void* funcPtr, void* cachedEntityPtr, void** componentArrays, uint32_t index, double dt)
        {
            STRIGID_ZONE_FINE_N("Lifecycle_Invoker"); // Level 3: Per-entity profiling

            // Reconstruct the function pointer
            Ret(C::*func)(Args...);
            std::memcpy(&func, &funcPtr, sizeof(funcPtr));

            // Cast cached entity back to correct type
            C* entity = static_cast<C*>(cachedEntityPtr);

            {
                STRIGID_ZONE_FINE_N("Bind_Components"); // Level 3: Per-entity profiling
                // Rebind Ref<T> members from component arrays (reuse cached entity)
                constexpr auto schema = C::DefineSchema();
                BindComponentsFromArrays(*entity, componentArrays, index, schema.members,
                                         std::make_index_sequence<std::tuple_size_v<decltype(schema.members)>>{});
            }

            {
                STRIGID_ZONE_FINE_N("Entity_Update"); // Level 3: Per-entity profiling
                // Call the member function
                if constexpr (sizeof...(Args) == 0)
                {
                    ((*entity).*func)();
                }
                else if constexpr (sizeof...(Args) == 1)
                {
                    ((*entity).*func)(dt);
                }
            }
        };

        // Allocate cached entity on heap (persists for lifetime of program)
        C* cachedEntity = new C{};

        // Store the function pointer as void*
        void* voidFuncPtr;
        static_assert(sizeof(funcPtr) <= sizeof(voidFuncPtr), "Function pointer too large");
        std::memcpy(&voidFuncPtr, &funcPtr, sizeof(funcPtr));

        MetaRegistry::Get().RegisterLifecycleFunction<C>(type, voidFuncPtr, cachedEntity, invoker);
    }

    // Helper to bind component arrays to entity Ref<T> members
    template <typename SchemaTuple, size_t... Is>
    static void BindComponentsFromArrays(Class& entity, void** componentArrays, uint32_t index,
                                        const SchemaTuple& schemaMembers, std::index_sequence<Is...>)
    {
        // For each member in the schema, check if it's a component reference
        (BindSingleComponent<Is>(entity, componentArrays, index, schemaMembers), ...);
    }

    template <size_t I, typename SchemaTuple>
    static void BindSingleComponent(Class& entity, void** componentArrays, uint32_t index, const SchemaTuple& schemaMembers)
    {
        auto member = std::get<I>(schemaMembers);

        // Only bind if it's a member object pointer (component reference)
        if constexpr (std::is_member_object_pointer_v<decltype(member)>)
        {
            using CompType = typename StripRef<decltype(member)>::Type;

            // Get the component array and bind to entity
            CompType* array = static_cast<CompType*>(componentArrays[I]);
            entity.*member = &array[index];
        }
    }

    /*
    // --- 2. RUNTIME SYSTEM ---
    static void SystemLoop(void* registryPtr, double dt) {
        Registry& reg = *static_cast<Registry*>(registryPtr);
        constexpr auto schema = Class::DefineSchema();

        // Unpack tuple to parameter pack
        std::apply([&](auto... args) {
            RunQueryImpl(reg, dt, schema.members, args...);
        }, schema.members);
    }

    // The Hot Loop Generator
    template <typename... MemberPtrs>
    static void RunQueryImpl(Registry& reg, double dt, const auto& schemaTuple, MemberPtrs... ptrs) {
        
        // Query Registry using deduced types
        auto view = reg.Query<typename StripRef<decltype(ptrs)>::Type...>();

        for (auto* chunk : view.chunks) {
            if (chunk->count == 0) continue;

            // Get pointers to the SoA arrays
            auto arrays = std::make_tuple(
                chunk->GetArray<typename StripRef<decltype(ptrs)>::Type>()...
            );

            // Iterate Entities
            for (size_t i = 0; i < chunk->count; ++i) {
                Class instance; 

                // Bind Members: instance.*Ptr = &array[i]
                BindMembers(instance, i, schemaTuple, arrays, 
                           std::make_index_sequence<sizeof...(MemberPtrs)>{});

                // Run Update
                instance.Update(dt);
            }
        }
    }
*/
    template <typename SchemaTuple, typename ArrayTuple, size_t... Is>
    static void BindMembers(Class& instance, size_t index, const SchemaTuple& schemaMembers, const ArrayTuple& arrays, std::index_sequence<Is...>) {
        // Aggregate initialization of Ref<T> with the pointer
        ((instance.*(std::get<Is>(schemaMembers)) = { &std::get<Is>(arrays)[index] }), ...);
    }
};

// --- MACRO ---
#define STRIGID_REGISTER_ENTITY(CLASS) \
    static const bool g_Reflect_##CLASS = PrefabReflector<CLASS>::Register(#CLASS);