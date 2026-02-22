#pragma once
#include <thread>
#include <atomic>

#include "Registry.h"

// Forward declarations
class Registry;
struct EngineConfig;
struct InputState;
struct FramePacket;

/**
 * LogicThread: The Brain
 *
 * Runs simulation at FixedUpdateHz with accumulator/substepping
 * Produces FramePackets for Render thread consumption via triple-buffer mailbox
 * Owns the mailbox setup
 */
class LogicThread
{
public:
    LogicThread() = default;
    ~LogicThread() = default;

    void Initialize(Registry* registry, const EngineConfig* config, int windowWidth, int windowHeight);
    void Start();
    void Stop();
    void Join();

    // Mailbox access for RenderThread
    std::shared_ptr<FramePacket> ExchangeMailbox(std::shared_ptr<FramePacket> visualPacket);
    // CAS swap: give visualPacket, get mailbox packet

    // Allow RenderThread to peek at accumulator for interpolation alpha calculation
    double GetAccumulator() const { return Accumulator; }

private:
    void ThreadMain(); // Thread entry point

    // Lifecycle Methods
    void ProcessInput(); // Swap input mailbox (TODO: future feature)
    void Update(double dt); // Variable update (runs every frame)
    void PrePhysics(double dt); // Fixed update at FixedUpdateHz
    void PostPhysics(double dt); // Fixed update at FixedUpdateHz
    void ProduceFramePacket(); // Fill staging packet and publish to mailbox

    void PublishFramePacket(); // CAS swap staging → mailbox
    void WaitForTiming(uint64_t frameStart, uint64_t perfFrequency);

    // References (non-owning)
    Registry* RegistryPtr = nullptr;
    const EngineConfig* ConfigPtr = nullptr;

    // Input (future)
    InputState* CurrentInput = nullptr;

    // Triple Buffer Mailbox (LogicThread owns allocation)
    // Logic writes → StagingPacket
    // CAS swap → Mailbox
    // Render reads → VisualPacket (Render owns this pointer)
    std::shared_ptr<FramePacket> StagingPacket = nullptr;
    std::atomic<std::shared_ptr<FramePacket>> Mailbox{nullptr};

    // Note: RenderThread will manage its own VisualPacket pointer

    // Threading
    std::thread Thread;
    std::atomic<bool> bIsRunning{false};

    // Timing
    double Accumulator = 0.0;
    double SimulationTime = 0.0;
    uint32_t FrameNumber = 0;
    int WindowWidth = 1920;
    int WindowHeight = 1080;

    // FPS tracking
    uint32_t FpsFrameCount = 0;
    double FpsTimer = 0.0;

    // FPS tracking
    uint32_t FpsFixedCount = 0;
    double FpsFixedTimer = 0.0;
};

inline void LogicThread::PrePhysics(double dt)
{
    STRIGID_ZONE_N("Logic_FixedUpdate");

    RegistryPtr->InvokePrePhys(dt);

    SimulationTime += dt;
}