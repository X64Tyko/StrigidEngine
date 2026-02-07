# Reading Assembly - Quick Guide for Your RenderFrame Loop

## Step 1: Find Your Loop in the .cod File

Open `StrigidEngine.cod` and search for:
```
for (uint32_t i = 0; i < entityCount
```

Or search for the line number like:
```
; 230
```

You'll see something like this (the actual code with line numbers).

## Step 2: What to Look For (TL;DR Version)

**Just answer these 3 questions:**

### Question 1: How are floats being moved?

Look at the instructions in the loop body:

**❌ NOT Vectorized (slow):**
```asm
movss   xmm0, DWORD PTR [rcx]           ; Load 1 float
movss   DWORD PTR [rdx], xmm0           ; Store 1 float
movss   xmm1, DWORD PTR [rcx+4]         ; Load 1 float
movss   DWORD PTR [rdx+4], xmm1         ; Store 1 float
; ... 13 separate movss instructions per entity
```
**Clue:** `movss` = move scalar single (1 float at a time)

**✅ SSE Vectorized (good):**
```asm
movaps  xmm0, XMMWORD PTR [rcx]         ; Load 4 floats (16 bytes)
movaps  XMMWORD PTR [rdx], xmm0         ; Store 4 floats
movaps  xmm1, XMMWORD PTR [rcx+16]      ; Load 4 more floats
movaps  XMMWORD PTR [rdx+16], xmm1      ; Store 4 more floats
; ... ~4 movaps per entity
```
**Clue:** `movaps` = move aligned packed single (4 floats at once)

**✅✅ AVX Vectorized (best):**
```asm
vmovups ymm0, YMMWORD PTR [rcx]         ; Load 8 floats (32 bytes)
vmovups YMMWORD PTR [rdx], ymm0         ; Store 8 floats
vmovups ymm1, YMMWORD PTR [rcx+32]      ; Load 8 more floats
vmovups YMMWORD PTR [rdx+32], ymm1      ; Store 8 more floats
; ... ~2 vmovups per entity
```
**Clue:** `vmovups` with `ymm` registers = 8 floats at once

### Question 2: How many instructions per entity?

Count the instructions between loop iterations:

**Bad (NOT vectorized):**
- 13+ `movss` instructions (one per float field)
- Total: ~30-40 instructions per entity

**Good (Vectorized):**
- 3-4 `movaps`/`vmovups` instructions
- Total: ~10-15 instructions per entity

### Question 3: Do you see a loop?

Look for the loop control:

```asm
$LL4@RenderFram:                    ; ← Loop label
    ; ... instructions ...
    add     rdi, 52                  ; ← Increment pointer (52 = sizeof(InstanceData))
    add     rsi, 48                  ; ← Increment transform pointer
    add     r8, 16                   ; ← Increment color pointer
    sub     r9, 1                    ; ← Decrement counter
    jne     SHORT $LL4@RenderFram   ; ← Jump if not zero (loop back)
```

**If the loop is unrolled**, you might see:
```asm
; Process entity 0
movaps  xmm0, [rcx]
movaps  [rdx], xmm0
; Process entity 1
movaps  xmm1, [rcx+52]
movaps  [rdx+52], xmm1
; Process entity 2
movaps  xmm2, [rcx+104]
movaps  [rdx+104], xmm2
; Process entity 3
movaps  xmm3, [rcx+156]
movaps  [rdx+156], xmm3
; Then loop
```
**Clue:** Same instructions repeated 2-4 times before the loop jump = unrolled

---

## Quick Cheat Sheet

### Instruction Decoder

| Instruction | Meaning | Good/Bad |
|-------------|---------|----------|
| `movss` | Move 1 float | ❌ Not vectorized |
| `movaps` | Move 4 floats (aligned) | ✅ SSE vectorized |
| `movups` | Move 4 floats (unaligned) | ✅ SSE vectorized |
| `vmovaps` | Move 4 floats (AVX) | ✅ AVX vectorized |
| `vmovups` | Move 4/8 floats (AVX) | ✅ AVX vectorized |
| `vmovups ymm` | Move 8 floats | ✅✅ Full AVX |

### Register Names

