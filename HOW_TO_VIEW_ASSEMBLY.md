# How to View Generated Assembly for Specific Functions

## Method 1: MSVC Assembly Listing (Recommended - No Extra Tools)

### Step 1: Add Compiler Flag to CMakeLists.txt

Add this near the top of your CMakeLists.txt:

```cmake
# Generate assembly listings
if(MSVC)
    add_compile_options(/FAcs)  # Generate .cod files with assembly + source
    # Or use /FA for just assembly
endif()
```

**Flag options:**
- `/FA` - Assembly only
- `/FAc` - Assembly + machine code
- `/FAs` - Assembly + source code
- `/FAcs` - Assembly + machine code + source code (most useful!)

### Step 2: Rebuild

```bash
cmake --build . --config Release
```

### Step 3: Find the .cod File

Look in your build directory:
```
build/
  StrigidEngine.dir/
    Release/
      StrigidEngine.cod   ← HERE!
```

### Step 4: Search for Your Function

Open `StrigidEngine.cod` and search for:
```
RenderFrame
```

You'll see something like:
```asm
; 226  :             for (uint32_t i = 0; i < entityCount; ++i)

  00000 test    r15d, r15d
  00003 je      SHORT $LN12@RenderFram

$LL4@RenderFram:

; 232  :                 InstanceData& inst = instances[instanceIdx++];

  00005 mov     rax, QWORD PTR instanceIdx$[rsp]
  0000a lea     rdx, QWORD PTR [rax+1]
  0000e mov     QWORD PTR instanceIdx$[rsp], rdx

; 233  :                 inst.PositionX = transforms[i].PositionX;

  00013 vmovss  xmm0, DWORD PTR [rbx+rax*4]
  00018 vmovss  DWORD PTR [rcx], xmm0

; 234  :                 inst.PositionY = transforms[i].PositionY;

  0001c vmovss  xmm1, DWORD PTR [rbx+rax*4+4]
  00022 vmovss  DWORD PTR [rcx+4], xmm1
```

**What to look for:**
- `vmovss` - Scalar float move (NOT vectorized)
- `vmovaps` / `vmovups` - 128-bit (4 floats) - SSE vectorized ✓
- `vmovapd` / `vmovupd` - 128-bit (2 doubles) - SSE vectorized ✓
- `vinsertf128` / `vbroadcastss` - 256-bit (8 floats) - AVX vectorized ✓✓
- Lots of individual loads/stores = not vectorized ✗

---

## Method 2: Visual Studio Disassembly Window (If Using VS IDE)

### Step 1: Set Breakpoint
Put a breakpoint in your `RenderFrame` function.

### Step 2: Run Debug Build
Start debugging (F5).

### Step 3: Open Disassembly
When breakpoint hits:
- Right-click in code window → "Go To Disassembly"
- Or: Debug → Windows → Disassembly (Alt+8)

**Pros:**
- Interactive, can step through assembly
- See registers in real-time

**Cons:**
- Debug build (not optimized)
- Need to run the program

---

## Method 3: Compiler Explorer (Godbolt) - Quick Experiments

### Step 1: Go to https://godbolt.org/

### Step 2: Paste Your Loop

```cpp
struct Transform {
    float PositionX, PositionY, PositionZ, _pad0;
    float RotationX, RotationY, RotationZ, _pad1;
    float ScaleX, ScaleY, ScaleZ, _pad2;
};

struct ColorData {
    float R, G, B, A;
};

struct InstanceData {
    float PositionX, PositionY, PositionZ;
    float RotationX, RotationY, RotationZ;
    float ScaleX, ScaleY, ScaleZ;
    float ColorR, ColorG, ColorB, ColorA;
};

void CopyLoop(InstanceData* __restrict instances,
              const Transform* __restrict transforms,
              const ColorData* __restrict colors,
              size_t count) {
    for (size_t i = 0; i < count; i++) {
        instances[i].PositionX = transforms[i].PositionX;
        instances[i].PositionY = transforms[i].PositionY;
        instances[i].PositionZ = transforms[i].PositionZ;
        instances[i].RotationX = transforms[i].RotationX;
        instances[i].RotationY = transforms[i].RotationY;
        instances[i].RotationZ = transforms[i].RotationZ;
        instances[i].ScaleX = transforms[i].ScaleX;
        instances[i].ScaleY = transforms[i].ScaleY;
        instances[i].ScaleZ = transforms[i].ScaleZ;
        instances[i].ColorR = colors[i].R;
        instances[i].ColorG = colors[i].G;
        instances[i].ColorB = colors[i].B;
        instances[i].ColorA = colors[i].A;
    }
}
```

### Step 3: Select Compiler
- Choose: `x64 msvc v19.latest`
- Add flags: `/O2 /arch:AVX2`

### Step 4: Analyze Output
Look at the assembly on the right side.

**Pros:**
- Instant feedback
- Try different optimizations
- Compare compiler versions

**Cons:**
- Simplified code (not your full codebase)
- Might behave differently than real build

---

## Method 4: objdump (Advanced)

### Step 1: Find Your .obj File

```bash
# In your build directory
dir /s StrigidEngine.obj
```

Look for: `build/StrigidEngine.dir/Release/StrigidEngine.obj`

### Step 2: Disassemble with dumpbin

```bash
# MSVC includes dumpbin.exe
dumpbin /disasm StrigidEngine.obj > disasm.txt

# Or for specific symbol:
dumpbin /disasm /symbols StrigidEngine.obj | findstr RenderFrame
```

### Step 3: View disasm.txt

