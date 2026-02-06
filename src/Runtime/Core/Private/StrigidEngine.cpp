#include  "StrigidEngine.h"
/*
#include "Platform/Window.h"
#include "Systems/JobSystem.h"
#include "Audio/AudioEngine.h"
#include "Tools/Profiler.h"
*/
#include <string>
#include <SDL3/SDL.h>

#include "Window.h"


StrigidEngine::StrigidEngine() {
}

StrigidEngine::~StrigidEngine()
{
}

bool StrigidEngine::Initialize([[maybe_unused]]const char* title, [[maybe_unused]]int width, [[maybe_unused]]int height) {
    //ZONE_SCOPED("Engine_Init");

    // 1. Core Systems (No dependencies)
    //m_JobSystem = std::make_unique<JobSystem>();
    //m_JobSystem->Init(std::thread::hardware_concurrency() - 1);

    // 2. Platform (Window/Video)
    m_Window = std::make_unique<Window>();
    if (m_Window->Open(title, width, height) < 0) return false;

    /*
    // 3. Audio (Depends on Platform mostly)
    m_Audio = std::make_unique<AudioEngine>();
    m_Audio->Init();

    // 4. Gameplay World (Depends on everything)
    m_World = std::make_unique<World>();
    */
    m_IsRunning = true;
    return true;
}

void StrigidEngine::Run() {
    m_IsRunning = true;

    const uint64_t perfFrequency = SDL_GetPerformanceFrequency();
    uint64_t lastCounter = SDL_GetPerformanceCounter();

    double physAccumulator = 0.0;
    double netAccumulator = 0.0;

    // Cache config values to avoid struct lookups in hot loop
    const double physStep = m_Config.GetFixedStepTime();
    const double netStep = m_Config.GetNetworkStepTime();
    const double targetFrameTime = m_Config.GetTargetFrameTime();

    // Safety caps to prevent long stalls / input starvation if we fall behind.
    // (Tune these as you like.)
    constexpr double kMaxDt = 0.25;
    constexpr double kMaxAccumulatedTime = 0.25;
    constexpr int kMaxPhysSubSteps = 8;
    constexpr int kMaxNetSubSteps  = 8;

    while (m_IsRunning) {
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
        if (netAccumulator > kMaxAccumulatedTime)  netAccumulator  = kMaxAccumulatedTime;

        // --- 2. Network Loop (The "Tick") ---
        // Runs at 20Hz/30Hz typically.
        if (netStep > 0.0) {
            int steps = 0;
            while (netAccumulator >= netStep && steps < kMaxNetSubSteps) {
                NetworkUpdate(netStep);
                netAccumulator -= netStep;
                ++steps;
            }
        } else {
            netAccumulator = 0.0;
        }

        // --- 3. Physics Loop (The "Sim") ---
        // Runs at 60Hz/128Hz typically.
        if (physStep > 0.0) {
            int steps = 0;
            while (physAccumulator >= physStep && steps < kMaxPhysSubSteps) {
                FixedUpdate(physStep);
                physAccumulator -= physStep;
                ++steps;
            }
        } else {
            physAccumulator = 0.0;
        }

        // --- 4. Frame Logic & Render ---
        PumpEvents();
        FrameUpdate(dt);

        // Calculate Alpha for Physics Interpolation
        double alpha = 1.0;
        if (physStep > 0.0) {
            alpha = physAccumulator / physStep;
            if (alpha < 0.0) alpha = 0.0;
            if (alpha > 1.0) alpha = 1.0;
        }

        RenderFrame(alpha);

        // --- 5. Frame Limiter ---
        if (targetFrameTime > 0.0) {
            WaitForTiming(frameStartCounter, perfFrequency);
        }

        // FPS calc
        CalculateFPS(dt);
    }

    Shutdown();
}

void StrigidEngine::Shutdown()
{
}

void StrigidEngine::PumpEvents() {
    //ZONE_SCOPED("Input_Poll");
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) m_IsRunning = false;
        //m_Window->ProcessEvent(e);
        // Dispatch to ImGui here too
    }
}

void StrigidEngine::FrameUpdate([[maybe_unused]]double dt) {
    //ZONE_SCOPED("ECS_Logic");

    // Phase 1: Calc (Parallel Read)
    // Runs user scripts, generates Command Buffers
    //m_World->RunSystems(SystemPhase::Update, dt);

    // Phase 2: Apply (Serial/Batch Write)
    // Applies the "Deferred Writes" we discussed
    //m_World->ResolveCommandBuffers();
}

void StrigidEngine::TickPhysics([[maybe_unused]]double dt) {
    //ZONE_SCOPED("Physics_Step");
    // Sync Transforms -> Physics Bodies
    //m_Physics->PreStep(dt);
    
    // Run Solver
    //m_Physics->Step(dt);
    
    // Sync Physics Bodies -> Transforms
    //m_Physics->PostStep();
}

void StrigidEngine::FixedUpdate([[maybe_unused]]double dt)
{
}

void StrigidEngine::RenderFrame([[maybe_unused]]double alpha) {
    m_Window->Render();
    /*
    ZONE_SCOPED("Render_Submit");
    m_Window->BeginFrame();
    m_World->Render(m_Window.get());
    m_Window->Present();
    */
}

void StrigidEngine::WaitForTiming(uint64_t frameStart, uint64_t perfFrequency)
{
    // Use target frame time (seconds) as the single source of truth.
    const double targetFrameTimeSec = m_Config.GetTargetFrameTime();
    const uint64_t targetTicks = static_cast<uint64_t>(targetFrameTimeSec * static_cast<double>(perfFrequency));
    const uint64_t frameEnd = frameStart + targetTicks;

    uint64_t currentCounter = SDL_GetPerformanceCounter();
    if (frameEnd > currentCounter) {

        const double remainingSec =
            static_cast<double>(frameEnd - currentCounter) / static_cast<double>(perfFrequency);

        // Sleep most of the remaining time; leave a small margin (~2ms) for the busy-wait.
        constexpr double kSleepMarginSec = 0.002;

        if (remainingSec > kSleepMarginSec) {
            const double sleepSec = remainingSec - kSleepMarginSec;
            SDL_Delay(static_cast<uint32_t>(sleepSec * 1000.0));
        }

        while (SDL_GetPerformanceCounter() < frameEnd)
        {
        }
    }
}

void StrigidEngine::NetworkUpdate([[maybe_unused]]double fixedDt) {
    // 1. Process Incoming Packets (Bulk State)
    //    e.g., Update positions of other 50 players.
    
    // 2. Reconcile Client-Side Prediction
    //    "Server said I was actually at X, correct my position."
    
    // 3. Serialize Outgoing State (Snapshot)
    //    "Here is where I think I am."
}

void StrigidEngine::CalculateFPS(double dt) {
    m_FrameCount++;
    m_FpsTimer += dt;

    // Update title every 1 second
    if (m_FpsTimer >= 1.0) {
        double fps = m_FrameCount / m_FpsTimer;
        double ms = (m_FpsTimer / m_FrameCount) * 1000.0;

        std::string title = "StrigidEngine V0.1 | FPS: " + std::to_string((int)fps) + 
                            " | Frame: " + std::to_string(ms) + "ms";
        
        m_Window->SetTitle(title.c_str());

        m_FrameCount = 0;
        m_FpsTimer = 0.0;
    }
}
