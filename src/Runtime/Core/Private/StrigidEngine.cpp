#include "StrigidEngine.h"
#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include "EngineConfig.h"
#include "Logger.h"
#include "LogicThread.h"
#include "Profiler.h"
#include "Registry.h"
#include "RenderThread.h"

// Define global component counter (declared in SchemaReflector.h)
namespace Internal
{
    uint32_t g_GlobalComponentCounter(1);
    ClassID g_GlobalClassCounter = 1;
}

StrigidEngine::StrigidEngine()
    : EngineWindow(nullptr)
      , GpuDevice(nullptr)
{
}

StrigidEngine::~StrigidEngine()
{
}

bool StrigidEngine::Initialize(const char* title, int width, int height)
{
    STRIGID_ZONE_N("Engine_Init");

    Logger::Get().Init("StrigidEngine.log", LogLevel::Debug);
    LOG_INFO("StrigidEngine initialization started");

    // Initialize SDL (should already be initialized by Engine)
    if (!SDL_WasInit(SDL_INIT_VIDEO))
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            std::cerr << "SDL Init Failed: " << SDL_GetError() << std::endl;
            return false;
        }
    }

    // Create Window
    EngineWindow = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
    if (!EngineWindow)
    {
        std::cerr << "Window Create Failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create GPU Device
    GpuDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (!GpuDevice)
    {
        std::cerr << "GPU Device Failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(EngineWindow);
        return false;
    }

    // Claim the Window for the Device
    if (!SDL_ClaimWindowForGPUDevice(GpuDevice, EngineWindow))
    {
        std::cerr << "Claim Window Failed: " << SDL_GetError() << std::endl;
        SDL_DestroyGPUDevice(GpuDevice);
        SDL_DestroyWindow(EngineWindow);
        return false;
    }

    if (!SDL_SetGPUSwapchainParameters(GpuDevice, EngineWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       SDL_GPU_PRESENTMODE_MAILBOX))
    {
        std::cerr << "Swapchain Parameters Failed: " << SDL_GetError() << std::endl;
        SDL_DestroyGPUDevice(GpuDevice);
        SDL_DestroyWindow(EngineWindow);
        return false;
    }

    // Create Registry
    RegistryPtr = std::make_unique<Registry>(&Config);
    Pacer.Initialize(GpuDevice);

    // Create threads
    Logic = std::make_unique<LogicThread>();
    Render = std::make_unique<RenderThread>();

    // Initialize threads
    Logic->Initialize(RegistryPtr.get(), &Config, width, height);

    // TODO: Get GPU device from Window
    // For now, pass nullptr - this will need to be fixed
    Render->Initialize(RegistryPtr.get(), Logic.get(), &Config, GpuDevice, EngineWindow);
    return true;
}

void StrigidEngine::Run()
{
    // Start threads
    Logic->Start();
    Render->Start();

    bIsRunning = true;

    const uint64_t perfFrequency = SDL_GetPerformanceFrequency();

    while (bIsRunning.load(std::memory_order_acquire))
    {
        STRIGID_ZONE_N("Main_Frame");

        const uint64_t frameStartCounter = SDL_GetPerformanceCounter();

        // Pump events early
        PumpEvents();

        // Service render thread (check for GPU resource requests or submit commands)
        ServiceRenderThread();

        // Frame limiter (if InputPollHz is set in config)
        if (Config.InputPollHz > 0)
        {
            WaitForTiming(frameStartCounter, perfFrequency);
        }

        // FPS tracking
        STRIGID_FRAME_MARK();
        CalculateFPS();
    }

    Shutdown();
}

void StrigidEngine::Shutdown()
{
    LOG_INFO("StrigidEngine shutting down");

    // Stop threads
    if (Logic) Logic->Stop();
    if (Render) Render->Stop();

    // Join threads
    if (Logic) Logic->Join();
    if (Render) Render->Join();

    // Cleanup window
    if (EngineWindow)
    {
        SDL_DestroyWindow(EngineWindow);
        EngineWindow = nullptr;
    }

    SDL_Quit();
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
            bIsRunning.store(false, std::memory_order_release);
        }
    }
}

void StrigidEngine::ServiceRenderThread()
{
    STRIGID_ZONE_N("Service_RenderThread");

    if (!Render)
        return;

    // Check if RenderThread is ready to submit
    if (Render->ReadyToSubmit())
    {
        SubmitRenderCommands();
    }

    // Check if RenderThread needs GPU resources
    if (Render->NeedsGPUResources())
    {
        AcquireAndProvideGPUResources();
    }
}

void StrigidEngine::AcquireAndProvideGPUResources()
{
    STRIGID_ZONE_N("Main_AcquireGPU");

    if (!Pacer.BeginFrame())
        return;

    // Acquire command buffer and swapchain texture
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(GpuDevice);
    if (!cmdBuf)
    {
        // No command buffer available, skip this frame
        return;
    }
    // Provide to RenderThread via Render->ProvideGPUResources(cmd, swapchain)
    SDL_GPUTexture* swapchainTex;
    if (!SDL_AcquireGPUSwapchainTexture(cmdBuf, EngineWindow, &swapchainTex, nullptr, nullptr) || !swapchainTex)
    {
        // Failed to acquire texture
        SDL_CancelGPUCommandBuffer(cmdBuf);
        return;
    }

    Render->ProvideGPUResources(cmdBuf, swapchainTex);
}

void StrigidEngine::SubmitRenderCommands()
{
    STRIGID_ZONE_N("Main_SubmitGPU");

    // Retrieve command buffer from RenderThread
    SDL_GPUCommandBuffer* cmdBuf = Render->TakeCommandBuffer();
    if (!cmdBuf)
    {
        LOG_ERROR("[Main] Failed to take command buffer from RenderThread");
        return;
    }

    Pacer.EndFrame(cmdBuf);
    Render->NotifyFrameSubmitted();
}

void StrigidEngine::CalculateFPS()
{
    FrameCount++;
    const double currentTime = SDL_GetPerformanceCounter() / static_cast<double>(SDL_GetPerformanceFrequency());
    FpsTimer += currentTime - LastFPSCheck;
    LastFPSCheck = currentTime;

    if (FpsTimer >= 1.0) [[unlikely]]
    {
        double fps = FrameCount / FpsTimer;
        double ms = (FpsTimer / FrameCount) * 1000.0;

        LOG_DEBUG_F("Main FPS: %d | Frame: %.2fms", static_cast<int>(fps), ms);

        FrameCount = 0;
        FpsTimer = 0;
    }
}

void StrigidEngine::WaitForTiming(uint64_t frameStart, uint64_t perfFrequency)
{
    STRIGID_ZONE_N("Main_WaitTiming");

    const uint64_t targetTicks = static_cast<uint64_t>(1.0 / Config.InputPollHz * static_cast<double>(perfFrequency));
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
