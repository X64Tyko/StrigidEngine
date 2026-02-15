#include "RenderCommandBuffer.h"
#include <cstdlib>
#include <cstring>
#include <valarray>

#include "Logger.h"
#include "Profiler.h"

int operator<<(RenderCommandType lhs, int rhs)
{
    //LOG_DEBUG_F("cmdHeader: %i", static_cast<int>(lhs) << rhs);
    return static_cast<int>(lhs) << rhs;
}

RenderCommandBuffer::RenderCommandBuffer()
    : buffer(nullptr)
      , tail(0)
      , head(0)
      , lastFrameHead(0)
{
    // Allocate 32MB buffer
    buffer = static_cast<uint8_t*>(std::malloc(MAX_BUFFER_BYTES));

    if (!buffer)
    {
        // Handle allocation failure
        // TODO: Log error, throw exception, or handle gracefully
    }

    // Zero out buffer for safety
    std::memset(buffer, 0, MAX_BUFFER_BYTES);
}

RenderCommandBuffer::~RenderCommandBuffer()
{
    if (buffer)
    {
        std::free(buffer);
        buffer = nullptr;
    }
}

void RenderCommandBuffer::WrapCommandBuffer(uint32_t current, uint32_t next, uint8_t*& outWrapPtr,
                                            uint32_t& outWrapAfter)
{
    outWrapPtr = nullptr;
    outWrapAfter = 0;

    if (next > MAX_BUFFER_BYTES)
    {
        outWrapPtr = buffer;
        outWrapAfter = static_cast<uint32_t>(MAX_BUFFER_BYTES - current);
        LOG_DEBUG_F("[CmdBuffer] WRAP: current=%u, next=%u, wrapAfter=%u", current, next, outWrapAfter);
    }
}

RenderCommand* RenderCommandBuffer::GetCommand(uint8_t*& outWrapPtr, uint32_t& outWrapAfter)
{
    outWrapPtr = nullptr;
    outWrapAfter = 0;
    uint32_t currentHead = head.load(std::memory_order_acquire);
    uint32_t currentTail = tail.load(std::memory_order_relaxed);

    LOG_TRACE_F("[RenderThread] GetCommand: tail=%u, head=%u", currentTail, currentHead);

    if (currentTail == currentHead)
    {
        LOG_TRACE("[RenderThread] Buffer empty (tail == head)");
        return nullptr;
    }

    if (currentTail + sizeof(RenderCommand) > MAX_BUFFER_BYTES)
    {
        LOG_DEBUG_F("[RenderThread] Tail wrap: %u -> 0", currentTail);
        currentTail = 0;
        tail.store(currentTail, std::memory_order_relaxed);
    }

    STRIGID_ZONE_N("RenderThread_ProcessCommand");

    // Peek at command header using struct accessors (endian-safe)
    auto cmdHeader = reinterpret_cast<const RenderCommand*>(&buffer[currentTail]);

    // Check if command is finished being written
    if (!cmdHeader->GetCommandFinished())
    {
        LOG_TRACE_F("[RenderThread] Command at %u not finished (Finished=%u)", currentTail, cmdHeader->Header.Finished);
        return nullptr;
    }

    // Extract command type
    RenderCommandType cmdType = cmdHeader->GetType();
    LOG_DEBUG_F("[RenderThread] Processing command type=%u at tail=%u", static_cast<uint32_t>(cmdType), currentTail);

    switch (cmdType)
    {
    case RenderCommandType::FrameStart:
        {
            STRIGID_ZONE_N("RenderThread_FrameStart");
            LOG_DEBUG_F("[RenderThread] FrameStart at %u", currentTail);
            uint32_t newTail = currentTail + sizeof(RenderCommand);
            tail.store(newTail, std::memory_order_release);
            LOG_TRACE_F("[RenderThread] Tail advanced: %u -> %u", currentTail, newTail);
            return reinterpret_cast<RenderCommand*>(&buffer[currentTail]);
        }

    case RenderCommandType::DrawInstanced:
        {
            STRIGID_ZONE_N("RenderThread_DrawInstanced");

            // Read full command header to get instance count
            auto drawCmd = reinterpret_cast<DrawInstancedCommand*>(&buffer[currentTail]);
            uint32_t instanceCount = drawCmd->GetCount();

            LOG_DEBUG_F("[RenderThread] DrawInstanced: %u instances at tail=%u", instanceCount, currentTail);

            size_t cmdSize = sizeof(DrawInstancedCommand) + sizeof(InstanceData) * instanceCount;
            WrapCommandBuffer(currentTail, static_cast<uint32_t>(currentTail + cmdSize), outWrapPtr, outWrapAfter);

            if (outWrapPtr)
            {
                LOG_WARN_F("[RenderThread] DrawInstanced data WRAPS! tail=%u, cmdSize=%zu, wrapAfter=%u",
                           currentTail, cmdSize, outWrapAfter);
            }

            //LOG_ALWAYS_F("Current Color: B-%f R-%f", drawCmd->instances[0].ColorB, drawCmd->instances[0].ColorR);

            uint32_t newTail = (currentTail + static_cast<uint32_t>(cmdSize)) % MAX_BUFFER_BYTES;
            tail.store(newTail, std::memory_order_release);
            LOG_TRACE_F("[RenderThread] Tail advanced: %u -> %u (cmdSize=%zu)", currentTail, newTail, cmdSize);
            return drawCmd;
        }

    case RenderCommandType::FrameEnd:
        {
            STRIGID_ZONE_N("RenderThread_FrameEnd");
            LOG_DEBUG_F("[RenderThread] FrameEnd at %u", currentTail);
            uint32_t newTail = currentTail + sizeof(RenderCommand);
            tail.store(newTail, std::memory_order_release);
            LOG_TRACE_F("[RenderThread] Tail advanced: %u -> %u", currentTail, newTail);
            return reinterpret_cast<RenderCommand*>(&buffer[currentTail]);
        }

    default:
        LOG_FATAL_F("[RenderThread] Unknown RenderCommandType: %d (raw header value: 0x%I64X) at tail=%u",
                    cmdType, cmdHeader->Header.Value, currentTail);
        break;
    }

    return nullptr;
}

bool RenderCommandBuffer::IsPreviousFrameInProgress() const
{
    uint32_t last = lastFrameHead.load(std::memory_order_relaxed);
    uint32_t currentTail = tail.load(std::memory_order_acquire);
    uint32_t currentHead = head.load(std::memory_order_relaxed);

    return IsInRange(currentTail, last, currentHead);
}

void RenderCommandBuffer::CommitCommand(size_t dataSize)
{
    uint32_t current = head.load(std::memory_order_relaxed);
    uint32_t next = static_cast<uint32_t>((current + dataSize) % MAX_BUFFER_BYTES);

    head.store(next, std::memory_order_release);
}

bool RenderCommandBuffer::IsInRange(uint32_t value, [[maybe_unused]] uint32_t start, uint32_t end) const
{
    auto tailHeader = reinterpret_cast<const RenderCommand*>(&buffer[value]);
    auto headHeader = reinterpret_cast<const RenderCommand*>(&buffer[end]);

    // TODO: if the FrameNumber wraps this breaks
    return headHeader->GetFrameNum() - tailHeader->GetFrameNum() < NUM_BUFFER_FRAMES;
}
