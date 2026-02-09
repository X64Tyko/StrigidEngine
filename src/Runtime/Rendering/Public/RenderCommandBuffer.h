#pragma once

#include <cstdint>
#include <atomic>

#include "Logger.h"

// Disable warning for flexible array member (C99 feature used in C++)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)  // nonstandard extension used: zero-sized array in struct/union
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
#pragma warning(disable : 4815)  // zero-sized array in stack object will have no elements
#pragma warning(disable : 4324)  // structure was padded due to alignment specifier
#endif



// Instance data format for GPU upload
// Aligned to 16 bytes for SIMD vectorization and GPU alignment
struct alignas(16) InstanceData
{
    float PositionX, PositionY, PositionZ, _pad0;     // 16 bytes (was 12)
    float RotationX, RotationY, RotationZ, _pad1;     // 16 bytes (was 12)
    float ScaleX, ScaleY, ScaleZ, _pad2;              // 16 bytes (was 12)
    float ColorR, ColorG, ColorB, ColorA;             // 16 bytes (same)
};
static_assert(sizeof(InstanceData) == 64, "InstanceData must be 64 bytes for optimal vectorization");


/**
 * Render Command Types
 * Embedded in header to identify command type
 */
enum class RenderCommandType : uint8_t {
    FrameStart,      // Marks beginning of frame commands
    DrawInstanced,   // Instanced draw call with embedded instance data
    FrameEnd,        // Marks end of frame, triggers present
    Wrap,           // Tells the Tail that it should reset to the beginning of the buffer
};

int operator<<(RenderCommandType lhs, int rhs);

/**
 * Base Render Command
 * All render commands inherit from this to provide consistent header access
 */
struct alignas(16) RenderCommand {
    union {
        alignas(uint16_t) uint32_t Value;
        struct {
            uint32_t Finished : 1;   // Command finished writing flag
            uint32_t Type     : 7;   // RenderCommandType
            uint32_t Count    : 24;  // Command-specific count (instances, bytes, etc)
        };
    } Header;

    RenderCommandType GetType() const {
        return static_cast<RenderCommandType>(Header.Type);
    }

    bool GetCommandFinished() const {
        return Header.Finished;
    }

    uint32_t GetCount() const {
        return Header.Count;
    }

    void SetTypeAndCount(RenderCommandType type, uint32_t count, bool finished = false) {
        Header.Type = static_cast<uint8_t>(type);
        Header.Count = count;
        Header.Finished = finished;
    }

    void SetCommandFinished() {
        Header.Finished = 1;
    }
};

/**
 * Draw Instanced Command
 * Instanced draw call with embedded instance data
 * Followed immediately by variable-length InstanceData array
 */
struct DrawInstancedCommand : RenderCommand {
    InstanceData instances[];  // Flexible array member - actual data follows
};

/**
 * Lock-Free Ring Buffer for Render Commands
 *
 * Main thread writes commands to head, render thread reads from tail.
 * Commands are variable-size and can wrap around buffer boundary.
 *
 * Memory Layout:
 *   [FrameStart:1byte][DrawInstanced:4+N*64bytes][FrameEnd:1byte]
 *
 * Frame Overwrite Strategy:
 *   If main thread catches up to previous frame (render hasn't consumed yet),
 *   rewind head to lastFrameHead and overwrite old commands.
 */
class RenderCommandBuffer {
public:
    // Buffer size: 32 MB
    // Conservative sizing for ~3 frames of 100k entities (6.4MB each)
    static constexpr size_t MAX_BUFFER_BYTES = 32 * 1024 * 1024;
     /**
     * Buffer Resizing:
     *   TODO: If single-frame command data exceeds 30% of buffer size,
     *   consider mutex-locked reallocation with size doubling strategy.
     *   This prevents pathological cases where large draws can't fit. */

    RenderCommandBuffer();
    ~RenderCommandBuffer();

