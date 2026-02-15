#pragma once
#include <tuple>
#include <functional>
#include <Logger.h>


// Helper concept to detect if T has a specific method
template <typename T> concept HasOnCreate = requires(T t) { t.OnCreate(); };
template <typename T> concept HasOnDestroy = requires(T t) { t.OnDestroy(); };
template <typename T> concept HasUpdate = requires(T t, double dt) { t.Update(dt); };
template <typename T> concept HasPrePhysics = requires(T t, double dt) { t.PrePhysics(dt); };
template <typename T> concept HasPostPhysics = requires(T t, double dt) { t.PostPhysics(dt); };
template <typename T> concept HasOnActivate = requires(T t) { t.OnActivate(); };
template <typename T> concept HasOnDeactivate = requires(T t) { t.OnDeactivate(); };
template <typename T> concept HasOnCollide = requires(T t) { t.OnCollide(); };
template <typename T> concept HasDefineSchema = requires(T t) { t.DefineSchema(); };

class Registry;

using ViewFactory = void(*)(Registry&, EntityID, void* outStorage);
using UpdateFunc = void(*)(double dt, void* storage);
using HydrateFunc = void(*)(void**, uint32_t, void*);
using PhysFunc = void(*)(double, void**, uint32_t, void*);

constexpr size_t MAX_ENTITY_VIEW_SIZE = 128;

struct EntityMeta
{
    ViewFactory GetView = nullptr;
    size_t ViewSize = 0;

    PhysFunc PrePhys = nullptr;
    UpdateFunc PostPhys = nullptr;
    UpdateFunc Update = nullptr;
    HydrateFunc Hydrate = nullptr;
};

class MetaRegistry
{
public:
    static MetaRegistry& Get()
    {
        static MetaRegistry instance; // Thread-safe magic static
        return instance;
    }

    using ComponentDef = std::tuple<ComponentSignature, std::vector<ComponentMeta>>;
    std::unordered_map<ClassID, ComponentDef> ClassToArchetype;
    std::unordered_map<Signature, std::vector<ClassID>> ArchetypeToClass;
    EntityMeta EntityGetters[4096];

    template <typename T>
    void RegisterPrefab()
    {
#ifdef _DEBUG
        if constexpr (sizeof(T) > MAX_ENTITY_VIEW_SIZE)
        {
            LOG_ERROR_F("Entity view size %zu exceeds maximum %zu",
                        sizeof(T), MAX_ENTITY_VIEW_SIZE);
        }
#endif

        const ClassID ID = T::StaticClass();
        EntityGetters[ID].GetView = [](Registry& reg, EntityID id, void* outStorage)
        {
            T* view = new(outStorage) T();
            view->Reg = &reg;
            view->ID = id;
        };
        EntityGetters[ID].ViewSize = sizeof(T);

        if constexpr (HasUpdate<T>)
        {
            EntityGetters[ID].Update = []([[maybe_unused]] double dt, [[maybe_unused]] void* storage)
            {
                T* view = (T*)storage;
                view->Update(dt);
            };
        }

        if constexpr (HasPrePhysics<T>)
        {
            EntityGetters[ID].PrePhys = []([[maybe_unused]] double dt, [[maybe_unused]] void** componentArrays, [[maybe_unused]] uint32_t componentCount, [[maybe_unused]] void* storage)
            {
                // in order to support vectorization, might need to look into _mm256. Creating a Wide version of components
                T* view = (T*)storage;
                for (uint32_t i = 0; i < componentCount; ++i)
                {
                    view->Hydrate(componentArrays, i);
                    view->PrePhysics(dt);
                }
            };
        }

        if constexpr (HasPostPhysics<T>)
        {
            EntityGetters[ID].PostPhys = []([[maybe_unused]] double dt, [[maybe_unused]] void* storage)
            {
                T* view = (T*)storage;
                view->PostPhysics(dt);
            };
        }

        EntityGetters[ID].Hydrate = [](void** componentArrays, uint32_t index, void* storage)
        {
            T* view = (T*)storage;
            view->Hydrate(componentArrays, index);
        };
    }

    template <typename C, typename T>
    void RegisterPrefabComponent()
    {
        const ClassID ID = C::StaticClass();
        ComponentDef& Meta = ClassToArchetype[ID];
        std::get<ComponentSignature>(Meta).set(GetComponentTypeID<T>() - 1);
        std::get<std::vector<ComponentMeta>>(Meta).push_back({GetComponentTypeID<T>(), sizeof(T), alignof(T), 0});
        LOG_INFO_F("RegisterPrefabComponent %s", typeid(T).name());
        LOG_INFO_F("Added %i to class %s ", GetComponentTypeID<T>(), typeid(C).name());
    }
};

// The container for member pointers
template <typename... Members>
struct SchemaDefinition
{
    std::tuple<Members...> members;

    constexpr SchemaDefinition(Members... m) : members(m...)
    {
    }

    // EXTEND: Allows derived classes to append their own members
    template <typename... NewMembers>
    constexpr auto Extend(NewMembers... newMembers) const
    {
        return std::apply([&](auto... currentMembers)
        {
            return SchemaDefinition<Members..., NewMembers...>(currentMembers..., newMembers...);
        }, members);
    }

    template <typename Target, typename Replacement>
    constexpr auto Replace(Target target, Replacement replacement) const
    {
        // Unpack the existing tuple...
        return std::apply([&](auto... args)
        {
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
    static constexpr auto ResolveReplacement(Current current, Target target, Replacement replacement)
    {
        // 1. Check if types match first (Optimization + Safety)
        if constexpr (std::is_same_v<Current, Target>)
        {
            // 2. Check value - use ternary to ensure consistent return type
            return (current == target) ? replacement : current;
        }
        else
        {
            // Not the droid we are looking for. Keep existing.
            return current;
        }
    }
};

// The static builder interface
struct Schema
{
    template <typename... Args>
    static constexpr auto Create(Args... args)
    {
        return SchemaDefinition<Args...>(args...);
    }
};