| Register | Size | Type |
|----------|------|------|
| `xmm0-15` | 128-bit | SSE (4 floats or 2 doubles) |
| `ymm0-15` | 256-bit | AVX (8 floats or 4 doubles) |
| `zmm0-31` | 512-bit | AVX-512 (16 floats) |
| `rax, rbx, rcx...` | 64-bit | General purpose (pointers/integers) |

---

## Example: How to Read Your Specific Loop

Let's say you find this in the .cod file:

```asm
; 230  :             for (uint32_t i = 0; i < entityCount; ++i)

  000000 test    r9d, r9d                   ; Test if entityCount == 0
  000003 je      SHORT $LN8@RenderFram      ; Jump to end if zero

$LL4@RenderFram:                            ; ← LOOP START

; 232  :                 InstanceData& inst = instances[instanceIdx++];

  000005 mov     rax, QWORD PTR instanceIdx$[rsp]
  00000a lea     rdx, QWORD PTR [rax+1]
  00000e mov     QWORD PTR instanceIdx$[rsp], rdx

; 233  :                 inst.PositionX = transforms[i].PositionX;
; 234  :                 inst.PositionY = transforms[i].PositionY;
; 235  :                 inst.PositionZ = transforms[i].PositionZ;

  000013 vmovss  xmm0, DWORD PTR [rbx]      ; Load PositionX (1 float)
  000018 vmovss  DWORD PTR [rcx], xmm0      ; Store PositionX
  00001c vmovss  xmm1, DWORD PTR [rbx+4]    ; Load PositionY (1 float)
  000022 vmovss  DWORD PTR [rcx+4], xmm1    ; Store PositionY
  000026 vmovss  xmm2, DWORD PTR [rbx+8]    ; Load PositionZ (1 float)
  00002c vmovss  DWORD PTR [rcx+8], xmm2    ; Store PositionZ

; ... more vmovss instructions for Rotation and Scale ...

; 238  :                 inst.ColorR = colors[i].R;
; 239  :                 inst.ColorG = colors[i].G;
; 240  :                 inst.ColorB = colors[i].B;
; 241  :                 inst.ColorA = colors[i].A;

  000060 vmovss  xmm10, DWORD PTR [rdi]     ; Load ColorR
  000065 vmovss  DWORD PTR [rcx+36], xmm10  ; Store ColorR
  00006b vmovss  xmm11, DWORD PTR [rdi+4]   ; Load ColorG
  000071 vmovss  DWORD PTR [rcx+40], xmm11  ; Store ColorG
  ; ... etc

; Loop control
  000090 add     rbx, 48                     ; transforms++ (48 = sizeof(Transform))
  000094 add     rdi, 16                     ; colors++ (16 = sizeof(ColorData))
  000098 add     rcx, 52                     ; instances++ (52 = sizeof(InstanceData))
  00009c sub     r9d, 1                      ; i--
  0000a0 jne     SHORT $LL4@RenderFram      ; ← LOOP BACK if not done

$LN8@RenderFram:                            ; ← LOOP END
```

**Reading this:**
1. ❌ All `vmovss` (scalar moves) - NOT vectorized
2. ❌ 13 separate move instructions per entity
3. ✅ Loop is there and working correctly
4. 📊 **Result:** Compiler is NOT vectorizing this loop

---

## What GOOD Vectorization Would Look Like

If the compiler vectorized it, you'd see:

```asm
$LL4@RenderFram:                            ; ← LOOP START

; Load Transform (48 bytes = 3x 16-byte chunks)
  vmovaps xmm0, XMMWORD PTR [rbx]           ; Load Position (12 bytes + 4 pad)
  vmovaps xmm1, XMMWORD PTR [rbx+16]        ; Load Rotation (12 bytes + 4 pad)
  vmovaps xmm2, XMMWORD PTR [rbx+32]        ; Load Scale (12 bytes + 4 pad)

; Load Color (16 bytes = 1x 16-byte chunk)
  vmovaps xmm3, XMMWORD PTR [rdi]           ; Load ColorRGBA (16 bytes)

; Store all at once
  vmovaps XMMWORD PTR [rcx], xmm0           ; Store Position
  vmovaps XMMWORD PTR [rcx+12], xmm1        ; Store Rotation (misaligned!)
  vmovaps XMMWORD PTR [rcx+24], xmm2        ; Store Scale
  vmovaps XMMWORD PTR [rcx+36], xmm3        ; Store Color

; Loop control
  add     rbx, 48                            ; transforms++
  add     rdi, 16                            ; colors++
  add     rcx, 52                            ; instances++
  sub     r9d, 1                             ; i--
  jne     SHORT $LL4@RenderFram             ; Loop back

; Result: Only 4 vector moves per entity (vs 13 scalar moves)
```

