# Data Structures Reference

> **Navigation:** [← Back to README](../README.md) | [← Performance](PERFORMANCE_TARGETS.md) | [Configuration →](CONFIGURATION.md)

---

## HistorySectionHeader (Replaces FramePacket)

Each section of the History Slab begins with this header:

```cpp
struct HistorySectionHeader
{
    // Ownership tracking (atomic bitfield for multiple readers)
    std::atomic<uint8_t> OwnershipFlags;
    /*
        Bitfield values:
        0x01 = LOGIC_WRITING   (exclusive)
        0x02 = RENDER_READING  (can coexist with other readers)
        0x04 = NETWORK_READING (can coexist with other readers)
        0x08 = DEFRAG_LOCKED   (exclusive, blocks all access)
    */

    // Frame identification
    uint32_t FrameNumber;
    double SimulationTime;  // Accumulated time since engine start

    // Camera/View data (replaces FramePacket)
    alignas(16) Matrix4 ViewMatrix;
    alignas(16) Matrix4 ProjectionMatrix;
    Vector3 CameraPosition;
    float _pad0;

    // Scene/Lighting data
    Vector3 SunDirection;
    float _pad1;
    Vector3 SunColor;
    float AmbientIntensity;

    // Entity metadata
    uint32_t ActiveEntityCount;
    uint32_t TotalAllocatedEntities;

    // Padding to cache line (64 bytes)
    char _padding[/* calculated to align to 64 */];
};

static_assert(sizeof(HistorySectionHeader) % 64 == 0, "Header must be cache-aligned");
```

**Usage:**

```cpp
// Logic Thread writes header when committing frame
void LogicThread::CommitFrame()
{
    HistorySection* section = slab.GetSection((FrameNumber + 1) % SectionCount);

    section->Header.FrameNumber = FrameNumber + 1;
    section->Header.SimulationTime = SimTime;
    section->Header.ViewMatrix = camera.GetViewMatrix();
    section->Header.ProjectionMatrix = camera.GetProjection();
    section->Header.CameraPosition = camera.GetPosition();
    section->Header.SunDirection = scene.GetSunDirection();
    section->Header.ActiveEntityCount = registry->GetEntityCount();

    LastFrameWritten.store(FrameNumber + 1, std::memory_order_release);
}

// Render Thread reads header for interpolation
void RenderThread::ProcessFrame()
{
    uint32_t latestFrame = LastFrameWritten.load(std::memory_order_acquire);
    HistorySection* currSection = slab.GetSection(latestFrame % SectionCount);

    Matrix4 viewProj = currSection->Header.ProjectionMatrix * currSection->Header.ViewMatrix;
    uint32_t entityCount = currSection->Header.ActiveEntityCount;

    // Use for culling, rendering, etc.
}
```

---

## FieldProxy (SoA Indirection)

The core of the component decomposition system:

```cpp
template <typename FieldType>
struct FieldProxy
{
    FieldType* _restrict array;  // Points to field's SoA array in History Slab
    uint32_t* _restrict index;   // Points to shared entity index in EntityView

    // Implicit conversion for reads
    operator FieldType() const { return array[*index]; }

    // Assignment operator for writes
    FieldProxy& operator=(FieldType value) {
        array[*index] = value;
        return *this;
    }

    // Compound assignment operators
    FieldProxy& operator+=(FieldType value) { array[*index] += value; return *this; }
    FieldProxy& operator-=(FieldType value) { array[*index] -= value; return *this; }
    FieldProxy& operator*=(FieldType value) { array[*index] *= value; return *this; }
    FieldProxy& operator/=(FieldType value) { array[*index] /= value; return *this; }

    // Bind to field array during hydration
    __forceinline void Bind(void* bindArray, uint32_t* idx) {
        array = (FieldType*)bindArray;
        index = idx;
    }
};
```

**Key Properties:**
- Zero overhead: All operations inline to direct array access
- Maintains OOP syntax: `transform.PositionX += velocity.VelocityX`
- Compiler optimizes away the proxy layer entirely
- SIMD-friendly: Arrays are contiguous, vectorizable

