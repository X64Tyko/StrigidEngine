# Threading Model

> [← Overview](Overview.md) | [ECS & Storage →](ECS-And-Storage.md) | [Home](../Home.md)

---

## The Trinyx Trinity

Three dedicated threads form the core of the engine. Brain and Encoder are **coordinators, not dedicated workers** — they initialize frames, distribute work, and then act as workers themselves while waiting for jobs to complete.

| Thread | Rate | Responsibilities |
|---|---|---|
| **Sentinel** (main) | 1000Hz | SDL event loop, input polling, Vulkan object lifetime, frame pacing |
| **Brain** (logic) | 512Hz fixed | Fixed-timestep simulation coordinator, job distribution |
| **Encoder** (render) | Variable | GPU upload, compute dispatch, graphics encoding |

On an 8-core CPU: 1 Sentinel + 1 Brain + 1 Encoder = **5 remaining cores** as workers. Because Brain and Encoder steal from the job queues while waiting, you get ~6 effective cores for logic and ~6 for render — roughly 20% throughput gain at no extra cost.

### Why Three Threads, Not Two

Two threads (logic + render) is the conventional split. The problem: the render thread holding a VSync lock also holds the slab read lock, which blocks the logic thread. Sentinel runs at 1000Hz and is never blocked by physics or GPU — input is always current.

A fourth dedicated physics thread was considered and rejected. Jolt's internal job system already parallelizes physics work across the worker pool. A dedicated physics thread would steal a core the workers use more effectively. Brain acts as a physics coordinator: it submits Jolt jobs and steals from the physics queue while waiting.

---

## Job System

### Four Priority Queues

| Queue | Producer | Consumer | Purpose |
|---|---|---|---|
| Logic Queue | Brain | All workers | PrePhysics/PostPhysics per-chunk jobs |
| Render Queue | Encoder | All workers | GPU upload, compute dispatch |
| Physics Queue | Jolt adapter | Workers (25% dedicated by default) | Jolt solver steps |
| General Queue | Any | Any | Everything else + overflow |

Queue implementation: lock-free MPMC Vyukov ring buffers. Worker wake: futex-based (`std::atomic::wait`/`notify`) — zero idle CPU, ~1–2μs wake latency.

### Core-Aware Pinning

Worker threads are pinned to physical cores first, SMT siblings second. Core 0 is skipped (OS and interrupt affinity). This reduces cache pollution from hyperthreading on hot paths.

---

## Brain Thread Tick Sequence

The Brain thread runs the full fixed-timestep loop. Wide sweeps (per-archetype SIMD jobs dispatched to workers) and Construct scalar batches interleave at defined points:

```
┌─ Fixed step (1.95ms budget) ─────────────────────────────────┐
│                                                               │
│  ProcessSimInput              (inject player inputs)         │
│                                                               │
│  ─── PrePhysics ──────────────────────────────────────────── │
│  Wide sweep     (per-archetype SIMD jobs → worker pool)      │
│  ScalarPrePhysics batch  (Construct hooks, scalar)           │
│                                                               │
│  ─── Physics Window ──────────────────────────────────────── │
│  FlushPendingBodies       (on physics tick boundary)         │
│  PushKinematicTransforms  (on physics tick boundary)         │
│  ScalarPhysicsStep batch  (Construct hooks — drive JoltChar) │
│  Jolt Step (async, via JoltJobSystemAdapter)                  │
│  PullActiveTransforms     (awake bodies only, next boundary)  │
│                                                               │
│  ─── PostPhysics ─────────────────────────────────────────── │
│  Wide sweep     (per-archetype SIMD jobs)                    │
│  ScalarPostPhysics batch  (Construct hooks)                  │
│                                                               │
│  PublishCompletedFrame                                       │
│  PropagateFrame                                              │
└───────────────────────────────────────────────────────────── ┘

(Between fixed steps, at variable rate:)
  Wide sweep ScalarUpdate
  Construct ScalarUpdate batch   (camera, UI, post-logic)
```

**Wide sweeps** process Entities (the horde) with 8-wide AVX2 SIMD. **Construct batches** process singular OOP objects (Player, GameMode, AIDirector) with scalar dispatch. The `Entity ScalarUpdate` slot is the bridge point: an AIDirector Construct thinks once per frame, then writes target positions into zombie entity fields. The zombies follow the data in the wide PrePhysics sweep.

### Physics Divisor

Jolt runs at a configurable fraction of the logic rate. Default: 512Hz logic / 8 = 64Hz physics. The ratio is configurable — games needing higher physics accuracy can set it lower.

- Physics entities are pushed into Jolt on `(currentFrame % PhysicsDivizor == 0)`
- Transforms are pulled from Jolt on `(currentFrame % PhysicsDivizor == PhysicsDivizor - 1)`
- The Brain thread helps with physics jobs during the wait window

---

## Encoder Thread

The Encoder runs independently of the Brain thread's fixed-rate loop. It polls for completed logic frames, copies dirty entity data from the temporal slab, and issues GPU compute and draw commands.

Key design: **5 PersistentMapped GPU InstanceBuffers** cycle independently of the 2 in-flight GPU frame slots. This breaks the latency chain: `VSync → GPU holds buffer → render thread blocks → holds slab read lock → logic stalls`. With 5 buffers, the render thread always finds a free buffer to write into. Logic and render are fully decoupled.

---

## Sentinel Thread

The Sentinel thread runs the SDL event loop at 1000Hz and owns:

- Input polling (double-buffered, lock-free)
- Vulkan object lifetime (VulkanContext, VulkanMemory)
- Audio system updates (at configurable rate, default 250Hz)
- Frame pacing (sleep + busy-wait tail for precise timing)
- FlowManager drain of network flow events

The Sentinel is never blocked by physics or GPU work. Input polling latency is constant regardless of simulation complexity.

---

## Thread Access to Slab Data

| Thread | Reads | Writes |
|---|---|---|
| Brain (Logic) | None; works with copied T+1 | Frame T+1 (WriteArray) |
| Encoder (Render) | Frame T (ReadArray) | GPU InstanceBuffer |
| GPU | Previous InstanceBuffer | Current InstanceBuffer (interpolation) |
| Net/Rollback | Historical frames from Temporal ring | Corrected frame (triggers dirty resim) |

The slab read lock is held only during entity state copy (~32µs at 10K dirty entities), not during dirty bit coalescing or GPU dispatch.
