#include  "StrigidEngine.h"
#include "Profiler.h"
#include "Logger.h"
#include <string>
#include <SDL3/SDL.h>
#include <random>
#include <vector>
#include <chrono>
#include <thread>

#include "Window.h"
#include "RenderCommandBuffer.h"

#include "Registry.h"
#include "CubeEntity.h"

// Define global component counter (declared in SchemaReflector.h)
namespace Internal {
    uint32_t g_GlobalComponentCounter(1);
    ClassID g_GlobalClassCounter = 1;
}

StrigidEngine::StrigidEngine()
{
}

StrigidEngine::~StrigidEngine()
{
}

bool StrigidEngine::Initialize([[maybe_unused]] const char* title, [[maybe_unused]] int width,
                               [[maybe_unused]] int height)
{
    STRIGID_ZONE_N("Engine_Init");

    Logger::Get().Init("StrigidEngine.log", LogLevel::Error);
    LOG_INFO("StrigidEngine initialization started");

    // 1. Core Systems (No dependencies)
    //m_JobSystem = std::make_unique<JobSystem>();
    //m_JobSystem->Init(std::thread::hardware_concurrency() - 1);

    // 2. Create command buffer for main/render thread communication
    CommandBuffer = std::make_unique<RenderCommandBuffer>();

    // 3. Store window parameters and spawn render thread
    WindowTitle = title;
    WindowWidth = width;
    WindowHeight = height;

    RenderThread = std::thread(&StrigidEngine::RenderThreadMain, this);

    // Wait for render thread to initialize
    {
        std::unique_lock lock(RenderInitMutex);
        RenderInitCondVar.wait(lock, [this]() { return bRenderThreadInitialized; });
    }
    
    int result = RenderInitResult.load(std::memory_order_acquire);
    if (result != 0)
    {
        LOG_ERROR("Render thread initialization failed");
        return false;
    }

    // 3. ECS Registry
    RegistryPtr = std::make_unique<Registry>();

    // 4. Create 1000 test entities
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posX(-30.0f, 30.0f);
    std::uniform_real_distribution<float> posY(-30.0f, 30.0f);
    std::uniform_real_distribution<float> posZ(-500.0f, -200.0f);
    std::uniform_real_distribution<float> vel(-2.0f, 2.0f);
    std::uniform_real_distribution<float> color(0.2f, 1.0f);

    for (int i = 0; i < 100000; ++i)
    {
        CubeEntity cube;
        EntityID id = RegistryPtr->Create<CubeEntity>();
        cube.transform = RegistryPtr->GetComponent<Transform>(id);
        cube.color = RegistryPtr->GetComponent<ColorData>(id);
        cube.transform->PositionX = posX(gen);
        cube.transform->PositionY = posY(gen);
        cube.transform->PositionZ = posZ(gen);
        cube.transform->RotationX = 0.0f;
        cube.transform->RotationY = 0.0f;
        cube.transform->RotationZ = 0.0f;
        cube.transform->ScaleX = cube.transform->ScaleY = cube.transform->ScaleZ = 1.0f;
        /*cube.color->R = color(gen);
        cube.color->G = color(gen);
        cube.color->B = color(gen);*/
        cube.color->B = 1.0f;
        cube.color->R = cube.color->G = 0.0f;
        cube.color->A = 1.0f;
    }

    instances.reserve(100000);

    LOG_INFO("Created 1000 test entities");

    /*
    // 3. Audio (Depends on Platform mostly)
    m_Audio = std::make_unique<AudioEngine>();
    m_Audio->Init();

    // 4. Gameplay World (Depends on everything)
    m_World = std::make_unique<World>();
    */
    bIsRunning = true;
    return true;
}