---

## Component Definition Pattern

**Hot Component (Lives in History Slab):**

```cpp
struct alignas(16) Transform
{
    // Mark as hot component (lives in History Slab)
    STRIGID_HOT_COMPONENT()

    // Position fields
    FieldProxy<float> PositionX;
    FieldProxy<float> PositionY;
    FieldProxy<float> PositionZ;

    // Rotation fields (Euler angles)
    FieldProxy<float> RotationX;
    FieldProxy<float> RotationY;
    FieldProxy<float> RotationZ;

    // Scale fields
    FieldProxy<float> ScaleX;
    FieldProxy<float> ScaleY;
    FieldProxy<float> ScaleZ;

    // Define fields for reflection system
    STRIGID_REGISTER_FIELDS(Transform,
        PositionX, PositionY, PositionZ,
        RotationX, RotationY, RotationZ,
        ScaleX, ScaleY, ScaleZ)
};

// Register component type for type ID generation
STRIGID_REGISTER_COMPONENT(Transform)

static_assert(alignof(Transform) == 16, "Transform must be 16-byte aligned");
```

**Cold Component (Lives in Archetype Chunks):**

```cpp
struct HealthComponent
{
    // No STRIGID_HOT_COMPONENT() marker - lives in chunks

    float CurrentHealth;
    float MaxHealth;
    float Armor;
    float RegenRate;

    // No FieldProxy needed - cold components don't need SoA decomposition
    // Accessed infrequently, doesn't need History Slab
};

STRIGID_REGISTER_COMPONENT(HealthComponent)
```

---

## Entity Definition Pattern

**Entity Class with Lifecycle:**

```cpp
// Forward declare for registration macro
STRIGID_REGISTER_ENTITY(CubeEntity);

class CubeEntity : public EntityView<CubeEntity>
{
public:
    // Component members (hot components use FieldProxy internally)
    Transform transform;
    Velocity velocity;
    ColorData color;

    // Lifecycle: PrePhysics (runs at FixedUpdateHz)
    __forceinline void PrePhysics(double dt)
    {
        // OOP-style syntax, SoA performance
        transform.PositionX += velocity.VelocityX * static_cast<float>(dt);
        transform.PositionY += velocity.VelocityY * static_cast<float>(dt);
        transform.PositionZ += velocity.VelocityZ * static_cast<float>(dt);

        transform.RotationY += static_cast<float>(dt) * 0.7f;

        // Damping
        velocity.VelocityX *= 0.99f;
        velocity.VelocityY *= 0.99f;
    }

    // Optional: PostPhysics (runs after physics solver)
    __forceinline void PostPhysics(double dt)
    {
        // Collision response, state updates, etc.
    }

    // Define schema for reflection (MUST be at end of class)
    STRIGID_REGISTER_SCHEMA(CubeEntity, EntityView<CubeEntity>,
        transform, velocity, color)
};
```

**Inheritance Pattern:**

```cpp
// Base class for shared logic
template <typename T>
class BaseCube : public EntityView<T>
{
public:
    Transform transform;
    ColorData color;

    __forceinline void PrePhysics(double dt)
    {
        transform.RotationY += static_cast<float>(dt) * 0.7f;
    }

    STRIGID_REGISTER_SCHEMA(BaseCube, EntityView<T>, transform, color)
};

// Derived entity type
STRIGID_REGISTER_ENTITY(SuperCube);
class SuperCube : public BaseCube<SuperCube>
{
public:
    Velocity velocity;  // Add extra component

    __forceinline void PrePhysics(double dt)
    {
        // Override base behavior
        BaseCube<SuperCube>::PrePhysics(dt);

        // Add velocity logic
        transform.PositionX += velocity.VelocityX * static_cast<float>(dt);
    }

    STRIGID_REGISTER_SCHEMA(SuperCube, BaseCube<SuperCube>, velocity)
};
```

