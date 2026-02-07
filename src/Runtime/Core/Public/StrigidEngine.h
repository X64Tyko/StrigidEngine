#pragma once
#include <memory>
#include <string>
#include <vector>

#include "EngineConfig.h"
#include "Window.h"
//#include "Core/Types.h"

// Forward Declarations (Keep compile times fast)
class Window;
class AudioEngine;
class JobSystem;
class PhysicsWorld;
class World;
class Profiler;
class Registry;

class StrigidEngine {
    
    StrigidEngine();
    ~StrigidEngine();
    StrigidEngine(const StrigidEngine&) =delete;
    StrigidEngine& operator=(const StrigidEngine&) =delete;

public:
    // 1. The Boot Sequence
    // Returns false if hardware init fails (e.g., No GPU found)
    bool Initialize(const char* Title, int Width, int Height);

    // 2. The Main Loop
    // Blocks until the game is closed.
    void Run();

    // 3. The Shutdown
    // Explicit cleanup (saves configs, closes streams)
    void Shutdown();

    // Accessors (Static or Singleton-like access if preferred)
    static StrigidEngine& Get()
    {
        static StrigidEngine Instance;
        return Instance;
    }

private:
    // The "Frame Pipeline" - Internal steps of the loop
    void PumpEvents();
    void FrameUpdate(double Dt);
    void FixedUpdate(double Dt);
    void RenderFrame(double Alpha);
    void WaitForTiming(uint64_t FrameStart, uint64_t PerfFrequency); // Cap FPS / VSync
    void NetworkUpdate(double FixedDt);

    // 4. Immediate Network Events
    // Call this from FrameUpdate or Input to send critical "One-Offs" 
    // (e.g., "Player Fired Gun") immediately without waiting for the Tick.
    void SendNetworkEvent(const std::string& EventData);
    
private:
    void CalculateFPS(double Dt);
    
private:
    EngineConfig& GetConfig() { return Config; }
    EngineConfig Config;
    bool bIsRunning = false;
    double NetAccumulator = 0.0;
    
    // FPS Counting
    double FpsTimer = 0.0;
    int FrameCount = 0;
    
    // Subsystems (Order matters for destruction!)
    std::unique_ptr<Registry>     RegistryPtr;
    std::unique_ptr<Window>       EngineWindow;
    
    std::vector<InstanceData> instances;
    /*
    std::unique_ptr<Window>       Window;
    std::unique_ptr<JobSystem>    JobSystem;
    std::unique_ptr<PhysicsWorld> Physics;
    std::unique_ptr<AudioEngine>  Audio;
    std::unique_ptr<World>        World; // The ECS Registry lives here
    */
};