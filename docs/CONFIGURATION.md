# Engine Configuration

> **Navigation:** [← Back to README](../README.md) | [← Data Structures](DATA_STRUCTURES.md) | [Current Status →](STATUS.md)

---

## EngineConfig Structure

All engine timing, threading, and memory parameters are configured through `EngineConfig`:

```cpp
struct EngineConfig
{
    // === Timing Configuration ===

    // Variadic Update (Logic Thread)
    // 0 = Uncapped, runs as fast as possible
    // >0 = Caps logic thread to target FPS (e.g., 240 for 240 FPS max)
    int TargetFPS = 0;

    // Fixed Update Rate (Physics/Simulation)
    // Runs at deterministic timestep regardless of frame rate
    // Higher = more responsive, but more CPU intensive
    // Common values: 60, 128, 256, 512
    int FixedUpdateHz = 128;

    // Network Tick Rate
    // How often to send/receive network packets
    // Lower = less bandwidth, higher = more precision
    // Common values: 20, 30, 60
    int NetworkUpdateHz = 30;

    // Input Polling Rate (Main Thread)
    // How often Sentinel samples input events
    // Higher = better input latency
    // Typical: 1000 (1ms polling)
    int InputPollHz = 1000;

    // === Memory Configuration ===

    // Maximum Dynamic Entities
    // Preallocates memory for this many entities
    // Exceeding this will trigger allocations (not ideal)
    int MaxDynamicEntities = 100000;

    // History Buffer Page Count
    // Number of frames to store in History Slab
    // MUST be power of 2, minimum 4
    // Memory usage scales linearly with this value
    //
    // Examples:
    //   4:   Minimal (render interpolation only)
    //   16:  Client prediction + rollback
    //   64:  Lag compensation, replay
    //   128: Full GGPO-style rollback, 1s @ 128Hz or 0.25s @ 512Hz
    int HistoryBufferPages = 128;

    // === Determinism ===

    // If true: Logic waits for Render if History Slab wraps around
    // If false: Logic skips to next available section (may overwrite old data)
    // Set to true for networked games requiring perfect consistency
    bool DeterministicFrames = false;

    // === Helper Functions ===

    // Get frame time for main thread limiter
    double GetTargetFrameTime() const
    {
        return (TargetFPS > 0) ? 1.0 / TargetFPS : 0.0;
    }

    // Get fixed timestep for physics simulation
    double GetFixedStepTime() const
    {
        return 1.0 / FixedUpdateHz;
    }

    // Get network tick interval
    double GetNetworkStepTime() const
    {
        return (NetworkUpdateHz > 0) ? 1.0 / NetworkUpdateHz : 0.0;
    }

    // Get input polling interval
    double GetInputPollInterval() const
    {
        return 1.0 / InputPollHz;
    }

    // Get history duration in seconds
    double GetHistoryDuration() const
    {
        return (double)HistoryBufferPages / (double)FixedUpdateHz;
    }
};
```

---

## Configuration Presets

### Lightweight (Rendering Focus)

```cpp
EngineConfig lightweightConfig;
lightweightConfig.FixedUpdateHz = 60;           // Standard physics rate
lightweightConfig.HistoryBufferPages = 4;       // Minimal memory (just for interpolation)
lightweightConfig.MaxDynamicEntities = 50000;   // Moderate entity count
lightweightConfig.NetworkUpdateHz = 0;          // No networking
lightweightConfig.DeterministicFrames = false;  // Performance over consistency
```

**Memory Usage:** ~52 MB for 50k entities
**Use Case:** Single-player games, simple simulations

---

### Balanced (Standard Game)

```cpp
EngineConfig balancedConfig;
balancedConfig.FixedUpdateHz = 128;             // Responsive simulation
balancedConfig.HistoryBufferPages = 16;         // Short rollback window
balancedConfig.MaxDynamicEntities = 100000;     // Large entity count
balancedConfig.NetworkUpdateHz = 30;            // Standard tick rate
balancedConfig.DeterministicFrames = false;     // Favor performance
```

**Memory Usage:** ~317 MB for 100k entities
**Use Case:** Most networked games, action games

---

### Competitive (High-Frequency)

```cpp
EngineConfig competitiveConfig;
competitiveConfig.FixedUpdateHz = 512;          // Ultra-responsive (1.95ms per frame)
competitiveConfig.HistoryBufferPages = 128;     // 0.25s history for lag comp
competitiveConfig.MaxDynamicEntities = 50000;   // Fewer entities for higher Hz
competitiveConfig.InputPollHz = 1000;           // 1ms input latency
competitiveConfig.NetworkUpdateHz = 60;         // High tick rate
competitiveConfig.DeterministicFrames = true;   // Consistency required
```

**Memory Usage:** ~945 MB for 50k entities
**Use Case:** Fighting games, competitive shooters, esports titles

---

### Simulation (Maximum Entities)

```cpp
EngineConfig simulationConfig;
simulationConfig.FixedUpdateHz = 60;            // Lower Hz for more entities
simulationConfig.HistoryBufferPages = 8;        // Minimal history
simulationConfig.MaxDynamicEntities = 250000;   // Massive entity count
simulationConfig.NetworkUpdateHz = 0;           // No networking
simulationConfig.DeterministicFrames = false;   // Performance focus
```

**Memory Usage:** ~327 MB for 250k entities (8 pages)
**Use Case:** Strategy games, simulations, crowd rendering