---

## EntityView Pattern

The base class for all entity types:

```cpp
template <typename T>
class EntityView
{
public:
    uint32_t ViewIndex = 0;  // Shared index for all FieldProxies

    // Generate unique ClassID per entity type
    static ClassID StaticClassID()
    {
        static ClassID id = Internal::g_GlobalClassCounter++;
        return id;
    }

    // Hydrate: Bind all component FieldProxies to field arrays
    __forceinline void Hydrate(void** fieldArrayTable, uint32_t index)
    {
        ViewIndex = index;
        constexpr auto schema = T::DefineSchema();

        size_t fieldArrayBaseIndex = 0;

        // For each component in schema
        std::apply([&](auto&&... members)
        {
            (..., [&](auto member)
            {
                if constexpr (std::is_member_object_pointer_v<decltype(member)>)
                {
                    using MemberType = std::remove_reference_t<decltype(static_cast<T*>(this)->*member)>;

                    // Check if this is a FieldProxy component
                    if constexpr (HasDefineFields<MemberType>)
                    {
                        // Bind component's FieldProxies to field array table
                        (static_cast<T*>(this)->*member).Bind(
                            &fieldArrayTable[fieldArrayBaseIndex],
                            &ViewIndex
                        );

                        // Advance by number of fields in this component
                        fieldArrayBaseIndex += ComponentFieldRegistry::Get()
                            .GetFieldCount(GetComponentTypeID<MemberType>());
                    }
                }
            }(members));
        }, schema.members);
    }

    // Advance to next entity (just increment shared index)
    __forceinline void Advance(uint32_t stepSize)
    {
        ViewIndex += stepSize;
    }
};
```

**Iteration Pattern (Internal):**

```cpp
// Generated by reflection system for each entity type
template <typename T>
__forceinline void InvokePrePhysicsImpl(double dt, void** fieldArrayTable, uint32_t count)
{
    alignas(16) T entityView;
    entityView.Hydrate(fieldArrayTable, 0);  // Bind all FieldProxies

    #pragma loop(ivdep)  // Hint for auto-vectorization
    for (uint32_t i = 0; i < count; ++i)
    {
        entityView.PrePhysics(dt);
        entityView.Advance(1);  // Just increments ViewIndex
    }
}
```

---

## InterpEntry (Render-Ready Data)

The output of the interpolation + culling pass:

```cpp
struct InterpEntry
{
    uint64_t sortKey;       // 64-bit composite key for state sorting
    Vector3 position;       // Interpolated position
    Quaternion rotation;    // Interpolated rotation (or Euler)
    Vector3 scale;          // Interpolated scale
    Color color;            // From ColorData component
    uint32_t meshID;        // Mesh to render
    uint32_t materialID;    // Material to apply
};

// Ephemeral buffer (cleared each frame)
std::vector<InterpEntry> InterpBuffer;
```

**Usage:**

```cpp
void RenderThread::ProcessFrame()
{
    InterpBuffer.clear();
    InterpBuffer.reserve(currSection->Header.ActiveEntityCount);

    // Read directly from History Slab sections
    Transform* prevTransforms = prevSection->GetComponentArray<Transform>();
    Transform* currTransforms = currSection->GetComponentArray<Transform>();
    ColorData* currColors = currSection->GetComponentArray<ColorData>();

    for (uint32_t i = 0; i < entityCount; ++i)
    {
        // Dead entity check
        if (IsInactive(currTransforms[i])) continue;

        // Interpolate
        Vector3 interpPos = Lerp(
            Vector3(prevTransforms[i].PositionX, prevTransforms[i].PositionY, prevTransforms[i].PositionZ),
            Vector3(currTransforms[i].PositionX, currTransforms[i].PositionY, currTransforms[i].PositionZ),
            alpha
        );

        // Frustum cull
        if (!camera.IsVisible(interpPos)) continue;

        // Only visible entities reach InterpBuffer
        InterpBuffer.push_back({
            sortKey: ComputeSortKey(interpPos, meshID, materialID),
            position: interpPos,
            rotation: ...,
            scale: ...,
            color: Color(currColors[i].R, currColors[i].G, currColors[i].B, currColors[i].A),
            meshID: GetMeshID(i),
            materialID: GetMaterialID(i)
        });
    }

    // Job system sorts and batches InterpBuffer
    // Then uploads to GPU via Transfer Buffer
}
```

