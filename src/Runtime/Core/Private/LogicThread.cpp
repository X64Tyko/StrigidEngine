#include "LogicThread.h"
#include "FramePacket.h"
#include "Registry.h"
#include "EngineConfig.h"
#include "Profiler.h"
#include "Logger.h"
#include <SDL3/SDL.h>

void LogicThread::Initialize(Registry* registry, const EngineConfig* config, int windowWidth, int windowHeight)
{
    RegistryPtr = registry;
    ConfigPtr = config;
    WindowWidth = windowWidth;
    WindowHeight = windowHeight;

    // Allocate 3 FramePackets for triple buffering
    StagingPacket = std::make_shared<FramePacket>();
    auto mailboxPacket = std::make_shared<FramePacket>();
    // Third packet will be allocated by RenderThread as its VisualPacket

    Mailbox.store(mailboxPacket, std::memory_order_release);

    LOG_INFO("[LogicThread] Initialized with triple-buffer mailbox");
}

void LogicThread::Start()
{
    bIsRunning.store(true, std::memory_order_release);
    Thread = std::thread(&LogicThread::ThreadMain, this);
    LOG_INFO("[LogicThread] Started");
}

void LogicThread::Stop()
{
    bIsRunning.store(false, std::memory_order_release);
    LOG_INFO("[LogicThread] Stop requested");
}

void LogicThread::Join()
{
    auto threadID = Thread.get_id();
    if (Thread.joinable())
    {
        Thread.join();
        LOG_INFO("[LogicThread] Joined");
    }

    // Cleanup mailbox
    StagingPacket = nullptr;
    Mailbox.store(nullptr, std::memory_order_release);
}

std::shared_ptr<FramePacket> LogicThread::ExchangeMailbox(std::shared_ptr<FramePacket> visualPacket)
{
    // RenderThread calls this to swap its visualPacket with Mailbox
    // Returns the mailbox packet (which has new data from LogicThread)
    return Mailbox.exchange(visualPacket, std::memory_order_acq_rel);
}

void LogicThread::ThreadMain()
{
    const uint64_t perfFrequency = SDL_GetPerformanceFrequency();
    uint64_t lastCounter = SDL_GetPerformanceCounter();

    // Cache config values
    const double fixedStepTime = ConfigPtr->GetFixedStepTime();

    // Safety caps
    constexpr double kMaxDt = 0.25;
    constexpr double kMaxAccumulatedTime = 0.25;
    constexpr int kMaxPhysSubSteps = 8;

    while (bIsRunning.load(std::memory_order_acquire))
    {
        STRIGID_ZONE_C(STRIGID_COLOR_LOGIC);

        // Measure delta time
        const uint64_t frameStartCounter = SDL_GetPerformanceCounter();
        uint64_t counterElapsed = frameStartCounter - lastCounter;
        lastCounter = frameStartCounter;

        double dt = static_cast<double>(counterElapsed) / static_cast<double>(perfFrequency);

        // FPS tracking
        FpsFrameCount++;
        FpsTimer += dt;

        if (FpsTimer >= 1.0)
        {
            double fps = FpsFrameCount / FpsTimer;
            double ms = (FpsTimer / FpsFrameCount) * 1000.0;

            LOG_DEBUG_F("Logic FPS: %d | Frame: %.2fms", static_cast<int>(fps), ms);

            FpsFrameCount = 0;
            FpsTimer = 0.0;
        }
        if (FpsFixedTimer >= 1.0)
        {
            double fps = FpsFixedCount / FpsFixedTimer;
            double ms = (FpsFixedTimer / FpsFixedCount) * 1000.0;

            LOG_DEBUG_F("Fixed FPS: %d | Frame: %.2fms", static_cast<int>(fps), ms);

            FpsFixedCount = 0;
            FpsFixedTimer = 0.0;
        }

        // Spiral of death cap
        if (dt > kMaxDt) dt = kMaxDt;

        Accumulator += dt;

        // Prevent unbounded catch-up
        if (Accumulator > kMaxAccumulatedTime) Accumulator = kMaxAccumulatedTime;

        // Fixed update loop with substepping
        if (fixedStepTime > 0.0)
        {
            STRIGID_ZONE_C(STRIGID_COLOR_LOGIC);

            int steps = 0;
            while (Accumulator >= fixedStepTime && steps < kMaxPhysSubSteps)
            {
                // FPS tracking
                FpsFixedCount++;
                FpsFixedTimer += fixedStepTime;
                
                PrePhysics(fixedStepTime);
                // insert Sim physics here
                PostPhysics(fixedStepTime);
                Accumulator -= fixedStepTime;
                ++steps;
            }

            ProduceFramePacket();
        }

        // Variable update
        Update(dt);

        // Frame limiter (if MaxFPS is set in config)
        if (ConfigPtr->TargetFPS > 0)
        {
            WaitForTiming(frameStartCounter, perfFrequency);
        }
    }
}

