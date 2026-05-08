# Fixed-Point Coordinate System

> [Home](../Home.md) | [Determinism →](Determinism.md)

---

## Overview

`Fixed32` is an int32-based fixed-point scalar providing sub-millimeter precision and bit-identical arithmetic across platforms. It is the authoritative numeric type for all simulation-critical data: position, velocity, and force fields.

**Status:** Complete. `FieldProxy<Fixed32, WIDTH>` is wired into all position, velocity, and force fields. `SimFloat` aliases `SimFloatImpl<Fixed32>` under `TNX_DETERMINISM`. The engine runs deterministically; Jolt bridge validated (2026-05).

---

## Unit Definition

```
1 unit = 0.1mm (100 micrometers)
1 meter        =       10,000 units
1 km           =   10,000,000 units
Cell size (1km) = 5,000,000 units   → 430× headroom before overflow
World size (int64 cell origins): ~45M km at 0.1mm precision
```

`Fixed32` wraps `int32_t`. Maximum value: ±214,748 meters (±214 km within a cell). With int64 cell origins, the world extends to ~45 million km at full 0.1mm precision.

---

## `Fixed32` Implementation

```cpp
struct Fixed32
{
    int32_t Value;

    // Construction
    static Fixed32 FromMeters(float m)  { return { (int32_t)(m * 10000.f) }; }
    static Fixed32 FromFloat(float f)   { return { (int32_t)(f * 10000.f) }; }
    float ToFloat() const               { return Value * 0.0001f; }

    // Arithmetic — all operate on int32 directly
    Fixed32 operator+(Fixed32 rhs) const { return { Value + rhs.Value }; }
    Fixed32 operator-(Fixed32 rhs) const { return { Value - rhs.Value }; }
    Fixed32 operator*(Fixed32 rhs) const { return { (int32_t)((int64_t)Value * rhs.Value / 10000) }; }
    Fixed32 operator/(Fixed32 rhs) const { return { (int32_t)((int64_t)Value * 10000 / rhs.Value) }; }

    // Comparison
    bool operator< (Fixed32 rhs) const { return Value <  rhs.Value; }
    bool operator<=(Fixed32 rhs) const { return Value <= rhs.Value; }
    bool operator> (Fixed32 rhs) const { return Value >  rhs.Value; }
    bool operator>=(Fixed32 rhs) const { return Value >= rhs.Value; }
    bool operator==(Fixed32 rhs) const { return Value == rhs.Value; }
};

Fixed32 FixedSqrt(Fixed32 x);  // integer Newton-Raphson
```

Multiplication uses int64 intermediate to prevent overflow: `(int64_t)a * b / 10000`.

---

## `FixedUnit` — Trig Output

Trigonometric results use a separate `FixedUnit` type (1<<20 scale, representing [-1, 1]):

```cpp
struct FixedUnit { int32_t Value; };  // 1<<20 = 1.0

FixedUnit FixedSin(Fixed32 angle);
FixedUnit FixedCos(Fixed32 angle);
```

`FixedUnit` cross-multiplies with `Fixed32` via right-shift: `(int64_t)f32.Value * unit.Value >> 20`.

A LUT-based implementation (`FixedTrig.h`) provides `FixedSin` / `FixedCos` with table lookup and linear interpolation.

---

## `SimFloat` — Determinism Toggle

`SimFloat` is the canonical simulation numeric alias. Its concrete type is swapped at compile time:

```cpp
#ifdef TNX_DETERMINISM
    using SimFloat = SimFloatImpl<Fixed32>;
#else
    using SimFloat = SimFloatImpl<float>;
#endif
```

`SimFloatImpl<T>` wraps either `Fixed32` or `float` with a uniform arithmetic interface. All gameplay code uses `SimFloat` — switching determinism on or off is a single CMake flag with no code changes.

`FastSin` / `FastCos` / `Sqrt` / `Rsqrt` in `SimFloat.h` dispatch to the appropriate implementation based on the template parameter.

---

## Jolt Physics Bridge

Jolt uses `float32` internally. The bridge at the physics boundary:

```
ECS (Fixed32)  →  float32  →  Jolt step  →  float32  →  ECS (Fixed32)
```

**Precision at cell scale (≤±500m):**
- Float32 precision at 500m: ≈0.03mm
- Fixed32 unit: 0.1mm

Float32 is **finer** than Fixed32 at this scale — the conversion is lossless in practice. The 0.1mm unit definition was chosen to guarantee this.

**Jolt determinism requirement:** Jolt must be compiled with `JPH_CROSS_PLATFORM_DETERMINISTIC` (which disables FMA and forces precise floating-point math). This is automatically set when `TNX_ENABLE_ROLLBACK=ON`. Without it, Jolt's internal arithmetic produces different results on different CPUs.

---

## GPU Render Thread

The only lossy step in the entire pipeline:

```
Simulation (Fixed32 cell-local)
        │
        ▼  render thread upload
Fixed32 → camera-relative float32 (for GPU)
```

At ≤1km from the camera, float32 gives ≈0.05mm precision — finer than the 0.1mm unit definition. The conversion is imperceptible and happens on the render thread outside the authoritative simulation path. The GPU never sees the fixed-point representation.

This means the entire determinism guarantee lives on the simulation side; the GPU gets full float32 throughput with no precision concerns.

---

## World Coordinate System

Transforms are cell-local. Each cell has a `float64` (or `int64`) world origin. Entity positions within a cell use `Fixed32`.

```
World origin: int64/float64 cell position (allows ~45M km range)
Entity position: Fixed32 cell-local offset (0.1mm precision, ±214km range per cell)
```

The cell system enables very large worlds while keeping simulation numeric stability — Jolt's float32 bridge stays in a range where precision loss is below the simulation's unit definition.

---

## `FieldProxy<Fixed32, WIDTH>`

All position, velocity, and force fields use `FieldProxy<Fixed32, WIDTH>`. The three widths:

- `Scalar` — single entity, scalar update path
- `Wide` — AVX2 8-wide, unconditional store
- `WideMask` — AVX2 8-wide, Active-flag masked store

AVX2 operates on packed int32 values — `Fixed32` SIMD is pure integer arithmetic with no floating-point conversion in the hot path. The conversion to float happens only on explicit `ToFloat()` calls.

---

## Files

| File | Purpose |
|---|---|
| `src/Runtime/Math/Public/Fixed32.h` | Fixed-point scalar — int32, 0.1mm precision, all arithmetic ops, `FixedSqrt` |
| `src/Runtime/Math/Public/SimFloat.h` | `SimFloat` alias — `SimFloatImpl<float>` or `<Fixed32>` via `TNX_DETERMINISM`; `FastSin/Cos/Sqrt/Rsqrt` |
| `src/Runtime/Math/Public/FixedTrig.h` | `FixedSin` / `FixedCos` LUT with linear interpolation |