void StrigidEngine::Run()
{
    bIsRunning = true;

    const uint64_t perfFrequency = SDL_GetPerformanceFrequency();
    uint64_t lastCounter = SDL_GetPerformanceCounter();

    double physAccumulator = 0.0;
    double netAccumulator = 0.0;

    // Cache config values to avoid struct lookups in hot loop
    const double physStep = Config.GetFixedStepTime();
    const double netStep = Config.GetNetworkStepTime();
    const double targetFrameTime = Config.GetTargetFrameTime();

    // Safety caps to prevent long stalls / input starvation if we fall behind.
    // (Tune these as you like.)
    constexpr double kMaxDt = 0.25;
    constexpr double kMaxAccumulatedTime = 0.25;
    constexpr int kMaxPhysSubSteps = 8;
    constexpr int kMaxNetSubSteps = 8;

    while (bIsRunning)
    {
        // --- 0. Pump Events Early (Responsiveness) ---
        PumpEvents();

        // --- 1. Measure Delta Time ---
        const uint64_t frameStartCounter = SDL_GetPerformanceCounter();
        uint64_t counterElapsed = frameStartCounter - lastCounter;
        lastCounter = frameStartCounter;

        double dt = (double)counterElapsed / (double)perfFrequency;

        // "Spiral of Death" Safety Cap (prevent freezing if debugging)
        if (dt > kMaxDt) dt = kMaxDt;

        physAccumulator += dt;
        netAccumulator += dt;

        // Prevent unbounded catch-up after stalls
        if (physAccumulator > kMaxAccumulatedTime) physAccumulator = kMaxAccumulatedTime;
        if (netAccumulator > kMaxAccumulatedTime) netAccumulator = kMaxAccumulatedTime;

        // --- 2. Network Loop (The "Tick") ---
        // Runs at 20Hz/30Hz typically.
        if (netStep > 0.0)
        {
            int steps = 0;
            while (netAccumulator >= netStep && steps < kMaxNetSubSteps)
            {
                NetworkUpdate(netStep);
                netAccumulator -= netStep;
                ++steps;
            }
        }
        else
        {
            netAccumulator = 0.0;
        }

        // --- 3. Physics Loop (The "Sim") ---
        // Runs at 60Hz/128Hz typically.
        if (physStep > 0.0)
        {
            int steps = 0;
            while (physAccumulator >= physStep && steps < kMaxPhysSubSteps)
            {
                FixedUpdate(physStep);
                physAccumulator -= physStep;
                ++steps;
            }
        }
        else
        {
            physAccumulator = 0.0;
        }

        // --- 4. Frame Logic & Render ---
        PumpEvents();
        FrameUpdate(dt);

        // Calculate Alpha for Physics Interpolation
        double alpha = 1.0;
        if (physStep > 0.0)
        {
            alpha = physAccumulator / physStep;
            if (alpha < 0.0) alpha = 0.0;
            if (alpha > 1.0) alpha = 1.0;
        }

        RenderFrame(alpha);

        // --- 5. Frame Limiter ---
        if (targetFrameTime > 0.0)
        {
            WaitForTiming(frameStartCounter, perfFrequency);
        }

        // FPS calc
        STRIGID_FRAME_MARK();
        CalculateFPS(dt);
    }

    Shutdown();
}

