#pragma once
#include <memory>
#include <string>

#include "EngineConfig.h"
//#include "Core/Types.h"

// Forward Declarations (Keep compile times fast)
class Window;
class AudioEngine;
class JobSystem;
class PhysicsWorld;
class World;
class Profiler;

class StrigidEngine {
    
    StrigidEngine();
    ~StrigidEngine();
    StrigidEngine(const StrigidEngine&) =delete;
    StrigidEngine& operator=(const StrigidEngine&) =delete;

public:
    // 1. The Boot Sequence
    // Returns false if hardware init fails (e.g., No GPU found)
    bool Initialize(const char* title, int width, int height);

    // 2. The Main Loop
    // Blocks until the game is closed.
    void Run();

    // 3. The Shutdown
    // Explicit cleanup (saves configs, closes streams)
    void Shutdown();

    // Accessors (Static or Singleton-like access if preferred)
    static StrigidEngine& Get()
    {
        static StrigidEngine s_Instance;
        return s_Instance;
    }

private:
    // The "Frame Pipeline" - Internal steps of the loop
    void PumpEvents();
    void FrameUpdate(double dt);
    void TickPhysics(double dt);
    void FixedUpdate(double dt);
    void RenderFrame(double alpha);
    void WaitForTiming(uint64_t frameStart, uint64_t perfFrequency); // Cap FPS / VSync
    void NetworkUpdate(double fixedDt);

    // 4. Immediate Network Events
    // Call this from FrameUpdate or Input to send critical "One-Offs" 
    // (e.g., "Player Fired Gun") immediately without waiting for the Tick.
    void SendNetworkEvent(const std::string& eventData);
    
private:
    void CalculateFPS(double dt);
    
private:
    EngineConfig& GetConfig() { return m_Config; }
    EngineConfig m_Config;
    bool m_IsRunning = false;
    double m_NetAccumulator = 0.0;
    
    // FPS Counting
    double m_FpsTimer = 0.0;
    int m_FrameCount = 0;
    
    // Subsystems (Order matters for destruction!)
    std::unique_ptr<Window>       m_Window;
    /*
    std::unique_ptr<Window>       m_Window;
    std::unique_ptr<JobSystem>    m_JobSystem;
    std::unique_ptr<PhysicsWorld> m_Physics;
    std::unique_ptr<AudioEngine>  m_Audio;
    std::unique_ptr<World>        m_World; // The ECS Registry lives here
    */
};