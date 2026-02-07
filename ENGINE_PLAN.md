# StrigidEngine Development Plan

## Week 4 - Basic Rendering Pipeline ✅ **COMPLETE**

### Completed
- ✓ SDL3 GPU rendering pipeline setup
- ✓ Cube mesh with vertex/index buffers
- ✓ Instanced rendering (100,000 cubes @ 130 FPS!)
- ✓ Vertex and fragment shaders (SPIR-V)
- ✓ Perspective projection matrix
- ✓ Per-instance colors
- ✓ Rotation animation with varying speeds
- ✓ Performance profiling with Tracy (3-level system)
- ✓ Persistent instance buffer optimization

---

## Week 5 - ECS Integration ✅ **COMPLETE**

### Completed
- ✓ All rendering driven by ECS entities
- ✓ Transform component (position, rotation, scale)
- ✓ ColorData component
- ✓ Velocity component
- ✓ RenderSystem queries entities and builds instance data
- ✓ Update lifecycle system with reflection
- ✓ Entity caching optimization
- ✓ 100,000+ entities managed by ECS
- ✓ Performance exceeds 60fps+ target (130 FPS achieved)

---

## Week 6 - Rendering Fundamentals & Input (CURRENT)

### Goals
Complete core rendering features and add basic input/camera control

### Rendering Tasks
- 🔲 Implement depth buffer (Z-fighting currently visible)
- 🔲 Add view matrix (camera position/orientation)
- 🔲 Basic lighting (simple directional or point light)
- 🔲 Proper normal calculations for cubes
- 🔲 Clean up shader code for model readiness

### Input & Camera Tasks
- 🔲 Formalize input system (keyboard, mouse)
- 🔲 Input state tracking (pressed, held, released)
- 🔲 Camera component
- 🔲 FPS camera controller (WASD movement, mouse look)
- 🔲 Camera projection updates (FOV, aspect ratio)

### Success Criteria
- Depth rendering works correctly
- Can move camera around the scene
- Lighting makes 3D shapes clearly visible
- Ready for loading real 3D models

---

## Week 7 - Asset Pipeline

### Goals
Load real 3D models and textures instead of hardcoded cubes

### Tasks
- Integrate asset loading library (Assimp or cgltf)
- Load OBJ or glTF models
- Create Mesh abstraction (vertex format, buffers)
- Implement texture loading (stb_image)
- Create Material system
- Resource manager with reference counting
- Hot-reload support for development

### Success Criteria
- Can load and render arbitrary 3D models
- Textures display correctly
- Assets cached and managed efficiently

---

## Week 8 - Physics & Movement Systems

### Goals
Add basic physics and interesting movement behaviors

### Tasks
- Create Physics system (apply velocity, gravity)
- Add simple collision detection (AABB or sphere)
- Implement spatial partitioning (grid or octree)
- Add bounce/wrap behavior at world boundaries
- Create different movement patterns (orbit, sine wave, random walk)
- Frustum culling for off-screen entities

### Success Criteria
- Objects move and collide realistically
- No performance degradation with movement/physics
- Culling significantly improves performance

---

## Week 9 - Advanced Rendering

### Goals
Improve visual quality with modern rendering techniques

### Tasks
- Implement PBR materials (metallic/roughness)
- Add multiple light types (directional/point/spot)
- Shadow mapping (basic directional shadows)
- Post-processing framework
- Add bloom, tonemapping, gamma correction
- Render graph architecture for complex passes

### Success Criteria
- Visually compelling lighting
- Shadows look correct
- Stable 60fps with full effects

---

## Week 10 - Advanced Input & Interaction

### Goals
Advanced interaction and debug visualization

### Tasks
- Input binding/mapping system
- Gamepad support
- Additional camera modes (orbit, free-fly)
- Object picking (ray casting)
- Gizmos for transform manipulation
- Debug visualization (wireframe, normals, bounds, colliders)

### Success Criteria
- Multiple camera modes work well
- Can select and manipulate objects
- Debug views aid development