void StrigidEngine::RenderFrame([[maybe_unused]] double alpha)
{
    // don't buffer a new frame if the previous one hasn't been consumed yet
    while (!CommandBuffer->IsPreviousFrameInProgress())
    {
        std::this_thread::yield();
        LOG_TRACE("[MainThread] Skipping frame - previous frame still in progress");
        //return;
    }

    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);

    LOG_DEBUG_F("[MainThread] === Starting new frame === head=%u, last=%u, tail=%u",
        CommandBuffer->GetHead(), CommandBuffer->GetLastFrameHead(), CommandBuffer->GetTail());

    // 1. Write FrameStart command
    uint8_t* wrapPtr = nullptr;
    uint32_t wrapafter = 0;
    RenderCommand* frameStartCmd = CommandBuffer->AllocateCommand<RenderCommand>(
        RenderCommandType::FrameStart,
        sizeof(RenderCommand),  // Use full header format
        wrapPtr,
        wrapafter
    );
    frameStartCmd->SetTypeAndCount(RenderCommandType::FrameStart, 0, true);
    CommandBuffer->CommitCommand(sizeof(RenderCommand));
    LOG_DEBUG_F("[MainThread] FrameStart written at head=%u", CommandBuffer->GetHead());

    // 2. Query archetypes with Transform and ColorData
    std::vector<Archetype*> archetypes = RegistryPtr->Query<Transform, ColorData>();

    // Reserve space for all instances (avoid reallocation)
    size_t totalEntities = RegistryPtr->GetTotalEntityCount();
    instances.resize(totalEntities);

    // Build instance data from ECS
    size_t instanceIdx = 0;
    for (Archetype* arch : archetypes)
    {
        // Iterate through all chunks in this archetype
        for (size_t chunkIdx = 0; chunkIdx < arch->Chunks.size(); ++chunkIdx)
        {
            Chunk* chunk = arch->Chunks[chunkIdx];
            uint32_t entityCount = arch->GetChunkCount(chunkIdx);

            // Get component arrays for this chunk
            Transform* transforms = arch->GetComponentArray<Transform>(chunk, GetComponentTypeID<Transform>());
            ColorData* colors = arch->GetComponentArray<ColorData>(chunk, GetComponentTypeID<ColorData>());

            // Copy data to instance buffer using memcpy for vectorization
            // Transform = 48 bytes (12 floats), ColorData = 16 bytes (4 floats)
            // InstanceData = 64 bytes (16 floats with padding)
            for (uint32_t i = 0; i < entityCount; ++i)
            {
                InstanceData& inst = instances[instanceIdx++];

                // Copy Transform (48 bytes = Position + Rotation + Scale)
                std::memcpy(&inst.PositionX, &transforms[i].PositionX, 48);

                // Copy ColorData (16 bytes = RGBA)
                std::memcpy(&inst.ColorR, &colors[i].R, 16);
            }
        }
    }

    // 3. Write DrawInstanced command with instance data
    if (instanceIdx > 0)
    {
        size_t commandSize = sizeof(DrawInstancedCommand) + sizeof(InstanceData) * instanceIdx;
        uint32_t headBefore = CommandBuffer->GetHead();

        DrawInstancedCommand* drawCmd = CommandBuffer->AllocateCommand<DrawInstancedCommand>(
            RenderCommandType::DrawInstanced,
            static_cast<uint32_t>(commandSize),
            wrapPtr,
            wrapafter
        );

        LOG_DEBUG_F("[MainThread] DrawInstanced: %zu instances, cmdSize=%zu, head=%u, wrap=%s",
            instanceIdx, commandSize, headBefore, wrapPtr ? "YES" : "NO");

        drawCmd->SetTypeAndCount(RenderCommandType::DrawInstanced, static_cast<uint32_t>(instanceIdx));

        if (!wrapPtr)
        {
            // No wrap, simple copy
            std::memcpy(drawCmd->instances, instances.data(), sizeof(InstanceData) * instanceIdx);
        }
        else
        {
            // Handle buffer wrap: split copy
            size_t lastPartSize = commandSize - wrapafter;
            LOG_WARN_F("[MainThread] WRAP COPY: wrapafter=%u, lastPartSize=%zu", wrapafter, lastPartSize);
            std::memcpy(drawCmd->instances, instances.data(), wrapafter);
            std::memcpy(wrapPtr, reinterpret_cast<uint8_t*>(instances.data()) + wrapafter, lastPartSize);
        }

        CommandBuffer->CommitCommand(commandSize);
        drawCmd->SetCommandFinished();
        LOG_TRACE_F("[MainThread] DrawInstanced finished, head now=%u", CommandBuffer->GetHead());
    }

    // 4. Write FrameEnd command
    RenderCommand* frameEndCmd = CommandBuffer->AllocateCommand<RenderCommand>(
        RenderCommandType::FrameEnd,
        sizeof(RenderCommand),
        wrapPtr,
        wrapafter
    );
    frameEndCmd->SetTypeAndCount(RenderCommandType::FrameEnd, 0, true);
    CommandBuffer->CommitCommand(sizeof(RenderCommand));
    LOG_DEBUG_F("[MainThread] === Frame complete === head=%u", CommandBuffer->GetHead());
}


void StrigidEngine::Shutdown()
{
    LOG_INFO("StrigidEngine shutting down");

    // Signal render thread to exit
    bShouldExitRenderThread.store(true, std::memory_order_release);

    // Wait for render thread to finish
    if (RenderThread.joinable())
    {
        RenderThread.join();
    }

    Logger::Get().Shutdown();
}

void StrigidEngine::PumpEvents()
{
    STRIGID_ZONE_N("Input_Poll");
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_EVENT_QUIT)
        {
            bIsRunning = false;
        }
    }
}

void StrigidEngine::FrameUpdate([[maybe_unused]] double dt)
{
    STRIGID_ZONE_C(STRIGID_COLOR_LOGIC);

    // Invoke Update() lifecycle on all entities
    RegistryPtr->InvokeAll(LifecycleType::Update, dt);
}

void StrigidEngine::FixedUpdate([[maybe_unused]] double dt)
{
    STRIGID_ZONE_C(STRIGID_COLOR_PHYSICS);

    // Invoke FixedUpdate() lifecycle on all entities
    RegistryPtr->InvokeAll(LifecycleType::FixedUpdate, dt);
}

void StrigidEngine::WaitForTiming(uint64_t frameStart, uint64_t perfFrequency)
{
    // Use target frame time (seconds) as the single source of truth.
    const double targetFrameTimeSec = Config.GetTargetFrameTime();
    const uint64_t targetTicks = static_cast<uint64_t>(targetFrameTimeSec * static_cast<double>(perfFrequency));
    const uint64_t frameEnd = frameStart + targetTicks;

    uint64_t currentCounter = SDL_GetPerformanceCounter();
    if (frameEnd > currentCounter)
    {
        const double remainingSec =
            static_cast<double>(frameEnd - currentCounter) / static_cast<double>(perfFrequency);

        // Sleep most of the remaining time; leave a small margin (~2ms) for the busy-wait.
        constexpr double kSleepMarginSec = 0.002;

        if (remainingSec > kSleepMarginSec)
        {
            const double sleepSec = remainingSec - kSleepMarginSec;
            SDL_Delay(static_cast<uint32_t>(sleepSec * 1000.0));
        }

        while (SDL_GetPerformanceCounter() < frameEnd)
        {
        }
    }
}