void LogicThread::ProcessInput()
{
    // TODO: Future feature - swap input mailbox
    // CurrentInput = InputMailbox.exchange(&InputFrontBuffer, std::memory_order_acq_rel);
}

void LogicThread::Update(double dt)
{
    STRIGID_ZONE_N("Logic_Update");

    // Invoke Update() lifecycle on all entities
    RegistryPtr->InvokeUpdate(dt);
}

void LogicThread::PostPhysics(double dt)
{
    STRIGID_ZONE_N("Logic_FixedUpdate");

    RegistryPtr->InvokePostPhys(dt);

    SimulationTime += dt;
}

void LogicThread::ProduceFramePacket()
{
    STRIGID_ZONE_N("Logic_ProduceFramePacket");

    // Fill staging packet
    StagingPacket->SimulationTime = SimulationTime;
    StagingPacket->ActiveEntityCount = static_cast<uint32_t>(RegistryPtr->GetTotalEntityCount());
    StagingPacket->FrameNumber = ++FrameNumber;

    // Fill ViewState (basic perspective camera)
    float AspectRatio = static_cast<float>(WindowWidth) / static_cast<float>(WindowHeight);
    float Fov = 60.0f * 3.14159f / 180.0f; // 60 degrees in radians
    float ZNear = 0.1f;
    float ZFar = 1000.0f;

    // Perspective projection matrix (column-major for GLSL)
    float F = 1.0f / std::tan(Fov / 2.0f);
    StagingPacket->View.ProjectionMatrix.m[0] = F / AspectRatio;
    StagingPacket->View.ProjectionMatrix.m[1] = 0.0f;
    StagingPacket->View.ProjectionMatrix.m[2] = 0.0f;
    StagingPacket->View.ProjectionMatrix.m[3] = 0.0f;

    StagingPacket->View.ProjectionMatrix.m[4] = 0.0f;
    StagingPacket->View.ProjectionMatrix.m[5] = F;
    StagingPacket->View.ProjectionMatrix.m[6] = 0.0f;
    StagingPacket->View.ProjectionMatrix.m[7] = 0.0f;

    StagingPacket->View.ProjectionMatrix.m[8] = 0.0f;
    StagingPacket->View.ProjectionMatrix.m[9] = 0.0f;
    StagingPacket->View.ProjectionMatrix.m[10] = ZFar / (ZFar - ZNear);
    StagingPacket->View.ProjectionMatrix.m[11] = -(ZFar * ZNear) / (ZFar - ZNear);

    StagingPacket->View.ProjectionMatrix.m[12] = 0.0f;
    StagingPacket->View.ProjectionMatrix.m[13] = 0.0f;
    StagingPacket->View.ProjectionMatrix.m[14] = 1;
    StagingPacket->View.ProjectionMatrix.m[15] = 0.0f;

    // View matrix = identity for now (camera at origin)
    // ViewMatrix is already initialized to identity in Matrix4 constructor

    // Camera position at origin
    StagingPacket->View.CameraPosition.x = 0.0f;
    StagingPacket->View.CameraPosition.y = 0.0f;
    StagingPacket->View.CameraPosition.z = 0.0f;

    // TODO: Fill SceneState (sun direction, color)

    // Publish to mailbox
    PublishFramePacket();
}

void LogicThread::PublishFramePacket()
{
    // Atomic swap: Staging ↔ Mailbox
    // After this, RenderThread can see the new packet
    std::shared_ptr<FramePacket> old = Mailbox.exchange(StagingPacket, std::memory_order_acq_rel);
    StagingPacket = old; // Reuse the old mailbox packet for next frame
}

void LogicThread::WaitForTiming(uint64_t frameStart, uint64_t perfFrequency)
{
    STRIGID_ZONE_N("Logic_WaitTiming");

    const double targetFrameTimeSec = ConfigPtr->GetTargetFrameTime();
    const uint64_t targetTicks = static_cast<uint64_t>(targetFrameTimeSec * static_cast<double>(perfFrequency));
    const uint64_t frameEnd = frameStart + targetTicks;

    uint64_t currentCounter = SDL_GetPerformanceCounter();
    if (frameEnd > currentCounter)
    {
        const double remainingSec =
            static_cast<double>(frameEnd - currentCounter) / static_cast<double>(perfFrequency);

        constexpr double kSleepMarginSec = 0.002;

        if (remainingSec > kSleepMarginSec)
        {
            const double sleepSec = remainingSec - kSleepMarginSec;
            SDL_Delay(static_cast<uint32_t>(sleepSec * 1000.0));
        }

        while (SDL_GetPerformanceCounter() < frameEnd)
        {
            // Busy wait
        }
    }
}
