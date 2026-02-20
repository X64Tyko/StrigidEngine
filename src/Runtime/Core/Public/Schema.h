#pragma once
#include <functional>
#include <Logger.h>
#include <tuple>
#include <unordered_set>

#include "Profiler.h"
#include "Signature.h"
#include "Types.h"


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
template <typename T> concept HasDefineFields = requires(T t) { t.DefineFields(); };

using UpdateFunc = void(*)(double, void**, uint32_t);

#define REGISTER_ENTITY_PREPHYS(Type, ClassID) \
    case ClassID: InvokePrePhysicsImpl<Type>(dt, fieldArrayTable, componentCount); break;

struct EntityMeta
{
    size_t ViewSize = 0;

    UpdateFunc PrePhys = nullptr;
    UpdateFunc PostPhys = nullptr;
    UpdateFunc Update = nullptr;

    EntityMeta(){}
    EntityMeta(const size_t inViewSize, const UpdateFunc prePhys, const UpdateFunc postPhys, const UpdateFunc update)
        : ViewSize(inViewSize)
        , PrePhys(prePhys)
        , PostPhys(postPhys)
        , Update(update)
    {}

    EntityMeta(const EntityMeta& rhs)
        : ViewSize(rhs.ViewSize)
        , PrePhys(rhs.PrePhys)
        , PostPhys(rhs.PostPhys)
        , Update(rhs.Update)
    {}

    EntityMeta& operator=(EntityMeta&& rhs) noexcept
    {
        std::memcpy(this, &rhs, sizeof(EntityMeta));
        return *this;
    }
};


template <typename T>
__forceinline void InvokePrePhysicsImpl(double dt, void** fieldArrayTable, uint32_t componentCount)
{
    alignas(16) T EntityView;
    EntityView.Hydrate(fieldArrayTable, 0);

#pragma loop(ivdep)
    for (uint32_t i = 0; i < componentCount; ++i)
    {
        EntityView.PrePhysics(dt);
        EntityView.Advance(1);
    }
}

template <typename T>
__forceinline void InvokePostPhysicsImpl(double dt, void** fieldArrayTable, uint32_t componentCount)
{
    alignas(16) T EntityView;
    EntityView.Hydrate(fieldArrayTable, 0);

#pragma loop(ivdep)
    for (uint32_t i = 0; i < componentCount; ++i)
    {
        EntityView.PostPhysics(dt);
        EntityView.Advance(1);
    }
}

template <typename T>
__forceinline void InvokeUpdateImpl(double dt, void** fieldArrayTable, uint32_t componentCount)
{
    alignas(16) T EntityView;
    EntityView.Hydrate(fieldArrayTable, 0);

#pragma loop(ivdep)
    for (uint32_t i = 0; i < componentCount; ++i)
    {
        EntityView.Update(dt);
        EntityView.Advance(1);
    }
}

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
    std::unordered_set<ComponentMeta> ReflectedComponents;
    EntityMeta EntityGetters[4096];

    template <typename T>
    void RegisterPrefab()
    {
        const ClassID ID = T::StaticClassID();
        EntityGetters[ID].ViewSize = sizeof(T);

        if constexpr (HasUpdate<T>)
        {
            // Then in RegisterEntity:
            EntityGetters[ID].Update = InvokeUpdateImpl<T>;
        }

        if constexpr (HasPrePhysics<T>)
        {
            // Then in RegisterEntity:
            EntityGetters[ID].PrePhys = InvokePrePhysicsImpl<T>;
        }

        if constexpr (HasPostPhysics<T>)
        {
            // Then in RegisterEntity:
            EntityGetters[ID].PostPhys = InvokePostPhysicsImpl<T>;
        }
    }

    template <typename C, typename T>
    void RegisterPrefabComponent()
    {
        bool bIsHot = false;
        if constexpr (requires { T::bHotComp; })
        {
            bIsHot = T::bHotComp;
        }

        const ClassID ID = C::StaticClassID();
        ComponentDef& Def = ClassToArchetype[ID];
        std::get<ComponentSignature>(Def).set(GetComponentTypeID<T>() - 1);
        ComponentMeta Meta = {GetComponentTypeID<T>(), sizeof(T), alignof(T), 0, bIsHot};
        ReflectedComponents.insert(Meta);
        std::get<std::vector<ComponentMeta>>(Def).push_back(Meta);
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
