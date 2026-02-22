#pragma once

#include "Registry.h"
#include "FieldMeta.h"

// Global counter (hidden in cpp)
namespace Internal
{
    extern uint32_t g_GlobalComponentCounter;
    extern ClassID g_GlobalClassCounter;
    // TODO: if the user changes the "Generation" bits for the Entity ID and has more than... 2B classes... nvm
}

template <typename T, bool MASK = false>
class EntityView
{
    // Friend all instantiations so masked/unmasked versions can construct each other
    template <typename U, bool M>
    friend class EntityView;

public:
    Registry* Reg = nullptr;
    EntityID ID = {};
    uint32_t ViewIndex = 0;
    using MaskedVersion = EntityView<T, true>;

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
    
    static MaskedVersion GetMasked()
    {
        MaskedVersion view;
        view.Reg = nullptr;
        view.ID = {};
        
        return view;
    }

    __forceinline void Advance(uint32_t step)
    {
        ViewIndex += step;
    }
    
    __forceinline void MaskFinal(int32_t count)
    {
        // what do?
    }

    __forceinline void Hydrate(void** fieldArrayTable, uint32_t index = 0, int32_t count = -1)
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
                    // Check if this is a FieldProxy<T>
                    if constexpr (HasDefineFields<MemberType>)
                    {
                        (static_cast<T*>(this)->*member).Bind(&fieldArrayTable[fieldArrayBaseIndex], index, count);

                        // Advance by number of fields for this component
                        constexpr size_t fieldCount = MemberType::FieldNames.size();
                        fieldArrayBaseIndex += fieldCount;
                    }
                    else
                    {
                        // TODO: support non-FieldProxy<T> members?
                    }
                }
            }(members));
        }, schema.members);
    }
};
