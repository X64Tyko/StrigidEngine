#pragma once
#include <cstdint>

// Helper: Proxy for individual field access
template <typename FieldType>
struct FieldProxy
{
    FieldType* array;
    uint32_t index;

    FieldProxy(FieldType* arr, uint32_t idx) : array(arr), index(idx)
    {
    }

    operator FieldType() const { return array[index]; }

    FieldProxy& operator=(FieldType value)
    {
        array[index] = value;
        return *this;
    }

    FieldProxy& operator+=(FieldType value)
    {
        array[index] += value;
        return *this;
    }

    FieldProxy& operator-=(FieldType value)
    {
        array[index] -= value;
        return *this;
    }

    FieldProxy& operator*=(FieldType value)
    {
        array[index] *= value;
        return *this;
    }

    FieldProxy& operator/=(FieldType value)
    {
        array[index] /= value;
        return *this;
    }

    FieldType* operator&() { return &array[index]; }
};

// Generic base template
template <typename T>
struct SoARef
{
    void** fieldArrays = nullptr;
    uint32_t index = 0;

    void Bind(void** arrays, uint32_t idx)
    {
        fieldArrays = arrays;
        index = idx;
    }

    operator bool() const { return fieldArrays != nullptr; }
};

/* Specialoizations follow, using this to test out ideas, not ideal use case
 * if each component has to define it's SoARef specification.
 */

// Forward declare components (so they can be used before their headers are included)
struct Transform;
struct Velocity;
struct ColorData;

// ===== Transform Specialization =====
// Auto-generate from Transform::DefineFields() using X-Macros

// X-Macro: List each field with (name, type, index)
#define TRANSFORM_FIELDS(X) \
    X(PositionX, float, 0) \
    X(PositionY, float, 1) \
    X(PositionZ, float, 2) \
    X(RotationX, float, 4) \
    X(RotationY, float, 5) \
    X(RotationZ, float, 6) \
    X(ScaleX, float, 8) \
    X(ScaleY, float, 9) \
    X(ScaleZ, float, 10)

template <>
struct SoARef<Transform>
{
    using ComponentType = Transform;

    void** fieldArrays = nullptr;
    uint32_t index = 0;

    void Bind(void** arrays, uint32_t idx)
    {
        fieldArrays = arrays;
        index = idx;
    }

    operator bool() const { return fieldArrays != nullptr; }

    struct Accessor
    {
        void** fieldArrays;
        uint32_t index;

        // Generate FieldProxy members for each field
#define GENERATE_FIELD(name, type, idx) FieldProxy<type> name;
        TRANSFORM_FIELDS(GENERATE_FIELD)
#undef GENERATE_FIELD

        // Constructor initializes all FieldProxies
        Accessor(void** arrays, uint32_t idx)
            : fieldArrays(arrays), index(idx)
#define GENERATE_INIT(name, type, fieldIdx) , name((type*)arrays[fieldIdx], idx)
              TRANSFORM_FIELDS(GENERATE_INIT)
#undef GENERATE_INIT
        {
        }

        Accessor* operator->() { return this; }
    };

    Accessor operator->()
    {
        return Accessor{fieldArrays, index};
    }
};

// ===== Velocity Specialization =====

#define VELOCITY_FIELDS(X) \
    X(vX, float, 0) \
    X(vY, float, 1) \
    X(vZ, float, 2)

template <>
struct SoARef<Velocity>
{
    using ComponentType = Velocity;

    void** fieldArrays = nullptr;
    uint32_t index = 0;

    void Bind(void** arrays, uint32_t idx)
    {
        fieldArrays = arrays;
        index = idx;
    }

    operator bool() const { return fieldArrays != nullptr; }

    struct Accessor
    {
        void** fieldArrays;
        uint32_t index;

        // Generate FieldProxy members for each field
#define GENERATE_FIELD(name, type, idx) FieldProxy<type> name;
        VELOCITY_FIELDS(GENERATE_FIELD)
#undef GENERATE_FIELD