---

## Week 11 - Audio System

### Goals
Add sound effects and music

### Tasks
- Integrate SDL3 audio or OpenAL
- Audio clip loading (WAV, OGG)
- 3D positional audio
- Audio mixer with volume control
- Music streaming
- Audio component for ECS

### Success Criteria
- Sounds play at correct world positions
- Music loops smoothly
- Volume and mixing work correctly

---

## Week 12 - UI/HUD System

### Goals
Render on-screen UI elements

### Tasks
- Immediate mode UI (Dear ImGui) or custom system
- 2D sprite rendering
- Text rendering (font atlas)
- UI components (buttons, sliders, panels)
- HUD overlay (FPS, debug info)
- In-world UI (health bars, labels)

### Success Criteria
- Can create menus and HUD
- UI renders over 3D scene
- Performance impact is minimal

---

## Week 13 - Serialization & Scenes

### Goals
Save and load game state

### Tasks
- Scene format (JSON or binary)
- Entity serialization
- Component serialization
- Asset references
- Scene loading/unloading
- Multiple scene support
- Prefab system

### Success Criteria
- Can save entire scene to disk
- Loading restores exact state
- Scenes can be swapped at runtime

---

## Week 14 - Architecture Refinement & Testing

### Goals
Improve code quality, refactor for maintainability, establish automated testing

### Tasks - Base Entity Classes & Concepts
- Create EntityBase, TransformEntity, RenderableEntity hierarchy
- Introduce C++20 concepts for entity validation
  - `HasSchema<T>` - Entity must have DefineSchema()
  - `Updatable<T>` - Entity has Update() method
  - `Transformable<T>` - Entity has transform component
- Refactor existing entities to use base classes
- Better compile errors through concepts
- Reduce boilerplate for common entity types

### Tasks - Automated Test Harness
- Create separate StrigidTests project
- Integrate test framework (Google Test / Catch2 / Doctest)
- Migrate manual tests to automated tests
  - Unit tests (ECS, components, lifecycle)
  - Integration tests (rendering, physics, systems)
  - Performance benchmarks (100k entities, frame time targets)
  - Regression tests (ensure bugs don't return)
- Add CMake integration with CTest
- Optional: CI/CD pipeline (GitHub Actions)
- Create Sandbox project for engine demos/examples

### Success Criteria
- All entities use base classes with concepts
- Compile errors are clear and helpful
- 90%+ test coverage for core systems
- Automated tests run on every build
- Performance baselines established (prevent regressions)

---

## Future Weeks - Game Features

### Potential Areas
- Scripting system (Lua or custom)
- Particle effects
- Animation system (skeletal, blend trees)
- AI/pathfinding
- Networking (multiplayer)
- Editor tools (scene editor)
- Build pipeline (asset packaging)
- Platform support (console ports)
- Job system for multithreading
- Custom allocators and memory pools

---

## Architecture Notes

### Core Principles
- Data-Oriented Design throughout
- Cache-friendly memory layouts
- Batch operations where possible
- Explicit lifetimes and ownership
- Profile-driven optimization

### Key Systems
- **ECS**: Entity-component-system for game objects
- **Rendering**: SDL3 GPU API, modern graphics
- **Physics**: Custom or integrated (Jolt, PhysX)
- **Assets**: Streaming, hot-reload, reference counting
- **Memory**: Custom allocators, pooling, arena allocation

### Performance Targets (Exceeded!)
- ✓ 60 FPS minimum (achieved 130 FPS)
- ✓ 10,000+ entities (achieved 100,000 entities)
- ✓ Sub-millisecond system updates (~4ms for updates)
- ✓ Minimal memory allocations per frame

---

## Current Status (Week 6)
✅ Weeks 4-5 complete - ECS-driven rendering with 100k entities @ 130 FPS
🎯 Next: Depth buffer, view matrix, camera controls, basic lighting
📊 Performance: Excellent - single-threaded, room for multithreading gains
🏗️ Architecture: Solid foundation with reflection-based lifecycle system