Search for your function name.

---

## What You're Looking For: Vectorization Patterns

### NOT Vectorized (Bad)
```asm
; Each float moved individually
movss   xmm0, DWORD PTR [rcx]
movss   DWORD PTR [rdx], xmm0
movss   xmm1, DWORD PTR [rcx+4]
movss   DWORD PTR [rdx+4], xmm1
movss   xmm2, DWORD PTR [rcx+8]
movss   DWORD PTR [rdx+8], xmm2
; ... 13 separate moves per entity
```

### SSE Vectorized (Good)
```asm
; Load 4 floats at once (128-bit)
movaps  xmm0, XMMWORD PTR [rcx]      ; Load Position (3 floats + pad)
movaps  xmm1, XMMWORD PTR [rcx+16]   ; Load Rotation
movaps  xmm2, XMMWORD PTR [rcx+32]   ; Load Scale

; Store 4 floats at once
movaps  XMMWORD PTR [rdx], xmm0
movaps  XMMWORD PTR [rdx+16], xmm1
movaps  XMMWORD PTR [rdx+32], xmm2
```

### AVX Vectorized (Best)
```asm
; Load 8 floats at once (256-bit)
vmovups ymm0, YMMWORD PTR [rcx]      ; Load Position + Rotation (7 floats)
vmovups ymm1, YMMWORD PTR [rcx+28]   ; Load Scale + ColorR

; Store 8 floats at once
vmovups YMMWORD PTR [rdx], ymm0
vmovups YMMWORD PTR [rdx+28], ymm1
```

---

## Why Your Loop Might NOT Be Vectorizing

### Issue 1: Two Separate Arrays
```cpp
Transform* transforms = ...;  // Array 1
ColorData* colors = ...;      // Array 2

for (...) {
    inst.PositionX = transforms[i].PositionX;  // Load from array 1
    inst.ColorR = colors[i].R;                  // Load from array 2
}
```

**Compiler sees:** Different pointer bases, can't vectorize easily.

**Fix:** Use `__restrict` keyword to promise no aliasing:
```cpp
Transform* __restrict transforms = ...;
ColorData* __restrict colors = ...;
```

### Issue 2: Non-Aligned Structs
```cpp
struct InstanceData {
    float PositionX, PositionY, PositionZ;      // 12 bytes (misaligned!)
    float RotationX, RotationY, RotationZ;      // 12 bytes
    // ... 52 bytes total (not 16-byte aligned)
};
```

**Fix:** Pad to 16 or 64 bytes:
```cpp
struct alignas(64) InstanceData {
    float PositionX, PositionY, PositionZ, _pad0;  // 16 bytes
    float RotationX, RotationY, RotationZ, _pad1;  // 16 bytes
    float ScaleX, ScaleY, ScaleZ, _pad2;           // 16 bytes
    float ColorR, ColorG, ColorB, ColorA;          // 16 bytes
    // 64 bytes total (cache-line aligned!)
};
```

### Issue 3: Complex Loop Body
If compiler sees:
- Function calls inside loop
- Pointer aliasing
- Unpredictable branches
- Non-unit stride access

It might give up on vectorization.

---

## Quick Test: Enable Vectorization Reports

Add to CMakeLists.txt:

```cmake
if(MSVC)
    add_compile_options(/Qvec-report:2)  # Report vectorization decisions
    add_compile_options(/FAcs)           # Generate assembly listing
endif()
```

**Build output will show:**
```
StrigidEngine.cpp(226): info: loop vectorized
StrigidEngine.cpp(240): info: loop not vectorized: no obvious induction variable
```

---

## Your Current Status

**180 FPS (38% gain from push_back removal)** suggests:
- Compiler is doing SOMETHING better
- Probably not fully vectorized (or you'd see 2-4x gain)
- Likely getting better instruction scheduling and fewer memory barriers

**To confirm:**
1. Add `/FAcs` flag to CMakeLists.txt
2. Rebuild Release
3. Open `StrigidEngine.cod`
4. Search for `RenderFrame`
5. Look at the loop - count `movss` vs `movaps`/`vmovups`

If you see lots of `movss` (scalar moves), it's not vectorized.
If you see `movaps`/`vmovups`, it IS vectorized!

---

## Next Steps

**Want to force vectorization?**

Try this version (I'll make the changes if you want):

```cpp
// Add __restrict to promise no aliasing
Transform* __restrict transforms = arch->GetComponentArray<Transform>(...);
ColorData* __restrict colors = arch->GetComponentArray<ColorData>(...);
InstanceData* __restrict output = &instances[instanceIdx];

// Explicit loop count (helps compiler)
const uint32_t N = entityCount;

// Manual unrolling hint
#pragma loop(hint_parallel(4))
for (uint32_t i = 0; i < N; ++i) {
    output[i].PositionX = transforms[i].PositionX;
    output[i].PositionY = transforms[i].PositionY;
    output[i].PositionZ = transforms[i].PositionZ;
    output[i].RotationX = transforms[i].RotationX;
    output[i].RotationY = transforms[i].RotationY;
    output[i].RotationZ = transforms[i].RotationZ;
    output[i].ScaleX = transforms[i].ScaleX;
    output[i].ScaleY = transforms[i].ScaleY;
    output[i].ScaleZ = transforms[i].ScaleZ;
    output[i].ColorR = colors[i].R;
    output[i].ColorG = colors[i].G;
    output[i].ColorB = colors[i].B;
    output[i].ColorA = colors[i].A;
}
```

Let me know what you find in the assembly! 🔍
