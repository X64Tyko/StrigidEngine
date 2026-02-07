#pragma once

#include <cstdint>
#include <vector>

// Forward declarations
struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUBuffer;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUShader;

// Instance data format for GPU upload
struct InstanceData
{
    float PositionX, PositionY, PositionZ;
    float RotationX, RotationY, RotationZ;
    float ScaleX, ScaleY, ScaleZ;
    float ColorR, ColorG, ColorB, ColorA;
};

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
    void DrawInstances(const InstanceData* Instances, size_t Count);
    
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
    SDL_GPUBuffer* InstanceBuffer = nullptr;
    SDL_GPUShader* VertexShader = nullptr;
    SDL_GPUShader* FragmentShader = nullptr;
    
    int Width = 0;
    int Height = 0;
    size_t InstanceBufferCapacity = 0;
    
    bool bInitialized = false;
    
    // Setup rendering pipeline
    void CreateRenderPipeline();
    void CreateCubeMesh();
    void CreateInstanceBuffer(size_t Capacity);
};