void StrigidEngine::NetworkUpdate([[maybe_unused]] double fixedDt)
{
    STRIGID_ZONE_C(STRIGID_COLOR_NETWORK);
    // 1. Process Incoming Packets (Bulk State)
    //    e.g., Update positions of other 50 players.

    // 2. Reconcile Client-Side Prediction
    //    "Server said I was actually at X, correct my position."

    // 3. Serialize Outgoing State (Snapshot)
    //    "Here is where I think I am."
}

void StrigidEngine::SendNetworkEvent([[maybe_unused]] const std::string& eventData)
{
    STRIGID_ZONE_C(STRIGID_COLOR_NETWORK);
}

void StrigidEngine::CalculateFPS(double dt)
{
    FrameCount++;
    FpsTimer += dt;

    // Update title every 1 second
    if (FpsTimer >= 1.0)
    {
        double fps = FrameCount / FpsTimer;
        double ms = (FpsTimer / FrameCount) * 1000.0;

        std::string title = "StrigidEngine V0.1 | FPS: " + std::to_string((int)fps) +
            " | Frame: " + std::to_string(ms) + "ms";

        LOG_ALWAYS_F("FPS: %d", (int)fps);
        //EngineWindow->SetTitle(title.c_str());

        FrameCount = 0;
        FpsTimer = 0.0;
    }
}

void StrigidEngine::RenderThreadMain()
{
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);

    // Create Window on render thread
    EngineWindow = std::make_unique<Window>();
    if (EngineWindow->Open(WindowTitle, WindowWidth, WindowHeight) < 0)
    {
        RenderInitResult.store(-1, std::memory_order_release);
        {
            std::lock_guard lock(RenderInitMutex);
            bRenderThreadInitialized = true;
        }
        RenderInitCondVar.notify_one();
        return;
    }

    // Signal successful initialization
    RenderInitResult.store(0, std::memory_order_release);
    {
        std::lock_guard lock(RenderInitMutex);
        bRenderThreadInitialized = true;
    }
    RenderInitCondVar.notify_one();

    // Render thread loop
    RenderThreadLoop();

    // Cleanup window on render thread
    EngineWindow->Shutdown();
    EngineWindow.reset();
}

void StrigidEngine::RenderThreadLoop()
{
    uint8_t* wrapPtr = nullptr;
    uint32_t wrapAfter = 0;
    while (!bShouldExitRenderThread.load(std::memory_order_acquire))
    {
        STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);
        RenderCommand* cmd = CommandBuffer->GetCommand(wrapPtr, wrapAfter);
        if (cmd == nullptr)
        {
            //std::this_thread::sleep_for(std::chrono::microseconds(10));
            continue;
        }
        
        switch (cmd->GetType())
        {
            case RenderCommandType::DrawInstanced:
            {
                DrawInstancedCommand* drawCmd = static_cast<DrawInstancedCommand*>(cmd);
                EngineWindow->DrawInstances(drawCmd->instances, drawCmd->GetCount(), wrapPtr, wrapAfter);
                break;
            }
            
            case RenderCommandType::FrameEnd:
            {
                CalculateRenderFPS();
                break;
            }
            
        }
    }
}

void StrigidEngine::CalculateRenderFPS()
{
    STRIGID_ZONE_N("RenderThread_UpdateFPS");
    // FPS tracking for render thread
    const uint64_t perfFrequency = SDL_GetPerformanceFrequency();

    uint64_t currentFrameTime = SDL_GetPerformanceCounter();
    double frameDt = static_cast<double>(currentFrameTime - lastFrameTime) / static_cast<double>(perfFrequency);
    lastFrameTime = currentFrameTime;

    renderFrameCount++;
    renderFpsTimer += frameDt;

    // Update title every 1 second
    if (renderFpsTimer >= 1.0)
    {
        static uint32_t FrameNumber = renderFrameCount;
        double renderFps = renderFrameCount / renderFpsTimer;
        double renderMs = (renderFpsTimer / renderFrameCount) * 1000.0;

        std::string title = "StrigidEngine V0.1 | Render FPS: " + std::to_string(static_cast<int>(renderFps)) +
            " | Frame: " + std::to_string(FrameNumber) + " | Render: " + std::to_string(renderMs) + "ms";

        EngineWindow->SetTitle(title.c_str());

        FrameNumber += renderFrameCount;
        renderFrameCount = 0;
        renderFpsTimer = 0.0;
    }
}
