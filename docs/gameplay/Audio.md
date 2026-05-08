# Audio

> [← Physics](Physics.md) | [Home](../Home.md)

---

## Overview

**Status:** Complete (2026-05). `AudioManager`: SDL3 device, fixed voice pool, handle-based playback, `PlayParams`, `AudioEventEntry` registry, per-voice fade, voice stealing by priority, lazy-load + auto-unload.

The audio system is designed as the substrate for a full Wwise-class system and GPU Physics-Based Audio (PBA) synthesis, without requiring future architectural rewrites. The initial SDL3 thin wrapper provides Anti-Event compatibility via handle-based `FadeOut`.

---

## Stack

| Component | Choice | Rationale |
|---|---|---|
| Device output | SDL3 `SDL_AudioStream` | Already linked, handles mixing, thread-safe push API |
| WAV decode | SDL3 `SDL_LoadWAV` | Zero-dependency, already available |
| OGG decode | `stb_vorbis.h` (single-header) | Public domain, 30KB, no build system impact |
| GPU PBA | Vulkan compute shader (planned) | Modal synthesis from physics contact events |

Audio update runs on the **Sentinel thread** at a configurable rate (default 250Hz). Sentinel skips audio tick when `FrameNumber % (SentinelHz / AudioUpdateHz) != 0`. SDL's internal audio callback thread pulls from `SDL_AudioStream` independently.

---

## Public API

### `PlayParams`

Call sites never break as features are added — new fields have safe defaults:

```cpp
struct PlayParams
{
    float    Volume   = 1.f;
    float    Pitch    = 1.f;
    bool     Loop     = false;
    BusID    Bus      = BusID::Master;  // stub enum — routing added later
    uint8_t  Priority = 128;            // 0=lowest; voice stealing uses this
};

SoundHandle Play(const SoundAsset* asset, PlayParams params = {});
```

### `SoundHandle`

```cpp
struct SoundHandle
{
    uint16_t Index;       // slot in voice pool
    uint16_t Generation;  // stale-handle detection — bumped on Stop/reuse
    bool IsValid() const  { return Index != 0 || Generation != 0; }
    static SoundHandle Invalid() { return {0, 0}; }
};
```

Stale handles (stopped/reused voice) are silently ignored by all API calls. There is no UB on stale access.

### Core API

```cpp
SoundHandle  Play(const SoundAsset* asset, PlayParams params = {});
void         Stop(SoundHandle handle);
void         FadeOut(SoundHandle handle, float seconds);    // Anti-Event compatible
void         SetVolume(SoundHandle handle, float volume);
bool         IsPlaying(SoundHandle handle) const;
```

---

## Voice Pool

Pool size is configurable in `EngineConfig::MaxAudioVoices` (default 64).

```cpp
struct Voice
{
    SDL_AudioStream*   Stream     = nullptr;
    SoundHandle        Handle     = SoundHandle::Invalid();
    const SoundAsset*  Asset      = nullptr;
    std::atomic<float> Volume     = {1.f};
    float              FadeTarget = 1.f;
    float              FadeRate   = 0.f;   // volume/sec; 0 = no fade
    uint8_t            Priority   = 128;
    bool               bLoop      = false;
};
```

**Voice stealing:** When the pool is full, the lowest-priority active voice is evicted. Priority ties are broken by age (oldest stolen first).

**Lazy-load / auto-unload:** `SoundAsset` PCM data is loaded on first `Play()` and unloaded when the last referencing voice finishes. No pre-load step required.

**Pinned slots:** High-frequency sounds (footsteps, UI clicks) can reserve a dedicated slot to prevent stealing.

---

## `AudioEventEntry` Registry

Named events provide a data-driven layer over the raw `Play` API:

```cpp
struct AudioEventEntry
{
    const char*  Name;           // event name key
    SoundAsset*  Asset;          // source asset
    PlayParams   DefaultParams;  // volume, pitch, loop, bus, priority
};

SoundHandle TriggerEvent(const char* eventName, PlayParams overrides = {});
```

Future: `AudioEvent::Trigger()` will fill `PlayParams` from RTPC state before forwarding.

---

## Speculative Presentation Integration (Anti-Events)

`FadeOut(handle, seconds)` is the Anti-Event primitive. When the `PresentationReconciler` determines an effect was wrongly predicted (rollback), it calls `FadeOut` on the relevant `SoundHandle` to gracefully decay the sound over ~20ms rather than cutting it instantly.

The reconciler diff logic itself is not yet implemented — see [Rollback Netcode](../networking/Rollback-Netcode.md).

---

## GPU Physics-Based Audio (Planned)

The long-term audio architecture synthesizes sounds from physics contact events rather than triggering pre-baked samples.

### What PBA enables

Instead of playing `metal_impact.wav` on collision, the engine synthesizes the sound from the physical properties of the objects:

- `CMaterialAudio` component (cold tier): `Frequencies[8]`, `Decays[8]`, `Gains[8]` per material
- Collision impulse excites those modes at the contact frame
- Output: sum of decaying sinusoids → raw PCM → `SDL_AudioStream`

Result: 1000 physics objects = 1000 physically correct, unique sounds. No sample library.

### Pipeline

```
Jolt contact events → AudioTriggerBuffer (ring, Brain thread)
        │
        ▼  (Sentinel AudioSystem::Update)
Vulkan compute: modal_synth.slang
  input:  AudioTriggerBuffer (contact point, impulse magnitude, material IDs)
  input:  MaterialModalData  (per-material: N modes, freq[], decay[], gain[])
  output: PcmSynthBuffer     (float32, 256-sample blocks per trigger, summed)
        │
        ▼  (async readback, 1-frame latency)
SDL_AudioStream push
```

The GPU PBA pipeline is not synthesis-only — sampled audio (music, VO, designed SFX) can also route through it for geometry-aware DSP (echo, material dampening, air absorption).

---

## `EngineConfig` Fields

```cpp
uint32_t AudioUpdateHz  = 250;   // Sentinel audio tick rate
uint32_t MaxAudioVoices = 64;    // Voice pool size
```
