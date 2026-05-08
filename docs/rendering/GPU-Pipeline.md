# GPU Pipeline

> [← Overview](Overview.md) | [Dirty Bit Upload →](Dirty-Bit-Upload.md) | [Home](../Home.md)

---

## Overview

The current GPU pipeline is a 3-pass Slang compute pipeline that converts the SoA entity slab into a compacted, interpolated InstanceBuffer for draw. All slab data is accessed via Vulkan Buffer Device Address — no per-frame descriptor set updates.

```
Slab (SoA field arrays)
        │
        ▼
1. predicate.slang   → scan[i] = 0 or 1  (active flag test)
        │
        ▼
2. prefix_sum.slang  → compacted offsets  (subgroup prefix + one atomicAdd/workgroup)
        │
        ▼
3. scatter.slang     → InstanceBuffer     (GPU interpolation + compacted SoA)
        │
        ▼
DrawIndexedIndirect  ← DrawArgs.instanceCount (written by scatter)
```

---

## `GpuFrameData`

All BDAs are packed into a single `GpuFrameData` struct pushed as a push constant or set-0 binding. The C++ mirror in `GpuFrameData.h` has a `static_assert` for 3192 bytes.

```cpp
struct GpuFrameData {
    uint64_t CurrFieldAddrs[MAX_FIELD_SEMANTICS];  // current frame slab slices
    uint64_t PrevFieldAddrs[MAX_FIELD_SEMANTICS];  // previous frame slab slices
    uint64_t ScanBDA;           // prefix scan output
    uint64_t InstanceBufferBDA; // output InstanceBuffer
    uint64_t DrawArgsBDA;       // indirect draw arguments
    // ... camera uniforms, entity counts, etc.
};
```

**14 field semantics** are defined: `SemFlags=1` (always index 0 by convention) through `SemColorA=14`, covering Flags, PosXYZ, RotXYZW, ScaleXYZ, ColorRGBA.

---

## Pass 1 — `predicate.slang`

Reads the active flag from the slab (bit 31 of the flags field at `CurrFieldAddrs[0]`). Writes `scan[i] = 1` for active entities, `0` for inactive. This is the culling hook — frustum tests and HZB occlusion tests will be added here.

```glsl
// Per entity i:
uint flags = LoadFlags(CurrFieldAddrs[0], i);
scan[i] = (flags >> 31) & 1u;  // Active bit
```

*Future:* After active-flag test, evaluate frustum planes against entity bounds. Check t-1 HZB for occlusion culling. Active entities that fail culling write `scan[i] = 0`.

---

## Pass 2 — `prefix_sum.slang`

Converts the `scan[]` array (0/1 per entity) into compacted offsets (exclusive prefix sum). Uses subgroup-level prefix within a workgroup plus one `atomicAdd` per workgroup to accumulate the global count — single dispatch, no second pass needed.

The final accumulated value (`scan[entityCount]` conceptually) is written to `DrawArgs.instanceCount`.

---

## Pass 3 — `scatter.slang`

For each active entity (where `scan[i] > 0`), writes a packed `InstanceData` record into `InstanceBuffer[scan[i]]`. Applies GPU-side interpolation between the current and previous frame positions for sub-frame smoothness:

```glsl
// Per active entity i:
uint dst = scan[i];

vec3 currPos = LoadPos(CurrFieldAddrs, i);
vec3 prevPos = LoadPos(PrevFieldAddrs, i);
InstanceBuffer[dst].Pos = mix(prevPos, currPos, SubFrameAlpha);

// Also writes: rotation, scale, color, mesh ref
```

`SubFrameAlpha` is passed in `GpuFrameData` by the render thread based on the time elapsed since the last logic tick.

The GPU keeps its own previous-frame `InstanceBuffer`. The scatter shader lerps between the *current slab frame* and *previous GPU InstanceBuffer frame* — the CPU never needs to supply two logic frames simultaneously. This is what allows the Volatile tier to be only 3 frames deep instead of 5.

---

## 5 InstanceBuffers

Five `InstanceBuffer` slots cycle independently of the 2 GPU frame-in-flight slots. This breaks the VSync stall chain:

```
Without cycling:
VSync holds GPU buffer
  → render thread blocks on buffer
  → render thread holds slab read lock
  → logic thread stalls on slab write lock

With 5 InstanceBuffers:
Render thread always has a free buffer → Logic never waits
```

If the render thread falls behind by more than 5 frames it becomes a renderer performance problem, not a synchronisation problem that contaminates the simulation.

---

## Barrier Between Compute and Graphics

After the scatter pass completes:

```
Compute → Graphics barrier:
    dstStage = VERTEX_SHADER_BIT | DRAW_INDIRECT_BIT
```

The InstanceBuffer write and the DrawArgs write must both be visible before the raster pass reads them.

---

## Slang Shader Layout

```
shaders/
  GpuFrameData.slang   — shared struct header; C++ mirror in GpuFrameData.h
  predicate.slang      — active-flag (+ future: frustum/HZB) → scan[]
  prefix_sum.slang     — scan[] → compacted offsets, DrawArgs.instanceCount
  scatter.slang        — compacted + interpolated → InstanceBuffer
  cube.vert            — vertex shader (reads InstanceBuffer via BDA)
  cube.frag            — fragment shader
```

CMakeLists invokes `slangc` with `-I shaders` for all shader targets, with `GpuFrameData.slang` in DEPENDS so a header change triggers recompilation.

---

## Compute→Graphics Synchronisation: The Current Model

Because Logic writes to the slab and the render thread dispatches GPU work that reads from it:

1. Logic thread writes to `WriteArray` (the active write frame)
2. `PublishCompletedFrame` signals the render thread
3. Render thread picks up the completed frame's field array BDAs
4. Render thread dispatches predicate → prefix_sum → scatter using those BDAs
5. Scatter writes into `InstanceBuffer[N % 5]`
6. Raster reads from that InstanceBuffer

The 5-slot InstanceBuffer rotation ensures steps 4–6 never conflict with the previous frame's raster read.

---

## Camera Integration

`LogicThread::PublishCompletedFrame` writes the Vulkan right-handed perspective matrix + view matrix into `GpuFrameData` each logic frame. The render thread reads these on the next dispatch. Camera state is treated like any other per-frame write — it goes through the same publish/handoff mechanism as entity transforms.
