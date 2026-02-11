#pragma once

#include <cstdint>
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include "../Private/FramePacer.h"

struct InstanceData;
// Forward declarations
struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUBuffer;
struct SDL_GPUTransferBuffer;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUShader;
class RenderCommandBuffer;


class Window
{
public:
    Window();
    ~Window();
    
    int Open(const char* Title, int W, int H);
    void Render();
    void Shutdown();
    
    void SetTitle(const char* Title);
    
    // Instanced rendering API
    void DrawInstances(const InstanceData* Instances, size_t Count, const uint8_t* WrapStart = nullptr, size_t WrapCount = 0);
    
    // Get screen dimensions
    int GetWidth() const { return Width; }
    int GetHeight() const { return Height; }

private:
    SDL_Window* EngineWindow = nullptr;
    SDL_GPUDevice* GpuDevice = nullptr;

    // Rendering pipeline resources
    SDL_GPUGraphicsPipeline* Pipeline = nullptr;
    SDL_GPUBuffer* VertexBuffer = nullptr;
    SDL_GPUBuffer* IndexBuffer = nullptr;
    SDL_GPUShader* VertexShader = nullptr;
    SDL_GPUShader* FragmentShader = nullptr;
    FramePacer FramePacer;

    // Per-frame buffers (tied to FramePacer::FRAMES_IN_FLIGHT)
    SDL_GPUBuffer* InstanceBuffers[FramePacer::FRAMES_IN_FLIGHT] = {nullptr};
    SDL_GPUTransferBuffer* TransferBuffers[FramePacer::FRAMES_IN_FLIGHT] = {nullptr};
    size_t InstanceBufferCapacities[FramePacer::FRAMES_IN_FLIGHT] = {0};
    size_t TransferBufferCapacities[FramePacer::FRAMES_IN_FLIGHT] = {0};
    int CurrentFrameIndex = 0;

    int Width = 0;
    int Height = 0;

    bool bInitialized = false;

    // Setup rendering pipeline
    void CreateRenderPipeline();
    void CreateCubeMesh();
    void CreateInstanceBuffer(size_t Capacity, int BufferIndex);
};