---

## Memory Calculations

### Per-Entity Memory Footprint (Physics-Enabled)

Based on realistic hot component mix (90% simple, 10% complex):

**Hot Component Sizes:**
- **Simple Entity:** Transform (36B) + Velocity (24B) + Forces (24B) + Collider (32B) = **116 bytes**
- **Complex Entity:** Transform (36B) + Velocity (24B) + Forces (24B) + Collider (32B) + BoneArray/Extra (200B) = **316 bytes**

```cpp
size_t CalculateMemoryUsage(const EngineConfig& config)
{
    // 90/10 simple/complex split
    const size_t simpleCount = config.MaxDynamicEntities * 0.9;
    const size_t complexCount = config.MaxDynamicEntities * 0.1;

    const size_t simpleBytes = 116;  // Simple hot components
    const size_t complexBytes = 316; // Complex hot components

    const size_t hotMemory = (simpleCount * simpleBytes + complexCount * complexBytes)
                           * config.HistoryBufferPages;

    const size_t coldMemory = config.MaxDynamicEntities * 50;  // Cold components (no history)
    const size_t metadata = hotMemory * 0.05;  // ECS metadata (~5%)

    return hotMemory + coldMemory + metadata;
}
```

### Memory Table

| Entities | 4 Pages   | 16 Pages  | 64 Pages  | 128 Pages |
|----------|-----------|-----------|-----------|-----------|
| 10k      | 10.4 MB   | 31.7 MB   | 97 MB     | 189 MB    |
| 50k      | 52 MB     | 158 MB    | 485 MB    | 945 MB    |
| 100k     | 104 MB    | 317 MB    | 970 MB    | 1.89 GB   |
| 250k     | 260 MB    | 792 MB    | 2.42 GB   | 4.72 GB   |

**Note:** This includes hot components (History Slab), cold components (archetype chunks), and ECS metadata. Assumes 90% simple physics entities, 10% complex (characters/vehicles).

---

## Performance Impact

### FixedUpdateHz

| Hz  | Frame Budget | Use Case                          | CPU Load |
|-----|--------------|-----------------------------------|----------|
| 60  | 16.67ms      | Standard games, VR                | Low      |
| 128 | 7.81ms       | Responsive action games           | Medium   |
| 256 | 3.91ms       | High-precision simulations        | High     |
| 512 | 1.95ms       | **TARGET** - Ultra-responsive     | Very High|
| 1000| 1.0ms        | Extreme (research only)           | Extreme  |

**Diminishing Returns:** Beyond 256Hz, human perception gains are minimal. 512Hz is the target for demonstrating technical capability.

### HistoryBufferPages

| Pages | Memory Impact | Latency Impact | Use Cases                     |
|-------|---------------|----------------|-------------------------------|
| 4     | Minimal       | None           | Rendering only                |
| 8-16  | Low           | None           | Basic rollback                |
| 32-64 | Medium        | None           | Lag compensation, replay      |
| 128+  | High          | None           | GGPO, full server rewind      |

**CPU Impact:** Negligible - page count doesn't affect frame time, only memory usage.

---

## Runtime Configuration

### Initialization

```cpp
int main()
{
    EngineConfig config;
    config.FixedUpdateHz = 512;
    config.HistoryBufferPages = 128;
    config.MaxDynamicEntities = 100000;
    config.InputPollHz = 1000;
    config.DeterministicFrames = false;

    StrigidEngine& engine = StrigidEngine::Get();
    if (engine.Initialize("My Game", 1920, 1080, config))
    {
        engine.Run();
    }

    return 0;
}
```

### Dynamic Adjustment (Not Recommended)

Most config values are baked into allocations at startup. Changing `HistoryBufferPages` or `MaxDynamicEntities` at runtime requires reallocating the entire History Slab.

**Safe to change at runtime:**
- `TargetFPS` (logic thread limiter)
- `NetworkUpdateHz` (if networking system supports it)

**Unsafe to change at runtime:**
- `FixedUpdateHz` (requires History Slab reallocation)
- `HistoryBufferPages` (requires History Slab reallocation)
- `MaxDynamicEntities` (requires ECS reallocation)

---

## Validation

The engine validates configuration at startup:

```cpp
bool EngineConfig::Validate() const
{
    if (HistoryBufferPages < 8) {
        LOG_ERROR("HistoryBufferPages must be >= 8");
        return false;
    }

    if (!IsPowerOfTwo(HistoryBufferPages)) {
        LOG_ERROR("HistoryBufferPages must be power of 2");
        return false;
    }

    if (FixedUpdateHz <= 0) {
        LOG_ERROR("FixedUpdateHz must be > 0");
        return false;
    }

    if (FixedUpdateHz > 1000) {
        LOG_WARN("FixedUpdateHz > 1000 is not recommended (frame budget <1ms)");
    }

    if (MaxDynamicEntities > 1000000) {
        LOG_WARN("MaxDynamicEntities > 1M may exceed memory limits");
    }

    size_t estimatedMemory = CalculateMemoryUsage(*this);
    if (estimatedMemory > 4ULL * 1024 * 1024 * 1024) {  // 4 GB
        LOG_WARN_F("Estimated memory usage: %.2f GB", estimatedMemory / (1024.0 * 1024.0 * 1024.0));
    }

    return true;
}
```

---
