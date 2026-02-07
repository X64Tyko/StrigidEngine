#include  "StrigidEngine.h"
#include "Profiler.h"
#include "Logger.h"
#include <string>
#include <SDL3/SDL.h>
#include <random>
#include <vector>

#include "Window.h"

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

    Logger::Get().Init("StrigidEngine.log", LogLevel::Debug);
    LOG_INFO("StrigidEngine initialization started");

    // 1. Core Systems (No dependencies)
    //m_JobSystem = std::make_unique<JobSystem>();
    //m_JobSystem->Init(std::thread::hardware_concurrency() - 1);

    // 2. Platform (Window/Video)
    EngineWindow = std::make_unique<Window>();
    if (EngineWindow->Open(title, width, height) < 0) return false;

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
        cube.color->R = color(gen);
        cube.color->G = color(gen);
        cube.color->B = color(gen);
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
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);

    // Query archetypes with Transform and ColorData
    std::vector<Archetype*> archetypes = RegistryPtr->Query<Transform, ColorData>();

    instances.clear();
    
    // Build instance data from ECS
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

            // Copy data to instance buffer
            for (uint32_t i = 0; i < entityCount; ++i)
            {
                InstanceData inst;
                inst.PositionX = transforms[i].PositionX;
                inst.PositionY = transforms[i].PositionY;
                inst.PositionZ = transforms[i].PositionZ;
                inst.RotationX = transforms[i].RotationX;
                inst.RotationY = transforms[i].RotationY;
                inst.RotationZ = transforms[i].RotationZ;
                inst.ScaleX = transforms[i].ScaleX;
                inst.ScaleY = transforms[i].ScaleY;
                inst.ScaleZ = transforms[i].ScaleZ;
                inst.ColorR = colors[i].R;
                inst.ColorG = colors[i].G;
                inst.ColorB = colors[i].B;
                inst.ColorA = colors[i].A;
                instances.push_back(inst);
            }
        }
    }

    EngineWindow->DrawInstances(instances.data(), instances.size());
}


void StrigidEngine::Shutdown()
{
    LOG_INFO("StrigidEngine shutting down");
    Logger::Get().Shutdown();
}

void StrigidEngine::PumpEvents()
{
    STRIGID_ZONE_N("Input_Poll");
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_EVENT_QUIT) bIsRunning = false;
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

        EngineWindow->SetTitle(title.c_str());

        FrameCount = 0;
        FpsTimer = 0.0;
    }
}
