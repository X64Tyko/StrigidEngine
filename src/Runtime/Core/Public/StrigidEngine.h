#pragma once
#include <memory>
#include <atomic>
#include <SDL3/SDL_gpu.h>

#include "EngineConfig.h"
#include "RenderThread.h"
#include "../../Rendering/Private/FramePacer.h"

// Forward declarations
class Window;
class Registry;
class LogicThread;
class RenderThread;
struct EngineConfig;

/**
 * StrigidEngine: The Sentinel (Main Thread)
 *
 * Responsibilities:
 * - OS Event Pumping (SDL requires this on main thread)
 * - Window ownership (SDL3 requirement)
 * - GPU Command Buffer acquisition and submission (SDL3 requirement)
 * - Frame Pacing
 * - Thread Lifecycle Management
 *
 * Threading Model:
 * - Main Thread: Events, Window, GPU acquire/submit, Frame Pacing
 * - Logic Thread: Simulation, produces FramePackets
 * - Render Thread: Consumes FramePackets, prepares render data, builds command buffers
 *
 * GPU Resource Flow:
 * 1. RenderThread requests resources (bNeedsGPUResources)
 * 2. Main continues polling events, waits for FramePacer fence
 * 3. When fence clears AND RenderThread needs resources → Main acquires
 * 4. Main provides resources via atomics, clears bNeedsGPUResources
 * 5. RenderThread builds commands, signals bReadyToSubmit
 * 6. Main retrieves CmdBuffer, submits via FramePacer
 * 7. Main loops back to step 1
 */
class StrigidEngine
{
public:
    StrigidEngine();
    ~StrigidEngine();
    StrigidEngine(const StrigidEngine&) = delete;
    StrigidEngine& operator=(const StrigidEngine&) = delete;

    bool Initialize(const char* title, int width, int height);
    void Run();
    void Shutdown();

    // Singleton
    static StrigidEngine& Get()
    {
        static StrigidEngine instance;
        return instance;
    }

private:
    // Sentinel Tasks (Main Thread)
    void PumpEvents(); // Handle OS events
    void ServiceRenderThread(); // Check if RenderThread needs GPU resources or wants to submit
    void AcquireAndProvideGPUResources(); // Acquire cmd + swapchain, provide to RenderThread
    void SubmitRenderCommands(); // Take CmdBuffer from RenderThread and submit
    void WaitForTiming(uint64_t frameStart, uint64_t perfFrequency);

    // FPS tracking
    void CalculateFPS();

    // --- Core Systems ---
    SDL_Window* EngineWindow;
    SDL_GPUDevice* GpuDevice; // MUST be on main thread for SDL
    std::unique_ptr<Registry> RegistryPtr;
    EngineConfig Config;
    FramePacer Pacer;

    // --- Thread Modules ---
    std::unique_ptr<LogicThread> Logic;
    std::unique_ptr<RenderThread> Render;

    // --- Lifecycle ---
    std::atomic<bool> bIsRunning{false};

    // FPS tracking
    double FpsTimer = 0;
    double LastFPSCheck = 0;
    int FrameCount = 0;
};
