#pragma once

#include "Profiler.h"
#include "Registry.h"
#include "SchemaReflector.h"

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

    static ClassID StaticClass()
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

    void Hydrate(Registry& reg, EntityID id, void** componentArrays, uint32_t index)
    {
        STRIGID_ZONE_FINE_N("Bind_Components");
        // Rebind Ref<T> members from component arrays (reuse cached entity)
        constexpr auto schema = T::DefineSchema();
        BindComponentsFromArrays(this, componentArrays, index, schema.members,
                                 std::make_index_sequence<std::tuple_size_v<decltype(schema.members)>>{});
    }

    void Hydrate(void* componentArrays[MAX_COMPONENTS], uint32_t CompIndex)
    {
        constexpr auto schema = T::DefineSchema();
        std::apply([&](auto&&... members)
        {
            (..., [&]<typename T0>(T0 member)
            {
                if constexpr (std::is_member_object_pointer_v<T0>)
                {
                    using CompType = typename StripRef<T0>::Type;

                    CompType* TypeArr = static_cast<CompType*>(componentArrays[GetComponentTypeID<CompType>()]);

                    if (TypeArr)
                    {
                        static_cast<T* const>(this)->*member = TypeArr + CompIndex;
                    }
                }
            }(members));
        }, schema.members);
    }
};