        // Constructor initializes all FieldProxies
        Accessor(void** arrays, uint32_t idx)
            : fieldArrays(arrays), index(idx)
#define GENERATE_INIT(name, type, fieldIdx) , name((type*)arrays[fieldIdx], idx)
              VELOCITY_FIELDS(GENERATE_INIT)
#undef GENERATE_INIT
        {
        }

        Accessor* operator->() { return this; }
    };

    Accessor operator->()
    {
        return Accessor{fieldArrays, index};
    }
};

// ===== ColorData Specialization =====

#define COLORDATA_FIELDS(X) \
    X(R, float, 0) \
    X(G, float, 1) \
    X(B, float, 2) \
    X(A, float, 3)

template <>
struct SoARef<ColorData>
{
    using ComponentType = ColorData;

    void** fieldArrays = nullptr;
    uint32_t index = 0;

    void Bind(void** arrays, uint32_t idx)
    {
        fieldArrays = arrays;
        index = idx;
    }

    operator bool() const { return fieldArrays != nullptr; }

    struct Accessor
    {
        void** fieldArrays;
        uint32_t index;

#define GENERATE_FIELD(name, type, idx) FieldProxy<type> name;
        COLORDATA_FIELDS(GENERATE_FIELD)
#undef GENERATE_FIELD

        Accessor(void** arrays, uint32_t idx)
            : fieldArrays(arrays), index(idx)
#define GENERATE_INIT(name, type, fieldIdx) , name((type*)arrays[fieldIdx], idx)
              COLORDATA_FIELDS(GENERATE_INIT)
#undef GENERATE_INIT
        {
        }

        Accessor* operator->() { return this; }
    };

    Accessor operator->()
    {
        return Accessor{fieldArrays, index};
    }
};

// Type trait helper
template <typename T>
struct IsSoARef : std::false_type
{
};

template <typename T>
struct IsSoARef<SoARef<T>> : std::true_type
{
    using ComponentType = T;
};

/*
===========================================
USAGE - PERFECT SYNTAX!
===========================================

SoARef<Transform> transform;
transform->RotationX += dt;       // Direct member access!
transform->RotationY = 0.5f;      // Assignment!
transform->RotationX *= 2.0f;     // Compound operators!

float value = transform->RotationX;  // Reading!

NO FUNCTION CALLS - LOOKS EXACTLY LIKE A REGULAR POINTER!

===========================================
HOW IT WORKS
===========================================

1. Accessor struct contains FieldProxy members:
   struct Accessor {
       FieldProxy<float> RotationX;  // Not a function!
       FieldProxy<float> RotationY;  // Members!
   };

2. operator-> returns Accessor by value:
   Accessor operator->() {
       return Accessor{fieldArrays, index};
   }

3. transform->RotationX accesses the member:
   - operator-> returns temporary Accessor
   - .RotationX accesses the FieldProxy member
   - FieldProxy operators handle read/write

4. FieldProxy makes array access transparent:
   - operator FieldType() reads array[index]
   - operator= writes array[index]
   - operator+= modifies array[index]

===========================================
TO ADD NEW COMPONENT
===========================================

1. Define X-Macro with fields:
   #define MYCOMPONENT_FIELDS(X) \
       X(Field1, float, 0) \
       X(Field2, int, 1)

2. Copy-paste template specialization (3 places to change):
   - Replace all "Transform" with "MyComponent"
   - Replace "TRANSFORM_FIELDS" with "MYCOMPONENT_FIELDS"

3. Done! Clean syntax works automatically.

===========================================
WHY X-MACROS?
===========================================

X-Macros let us iterate over field definitions and generate code:

#define MYFIELDS(X) \
    X(Name, Type, Index)

// Generate struct members:
#define GEN(name, type, idx) FieldProxy<type> name;
MYFIELDS(GEN)  // Expands to: FieldProxy<Type> Name;

// Generate constructor initializers:
#define GEN(name, type, idx) , name((type*)arrays[idx], idx)
MYFIELDS(GEN)  // Expands to: , Name((Type*)arrays[Index], idx)

ONE DEFINITION -> MULTIPLE USES!
*/
