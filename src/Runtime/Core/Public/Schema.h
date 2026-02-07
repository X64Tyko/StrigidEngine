#pragma once
#include <tuple>
#include <Logger.h>

// Function pointer types for lifecycle callbacks
enum class LifecycleType
{
    OnCreate,
    Update,
    FixedUpdate,
    OnDestroy
};

// Type-erased function wrapper for lifecycle callbacks
struct LifecycleFunction
{
    LifecycleType Type;
    void* FunctionPtr;  // Type-erased function pointer

    // Invoke function on entity at given index in component arrays
    void (*Invoker)(void* funcPtr, void** componentArrays, uint32_t index, double dt);
};

class MetaRegistry {
public:
    static MetaRegistry& Get() {
        static MetaRegistry instance; // Thread-safe magic static
        return instance;
    }

    typedef std::tuple<ComponentSignature, std::vector<ComponentMeta>, std::vector<LifecycleFunction>> ComponentDef;
    std::unordered_map<ClassID, ComponentDef> MetaComponents;

    void RegisterPrefab([[maybe_unused]] const char* name)
    {
    }

    void RegisterSystem(const char* name,[[maybe_unused]] void* systemPtr)
    {
        LOG_INFO_F("RegisterSystem %s", name);
    }

    template <typename C, typename T>
    void RegisterPrefabComponent()
    {
        ComponentDef& Meta = MetaComponents[GetClassID<C>()];
        std::get<ComponentSignature>(Meta).set(GetComponentTypeID<T>() - 1);
        std::get<std::vector<ComponentMeta>>(Meta).push_back({GetComponentTypeID<T>(), sizeof(T), alignof(T), 0});
        LOG_INFO_F("RegisterPrefabComponent %s", typeid(T).name());
        LOG_INFO_F("Added %i to class %s ", GetComponentTypeID<T>(), typeid(C).name());
    }

    template <typename C>
    void RegisterLifecycleFunction(LifecycleType type, void* funcPtr, auto invoker)
    {
        ComponentDef& Meta = MetaComponents[GetClassID<C>()];
        std::get<std::vector<LifecycleFunction>>(Meta).push_back({type, funcPtr, invoker});
        LOG_INFO_F("RegisterLifecycleFunction for class %s", typeid(C).name());
    }
};

// The container for member pointers
template <typename... Members>
struct SchemaDefinition {
    std::tuple<Members...> members;

    constexpr SchemaDefinition(Members... m) : members(m...) {}

    // EXTEND: Allows derived classes to append their own members
    template <typename... NewMembers>
    constexpr auto Extend(NewMembers... newMembers) const {
        return std::apply([&](auto... currentMembers) {
            return SchemaDefinition<Members..., NewMembers...>(currentMembers..., newMembers...);
        }, members);
    }
    
    template <typename Target, typename Replacement>
    constexpr auto Replace(Target target, Replacement replacement) const {
        // Unpack the existing tuple...
        return std::apply([&](auto... args) {
            // ...and rebuild a NEW SchemaDefinition
            return SchemaDefinition<decltype(ResolveReplacement(args, target, replacement))...>(
                ResolveReplacement(args, target, replacement)...
            );
        }, members);
    }
    
private:
    
    // Helper: Selects either the 'current' item or the 'replacement'
    // based on whether 'current' matches the 'target' we want to remove.
    template <typename Current, typename Target, typename Replacement>
    static constexpr auto ResolveReplacement(Current current, Target target, Replacement replacement) {
        // 1. Check if types match first (Optimization + Safety)
        if constexpr (std::is_same_v<Current, Target>) {
            // 2. Check value - use ternary to ensure consistent return type
            return (current == target) ? replacement : current;
        } else {
            // Not the droid we are looking for. Keep existing.
            return current;
        }
    }
};

// The static builder interface
struct Schema {
    template <typename... Args>
    static constexpr auto Create(Args... args) {
        return SchemaDefinition<Args...>(args...);
    }
};