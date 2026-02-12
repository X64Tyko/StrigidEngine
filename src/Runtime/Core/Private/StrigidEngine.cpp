#include "StrigidEngine.h"

#include <iostream>

#include "LogicThread.h"
#include "RenderThread.h"
#include "Window.h"
#include "Registry.h"
#include "EngineConfig.h"
#include "CubeEntity.h"
#include "Profiler.h"
#include "Logger.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <random>

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

bool StrigidEngine::Initialize(const char* title, int width, int height)
{
    STRIGID_ZONE_N("Engine_Init");

    Logger::Get().Init("StrigidEngine.log", LogLevel::Error);
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
    
    if (!SDL_SetGPUSwapchainParameters(GpuDevice, EngineWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX))
    {
        std::cerr << "Swapchain Parameters Failed: " << SDL_GetError() << std::endl;
        SDL_DestroyGPUDevice(GpuDevice);
        SDL_DestroyWindow(EngineWindow);
        return false;
    }

    // Create Registry
    RegistryPtr = std::make_unique<Registry>();
    Pacer.Initialize(GpuDevice);

    // Create 100k test entities (same as old code)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posX(-30.0f, 30.0f);
    std::uniform_real_distribution<float> posY(-30.0f, 30.0f);
    std::uniform_real_distribution<float> posZ(-500.0f, -200.0f);
    std::uniform_real_distribution<float> color(0.2f, 1.0f);

    for (int i = 0; i < 1000000; ++i)
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
        cube.color->B = color(gen);
        cube.color->R = color(gen);
        cube.color->G = color(gen);
        cube.color->A = color(gen);
    }

    LOG_ALWAYS("Created 100000 test entities");

    // Create threads
    Logic = std::make_unique<LogicThread>();
    Render = std::make_unique<RenderThread>();

    // Initialize threads
    Logic->Initialize(RegistryPtr.get(), &Config, width, height);

    // TODO: Get GPU device from Window
    // For now, pass nullptr - this will need to be fixed
    Render->Initialize(RegistryPtr.get(), Logic.get(), &Config, GpuDevice, EngineWindow);

    // Start threads
    Logic->Start();
    Render->Start();

    bIsRunning = true;
    return true;
}

void StrigidEngine::Run()
{
    bIsRunning = true;

    const uint64_t perfFrequency = SDL_GetPerformanceFrequency();
    uint64_t lastCounter = SDL_GetPerformanceCounter();

    //const double targetFrameTime = Config.GetTargetFrameTime();

    while (bIsRunning.load(std::memory_order_acquire))
    {
        STRIGID_ZONE_N("Main_Frame");

        const uint64_t frameStartCounter = SDL_GetPerformanceCounter();
        uint64_t counterElapsed = frameStartCounter - lastCounter;
        lastCounter = frameStartCounter;

        double dt = static_cast<double>(counterElapsed) / static_cast<double>(perfFrequency);

        // Pump events early
        PumpEvents();

        // Service render thread (check for GPU resource requests or submit commands)
        ServiceRenderThread();

        // Frame limiter - not here bro!
        /*
        if (targetFrameTime > 0.0)
        {
            WaitForTiming(frameStartCounter, perfFrequency);
        }
        */

        // FPS tracking
        STRIGID_FRAME_MARK();
        CalculateFPS(dt);
        
        std::this_thread::sleep_for(std::chrono::microseconds(1));
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

    // TODO: Get GPU device from Window
    // TODO: Access FramePacer from Window - FramePacer.BeginFrame() to wait for fence
    if (!Pacer.BeginFrame())
        return;

    // TODO: Acquire command buffer and swapchain texture
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(GpuDevice);
    if (!cmdBuf)
    {
        // No command buffer available, skip this frame
        return;
    }
    // TODO: Provide to RenderThread via Render->ProvideGPUResources(cmd, swapchain)
    SDL_GPUTexture* swapchainTex;
    if (!SDL_AcquireGPUSwapchainTexture(cmdBuf, EngineWindow, &swapchainTex, nullptr, nullptr) || !swapchainTex)
    {
        // Failed to acquire texture
        SDL_CancelGPUCommandBuffer(cmdBuf);
        return;
    }
    
    Render->ProvideGPUResources(cmdBuf, swapchainTex);

    LOG_TRACE("[Main] GPU resources acquisition - TODO: Implement");
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

    LOG_TRACE("[Main] Submitted command buffer to GPU - TODO: Implement");
}

void StrigidEngine::CalculateFPS(double dt)
{
    FrameCount++;
    FpsTimer += dt;

    if (FpsTimer >= 1.0)
    {
        double fps = FrameCount / FpsTimer;
        double ms = (FpsTimer / FrameCount) * 1000.0;

        LOG_ALWAYS_F("Main FPS: %d | Frame: %.2fms", static_cast<int>(fps), ms);

        FrameCount = 0;
        FpsTimer = 0.0;
    }
}
