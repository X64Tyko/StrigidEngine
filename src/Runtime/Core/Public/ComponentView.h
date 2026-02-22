#pragma once
#include "FieldProxy.h"

template<typename Derived, bool MASK = false>
struct ComponentView
{
    using FloatProxy = FieldProxy<float, MASK>;
    using IntProxy = FieldProxy<int32_t, MASK>;
    using UIntProxy = FieldProxy<uint32_t, MASK>;
    using Int64Proxy = FieldProxy<int64_t, MASK>;
    using UInt64Proxy = FieldProxy<uint64_t, MASK>;
};