    void WrapCommandBuffer(uint32_t current, uint32_t next, uint8_t*& outWrapPtr, uint32_t& outWrapAfter);
    /**
     * Allocate space for a command in the ring buffer
     *
     * @param type Command type (for FrameStart handling)
     * @param dataSize Total size of command data in bytes
     * @param outWrapPtr If command wraps, points to start of buffer for continuation
     * @param outWrapAfter If command wraps, number of bytes to write at wrap location
     * @return Pointer to allocated space in buffer (caller writes command data here)
     *
     * Usage:
     *   void* wrapPtr;
     *   uint32_t wrapAfter;
     *   auto* cmd = buffer.AllocateCommand<DrawInstancedCommand>(
     *       RenderCommandType::DrawInstanced,
     *       sizeof(DrawInstancedCommand) + sizeof(InstanceData) * count,
     *       wrapPtr,
     *       wrapAfter
     *   );
     *
     *   cmd->SetTypeAndCount(RenderCommandType::DrawInstanced, count);
     *
     *   if (!wrapPtr) {
     *       memcpy(cmd->instances, data, size);
     *   } else {
     *       // Handle wrap: copy partial data, then remainder to wrapPtr
     *   }
     */
    template<typename T>
    T* AllocateCommand(RenderCommandType type, uint32_t dataSize, uint8_t*& outWrapPtr, uint32_t& outWrapAfter);
    
    RenderCommand* GetCommand(uint8_t*& outWrapPtr, uint32_t& outWrapAfter);
    
    bool IsPreviousFrameInProgress() const;
    
    void CommitCommand(size_t dataSize);

    /**
     * Get current tail position (render thread reads from here)
     */
    uint32_t GetTail() const { return tail.load(std::memory_order_relaxed); }

    /**
     * Get current head position (main thread writes to here)
     */
    uint32_t GetHead() const { return head.load(std::memory_order_relaxed); }

    /**
     * Get current head position (main thread writes to here)
     */
    uint32_t GetLastFrameHead() const { return lastFrameHead.load(std::memory_order_relaxed); }

    /**
     * Advance tail (render thread consumes commands)
     */
    void AdvanceTail(uint32_t newTail) { tail.store(newTail, std::memory_order_release); }

    /**
     * Get raw buffer pointer (for reading commands)
     */
    const uint8_t* GetBuffer() const { return buffer; }
    uint8_t* GetBuffer() { return buffer; }

private:
    bool IsInRange(uint32_t value, uint32_t start, uint32_t end) const;

    uint8_t* buffer;
    std::atomic<uint32_t> tail;          // Render thread reads from here (byte offset)
    std::atomic<uint32_t> head;          // Main thread writes to here (byte offset)
    std::atomic<uint32_t> lastFrameHead; // Head position at last FrameStart
};

// Template implementation
template<typename T>
T* RenderCommandBuffer::AllocateCommand(RenderCommandType type, uint32_t dataSize, uint8_t*& outWrapPtr, uint32_t& outWrapAfter) {

    // Frame boundary check: if starting new frame and render hasn't consumed previous frame, rewind
    if (type == RenderCommandType::FrameStart) {
        //uint32_t last = lastFrameHead.load(std::memory_order_relaxed);
        //uint32_t currentTail = tail.load(std::memory_order_acquire);
        uint32_t currentHead = head.load(std::memory_order_relaxed);

        /*
        if (!IsInRange(currentTail, last, currentHead)) {
            // Render thread hasn't consumed previous frame, overwrite it
            head.store(last, std::memory_order_release);
        }
        else
        {*/
            if (currentHead + sizeof(RenderCommand) > MAX_BUFFER_BYTES)
            {
                // command header won't fit, wrap.
                currentHead = 0;
                head.store(currentHead, std::memory_order_relaxed);
            }
            
            // Render caught up, update lastFrameHead to current position
            lastFrameHead.store(currentHead, std::memory_order_relaxed);
        //}
    }

    uint32_t current = head.load(std::memory_order_relaxed);

    WrapCommandBuffer(current, current + dataSize, outWrapPtr, outWrapAfter);

    // CRITICAL: Clear the finished flag IMMEDIATELY to prevent render thread
    // from reading this command before we've written the data
    T* cmd = reinterpret_cast<T*>(&buffer[current]);
    cmd->Header.Finished = 0;

    return cmd;
}

// Re-enable warnings
#ifdef _MSC_VER
#pragma warning(pop)
#endif