**Future: Job-Based Rendering:**

```cpp
struct RenderJob
{
    uint32_t startIndex;  // Index into sorted InterpBuffer
    uint32_t count;       // Number of entities to draw
    uint64_t sortKeyMin;  // Min sort key in this job
    uint64_t sortKeyMax;  // Max sort key in this job
};

// After sorting InterpBuffer, split into jobs
std::vector<RenderJob> jobs = SplitIntoJobs(InterpBuffer);

// Jobs can run in parallel (different GPU command buffers)
// Then submitted in order based on sort key ranges
for (auto& job : jobs)
{
    job.WaitForDependencies();  // Previous job if sort key overlap
    job.Execute();
}
```

---

## DrawCall / State Sorting (Planned)

**64-Bit Sort Key:**

```cpp
union SortKey
{
    uint64_t value;

    struct
    {
        uint64_t mesh     : 16;  // 65536 unique meshes
        uint64_t material : 16;  // 65536 unique materials
        uint64_t pipeline : 12;  // 4096 unique pipelines (shader/blend states)
        uint64_t depth    : 16;  // Distance from camera (quantized)
        uint64_t layer    : 4;   // 0=Background, 1=Opaque, 2=Transparent, 3=UI
    };
};

uint64_t ComputeSortKey(Vector3 pos, uint32_t meshID, uint32_t materialID)
{
    SortKey key;
    key.layer = 1;  // Opaque
    key.depth = QuantizeDepth(camera.DistanceTo(pos));
    key.pipeline = GetPipelineID(materialID);
    key.material = materialID;
    key.mesh = meshID;
    return key.value;
}
```

**Radix Sort:**

```cpp
void RadixSort(std::vector<InterpEntry>& buffer)
{
    // O(N) integer sort, cache-friendly
    // Sort by full 64-bit key
    // Automatically groups by Layer → Depth → Pipeline → Material → Mesh
}
```

---

## SnapshotEntry (Legacy - Being Removed)

**Current implementation** (will be replaced by direct History Slab access):

```cpp
struct alignas(16) SnapshotEntry
{
    float PositionX, PositionY, PositionZ;
    float RotationX, RotationY, RotationZ;
    float ScaleX, ScaleY, ScaleZ;
    float _pad0, _pad1, _pad2;
    float ColorR, ColorG, ColorB, ColorA;
};
// Total: 64 bytes (matches GPU InstanceData layout)

// Double-buffered
std::vector<SnapshotEntry> SnapshotPrevious;
std::vector<SnapshotEntry> SnapshotCurrent;
```

**Problem:** Requires expensive memcpy (5-8ms for 100k entities)

**Solution:** History Slab eliminates this by allowing direct pointer access

---

## InstanceData (GPU Upload Format)

Final format uploaded to GPU:

```cpp
struct alignas(16) InstanceData
{
    float PositionX, PositionY, PositionZ, _pad0;  // 16 bytes
    float RotationX, RotationY, RotationZ, _pad1;  // 16 bytes
    float ScaleX, ScaleY, ScaleZ, _pad2;           // 16 bytes
    float ColorR, ColorG, ColorB, ColorA;          // 16 bytes
};
// Total: 64 bytes (optimal for GPU vectorization)

static_assert(sizeof(InstanceData) == 64, "Must match GPU layout");
static_assert(alignof(InstanceData) == 16, "Must be 16-byte aligned");
```

**Upload Path:**

```
InterpBuffer → Transfer Buffer → Instance Buffer → GPU
     CPU            CPU/GPU         GPU              GPU
```

---
