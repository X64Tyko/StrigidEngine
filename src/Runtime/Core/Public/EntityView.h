#pragma once

#include "Profiler.h"
#include "Registry.h"
#include "SchemaReflector.h"
#include "SoARef.h"
#include "FieldMeta.h"

// Global counter (hidden in cpp)
namespace Internal
{
    extern uint32_t g_GlobalComponentCounter;
    extern ClassID g_GlobalClassCounter;
    // TODO: if the user changes the "Generation" bits for the Entity ID and has more than... 2B classes... nvm
}

template <typename T>
class EntityView
{
public:
    Registry* Reg = nullptr;
    EntityID ID = {};

    static ClassID StaticClassID()
    {
        static ClassID id = Internal::g_GlobalClassCounter++;
        return id;
    }

    static constexpr auto DefineSchema()
    {
        return Schema::Create();
    }

protected:
    EntityView() = default;

public:
    // Delete Copy/Move to prevent "Orphaned Views"
    EntityView(const EntityView&) = delete;
    EntityView& operator=(const EntityView&) = delete;

    static T Get(Registry& reg, EntityID id)
    {
        T view;
        view.Reg = &reg;
        view.ID = id;

        return view;
    }

    __forceinline void Advance(uint32_t StepSize)
    {
        constexpr auto schema = T::DefineSchema();

        std::apply([&](auto&&... members)
        {
            // Correct fold expression: call lambda with each member
            (void)((
                [&](auto member)
                {
                    if constexpr (std::is_member_object_pointer_v<decltype(member)>)
                    {
                        using MemberType = std::remove_reference_t<decltype(static_cast<T*>(this)->*member)>;

                        if constexpr (IsSoARef<MemberType>::value)
                        {
                            (static_cast<T*>(this)->*member).index += StepSize;
                        }
                    }
                }(members), ...
            ), 0);
        }, schema.members);
    }

    __forceinline void Hydrate(void** fieldArrayTable, uint32_t index)
    {
        constexpr auto schema = T::DefineSchema();

        size_t fieldArrayBaseIndex = 0;

        std::apply([&](auto&&... members)
        {
            (..., [&](auto member)
            {
                if constexpr (std::is_member_object_pointer_v<decltype(member)>)
                {
                    using MemberType = std::remove_reference_t<decltype(static_cast<T*>(this)->*member)>;

                    // Check if this is a SoARef
                    if constexpr (IsSoARef<MemberType>::value)
                    {
                        using ComponentType = MemberType::ComponentType;

                        // Bind SoARef to field arrays
                        (static_cast<T*>(this)->*member).Bind(
                            &fieldArrayTable[fieldArrayBaseIndex],
                            index
                        );

                        // Advance by number of fields for this component
                        fieldArrayBaseIndex += ComponentFieldRegistry::Get()
                            .GetFieldCount(GetComponentTypeID<ComponentType>());
                    }
                    else
                    {
                        // Legacy Ref<T> support (non-decomposed components)
                        using CompType = std::remove_pointer_t<decltype((static_cast<T*>(this)->*member).ptr)>;

                        CompType* TypeArr = static_cast<CompType*>(
                            fieldArrayTable[fieldArrayBaseIndex]
                        );

                        if (TypeArr)
                        {
                            (static_cast<T*>(this)->*member) = TypeArr + index;
                        }

                        fieldArrayBaseIndex += 1; // Non-decomposed = 1 array
                    }
                }
            }(members));
        }, schema.members);
    }
};
