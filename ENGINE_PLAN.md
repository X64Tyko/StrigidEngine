# StrigidEngine Development Plan

## Week 4 - Basic Rendering Pipeline ✓ (CURRENT)

### Completed
- ✓ SDL3 GPU rendering pipeline setup
- ✓ Cube mesh with vertex/index buffers
- ✓ Instanced rendering (1000 cubes)
- ✓ Vertex and fragment shaders (SPIR-V)
- ✓ Perspective projection matrix
- ✓ Per-instance colors
- ✓ Rotation animation with varying speeds

### Remaining
- 🔲 Camera controls (WASD movement, mouse look)
- 🔲 Depth buffer support
- 🔲 View matrix (currently using identity)
- 🔲 Performance profiling with Tracy

---

## Week 5 - ECS Integration

### Goals
Wire up the existing ECS system to drive rendering

### Tasks
- Replace hardcoded test instances with ECS entities
- Create Transform component (position, rotation, scale)
- Create Renderable component (mesh, material)
- Create RenderSystem that queries entities and builds instance data
- Update StrigidEngine::RenderFrame to use ECS queries
- Test with 1000+ entities managed by ECS

### Success Criteria
- All rendered objects are ECS entities
- Can add/remove entities at runtime
- Performance remains 60fps+

---

## Week 6 - Physics & Movement Systems

### Goals
Add basic physics and interesting movement behaviors

### Tasks
- Create Velocity component
- Create Physics system (apply velocity, gravity)
- Add simple collision detection (AABB or sphere)
- Implement spatial partitioning (grid or octree)
- Add bounce/wrap behavior at world boundaries
- Create different movement patterns (orbit, sine wave, random walk)

### Success Criteria
- Objects move and collide realistically
- No performance degradation with 1000+ moving objects
- Clean separation between physics and rendering

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

## Week 8 - Advanced Rendering

### Goals
Improve visual quality with modern rendering techniques

### Tasks
- Implement proper lighting (Phong or PBR)
- Add directional/point/spot lights
- Shadow mapping (basic cascaded or single)
- Post-processing framework
- Add bloom, tonemapping, gamma correction
- Render graph architecture for complex passes
- Multiple render targets

### Success Criteria
- Visually compelling lighting
- Shadows look correct
- Stable 60fps with full effects

---

## Week 9 - Input & Interaction

### Goals
Make the engine interactive

### Tasks
- Formalize input system (keyboard, mouse, gamepad)
- Input binding/mapping system
- Camera controller (FPS, orbit, free-fly)
- Object picking (ray casting)
- Gizmos for transform manipulation
- Debug visualization (wireframe, normals, bounds)

### Success Criteria
- Responsive camera controls
- Can select and manipulate objects
- Debug views aid development

---

## Week 10 - Audio System

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

## Week 11 - UI/HUD System

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

## Week 12 - Serialization & Scenes

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

### Performance Targets
- 60 FPS minimum
- 10,000+ entities
- Sub-millisecond system updates
- Minimal memory allocations per frame

---

## Current Status (Week 4)
✅ Basic rendering works - 1000 rotating cubes at 60fps
🎯 Next: Camera controls and depth buffer
📊 Performance: Solid, Tracy integrated for profiling