**But wait!** Notice the misaligned stores? That's why it's probably NOT vectorizing - your `InstanceData` is 52 bytes (not 16-byte aligned).

---

## Why Your Loop Isn't Vectorizing (Likely)

### Problem: Misaligned Struct

```cpp
struct InstanceData {
    float PositionX, PositionY, PositionZ;      // 12 bytes
    float RotationX, RotationY, RotationZ;      // 12 bytes
    float ScaleX, ScaleY, ScaleZ;               // 12 bytes
    float ColorR, ColorG, ColorB, ColorA;       // 16 bytes
};
// Total: 52 bytes (NOT 16-byte aligned!)
```

**Compiler sees:**
- Position at offset 0 (aligned ✓)
- Rotation at offset 12 (misaligned ✗)
- Scale at offset 24 (misaligned ✗)
- Color at offset 36 (misaligned ✗)

**Compiler thinks:** "I can't use aligned vector loads/stores (`movaps`), and using unaligned (`movups`) for every field is slower than scalar. Give up."

---

## The Fix

Change `InstanceData` to be 64 bytes (cache-aligned):

```cpp
struct alignas(64) InstanceData {
    float PositionX, PositionY, PositionZ, _pad0;     // 16 bytes ✓
    float RotationX, RotationY, RotationZ, _pad1;     // 16 bytes ✓
    float ScaleX, ScaleY, ScaleZ, _pad2;              // 16 bytes ✓
    float ColorR, ColorG, ColorB, ColorA;             // 16 bytes ✓
};
// Total: 64 bytes (perfectly aligned!)
```

**Now compiler can:**
- Use `vmovaps` (fast aligned moves)
- Load/store 4 floats per instruction
- Reduce from 13 instructions to 4 per entity

**Expected assembly after fix:**
```asm
vmovaps xmm0, XMMWORD PTR [rbx]        ; Load Position (4 floats)
vmovaps xmm1, XMMWORD PTR [rbx+16]     ; Load Rotation (4 floats)
vmovaps xmm2, XMMWORD PTR [rbx+32]     ; Load Scale (4 floats)
vmovaps xmm3, XMMWORD PTR [rdi]        ; Load Color (4 floats)
vmovaps XMMWORD PTR [rcx], xmm0        ; Store all 4
vmovaps XMMWORD PTR [rcx+16], xmm1
vmovaps XMMWORD PTR [rcx+32], xmm2
vmovaps XMMWORD PTR [rcx+48], xmm3
```

Only 8 instructions (vs 26)!

---

## Quick Diagnostic

Just search the .cod file for your loop and count:

**Count `vmovss` in the loop:**
```bash
# Extract just your loop section
sed -n '/for (uint32_t i = 0; i < entityCount/,/jne.*RenderFram/p' StrigidEngine.cod > loop.asm

# Count scalar moves
grep -c "vmovss" loop.asm
```

**If you see:**
- 13+ `vmovss` = NOT vectorized ❌
- 3-4 `vmovaps` = Vectorized! ✅
- 2 `vmovups ymm` = Fully AVX vectorized! ✅✅

---

## TL;DR - What You Should Do Now

1. **Find your loop in the .cod file** (search for line 230)
2. **Count the move instructions:**
   - Lots of `movss` = bad
   - A few `movaps` = good
3. **If it's bad (vmovss everywhere):**
   - The 52-byte struct is the culprit
   - Pad it to 64 bytes
   - Rebuild and check again

Want me to help you pad the struct and measure the performance difference? That's probably a **20-30% speedup** if it starts vectorizing! 🚀
