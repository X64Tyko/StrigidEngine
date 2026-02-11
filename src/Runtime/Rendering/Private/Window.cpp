#include "Window.h"
#include "Profiler.h"
#include "RenderCommandBuffer.h"

#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <cmath>

Window::Window()
{
    bInitialized = false;
}

Window::~Window()
{
    if (!bInitialized)
    {
        Shutdown();
    }
}

int Window::Open(const char* title, int w, int h)
{
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);

    Width = w;
    Height = h;

    // Initialize SDL (should already be initialized by Engine)
    if (!SDL_WasInit(SDL_INIT_VIDEO))
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            std::cerr << "SDL Init Failed: " << SDL_GetError() << std::endl;
            return -1;
        }
    }

    // Create Window
    EngineWindow = SDL_CreateWindow(title, w, h, SDL_WINDOW_RESIZABLE);
    if (!EngineWindow)
    {
        std::cerr << "Window Create Failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Create GPU Device
    GpuDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (!GpuDevice)
    {
        std::cerr << "GPU Device Failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(EngineWindow);
        return -1;
    }

    // Claim the Window for the Device
    if (!SDL_ClaimWindowForGPUDevice(GpuDevice, EngineWindow))
    {
        std::cerr << "Claim Window Failed: " << SDL_GetError() << std::endl;
        SDL_DestroyGPUDevice(GpuDevice);
        SDL_DestroyWindow(EngineWindow);
        return -1;
    }
    
    if (!SDL_SetGPUSwapchainParameters(GpuDevice, EngineWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX))
    {
        std::cerr << "Swapchain Parameters Failed: " << SDL_GetError() << std::endl;
        SDL_DestroyGPUDevice(GpuDevice);
        SDL_DestroyWindow(EngineWindow);
        return -1;
    }

    // Initialize rendering pipeline
    CreateCubeMesh();
    for (int i = 0; i < FramePacer::FRAMES_IN_FLIGHT; ++i)
    {
        CreateInstanceBuffer(2000, i);
    }
    CreateRenderPipeline();
    FramePacer.Initialize(GpuDevice);

    bInitialized = true;
    return 0;
}

void Window::Render()
{
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);
    // A. Acquire Command Buffer
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(GpuDevice);
    if (!cmdBuf)
    {
        // No command buffer available, skip this frame
        return;
    }

    // B. Get the Swapchain Texture (The Window Surface)
    SDL_GPUTexture* swapchainTex;
    if (!SDL_AcquireGPUSwapchainTexture(cmdBuf, EngineWindow, &swapchainTex, nullptr, nullptr) || !swapchainTex)
    {
        // Failed to get texture (minimized?), just submit empty and continue
        SDL_CancelGPUCommandBuffer(cmdBuf);
        return;
    }

    // C. Setup Render Pass (Clear to Dark Gray)
    SDL_GPUColorTargetInfo colorTargetInfo = {};
    colorTargetInfo.texture = swapchainTex;
    colorTargetInfo.clear_color = {0.1f, 0.1f, 0.1f, 1.0f};
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTargetInfo, 1, nullptr);

    // (Draw commands go here later)

    SDL_EndGPURenderPass(renderPass);

    // D. Submit and Present
    SDL_SubmitGPUCommandBuffer(cmdBuf);
}

void Window::Shutdown()
{
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);

    if (!bInitialized)
        return;

    // Cleanup GPU resources
    for (SDL_GPUTransferBuffer*& TransferBuffer : TransferBuffers)
    {
        SDL_ReleaseGPUTransferBuffer(GpuDevice, TransferBuffer);
        TransferBuffer = nullptr;
    }

    if (EngineWindow && GpuDevice)
    {
        SDL_ReleaseWindowFromGPUDevice(GpuDevice, EngineWindow);
    }

    if (GpuDevice)
    {
        SDL_DestroyGPUDevice(GpuDevice);
        GpuDevice = nullptr;
    }

    if (EngineWindow)
    {
        SDL_DestroyWindow(EngineWindow);
        EngineWindow = nullptr;
    }

    bInitialized = false;
}

void Window::SetTitle(const char* title)
{
    SDL_SetWindowTitle(EngineWindow, title);
}

#include "CubeMesh.h"
#include "CompiledShaders.h"
#include <cstring>

void Window::CreateCubeMesh()
{
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);
    
    // Create vertex buffer
    SDL_GPUBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertexBufferInfo.size = sizeof(CubeMesh::Vertices);
    
    VertexBuffer = SDL_CreateGPUBuffer(GpuDevice, &vertexBufferInfo);
    if (!VertexBuffer)
    {
        std::cerr << "Failed to create vertex buffer: " << SDL_GetError() << std::endl;
        return;
    }
    
    // Upload vertex data
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = sizeof(CubeMesh::Vertices);
    
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(GpuDevice, &transferInfo);
    void* mapped = SDL_MapGPUTransferBuffer(GpuDevice, transferBuffer, true); // true = cycle (wait if needed)
    std::memcpy(mapped, CubeMesh::Vertices, sizeof(CubeMesh::Vertices));
    SDL_UnmapGPUTransferBuffer(GpuDevice, transferBuffer);

    SDL_GPUCommandBuffer* uploadCmd = SDL_AcquireGPUCommandBuffer(GpuDevice);
    // In initialization, we must get a command buffer - if this fails, something is seriously wrong
    if (!uploadCmd)
    {
        SDL_ReleaseGPUTransferBuffer(GpuDevice, transferBuffer);
        std::cerr << "Failed to acquire command buffer during initialization: " << SDL_GetError() << std::endl;
        return;
    }
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmd);
    
    SDL_GPUTransferBufferLocation src = {};
    src.transfer_buffer = transferBuffer;
    src.offset = 0;
    
    SDL_GPUBufferRegion dst = {};
    dst.buffer = VertexBuffer;
    dst.offset = 0;
    dst.size = sizeof(CubeMesh::Vertices);
    
    SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmd);
    
    SDL_ReleaseGPUTransferBuffer(GpuDevice, transferBuffer);
    
    // Create index buffer (similar process)
    SDL_GPUBufferCreateInfo indexBufferInfo = {};
    indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    indexBufferInfo.size = sizeof(CubeMesh::Indices);
    
    IndexBuffer = SDL_CreateGPUBuffer(GpuDevice, &indexBufferInfo);
    
    // Upload index data
    transferInfo.size = sizeof(CubeMesh::Indices);
    transferBuffer = SDL_CreateGPUTransferBuffer(GpuDevice, &transferInfo);
    mapped = SDL_MapGPUTransferBuffer(GpuDevice, transferBuffer, true); // true = cycle (wait if needed)
    std::memcpy(mapped, CubeMesh::Indices, sizeof(CubeMesh::Indices));
    SDL_UnmapGPUTransferBuffer(GpuDevice, transferBuffer);

    uploadCmd = SDL_AcquireGPUCommandBuffer(GpuDevice);
    // In initialization, we must get a command buffer - if this fails, something is seriously wrong
    if (!uploadCmd)
    {
        SDL_ReleaseGPUTransferBuffer(GpuDevice, transferBuffer);
        std::cerr << "Failed to acquire command buffer during initialization: " << SDL_GetError() << std::endl;
        return;
    }
    copyPass = SDL_BeginGPUCopyPass(uploadCmd);
    
    src.transfer_buffer = transferBuffer;
    src.offset = 0;
    
    dst.buffer = IndexBuffer;
    dst.offset = 0;
    dst.size = sizeof(CubeMesh::Indices);
    
    SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmd);
    
    SDL_ReleaseGPUTransferBuffer(GpuDevice, transferBuffer);
}

void Window::CreateInstanceBuffer(size_t Capacity, int BufferIndex)
{
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);
    
    InstanceBufferCapacities[BufferIndex] = Capacity;
    
    SDL_GPUBufferCreateInfo bufferInfo = {};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferInfo.size = sizeof(InstanceData) * static_cast<uint32_t>(Capacity);
    
    InstanceBuffers[BufferIndex] = SDL_CreateGPUBuffer(GpuDevice, &bufferInfo);
    if (!InstanceBuffers[BufferIndex])
    {
        std::cerr << "Failed to create instance buffer: " << SDL_GetError() << std::endl;
    }
}

void Window::CreateRenderPipeline()
{
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);
    
    // Create vertex shader
    SDL_GPUShaderCreateInfo vertShaderInfo = {};
    vertShaderInfo.code = (const uint8_t*)CompiledShaders::VertexShader;
    vertShaderInfo.code_size = CompiledShaders::VertexShaderSize;
    vertShaderInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vertShaderInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    vertShaderInfo.entrypoint = "main";
    vertShaderInfo.num_samplers = 0;
    vertShaderInfo.num_storage_textures = 0;
    vertShaderInfo.num_storage_buffers = 0;
    vertShaderInfo.num_uniform_buffers = 1;
    
    VertexShader = SDL_CreateGPUShader(GpuDevice, &vertShaderInfo);
    if (!VertexShader)
    {
        std::cerr << "Failed to create vertex shader: " << SDL_GetError() << std::endl;
        return;
    }
    
    // Create fragment shader
    SDL_GPUShaderCreateInfo fragShaderInfo = {};
    fragShaderInfo.code = (const uint8_t*)CompiledShaders::FragmentShader;
    fragShaderInfo.code_size = CompiledShaders::FragmentShaderSize;
    fragShaderInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fragShaderInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    fragShaderInfo.entrypoint = "main";
    fragShaderInfo.num_samplers = 0;
    fragShaderInfo.num_storage_textures = 0;
    fragShaderInfo.num_storage_buffers = 0;
    fragShaderInfo.num_uniform_buffers = 0;
    
    FragmentShader = SDL_CreateGPUShader(GpuDevice, &fragShaderInfo);
    if (!FragmentShader)
    {
        std::cerr << "Failed to create fragment shader: " << SDL_GetError() << std::endl;
        return;
    }
    
    std::cout << "Shaders created successfully!" << std::endl;
    
    // Define vertex attributes
    SDL_GPUVertexAttribute vertexAttributes[5] = {};
    
    // Location 0: vertex position (vec3)
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[0].offset = 0;
    vertexAttributes[0].buffer_slot = 0;
    
    // Location 1: instance position (vec3)
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[1].offset = 0;
    vertexAttributes[1].buffer_slot = 1;
    
    // Location 2: instance rotation (vec3)
    vertexAttributes[2].location = 2;
    vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[2].offset = 16;  // Changed from 12 due to padding
    vertexAttributes[2].buffer_slot = 1;

    // Location 3: instance scale (vec3)
    vertexAttributes[3].location = 3;
    vertexAttributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[3].offset = 32;  // Changed from 24 due to padding
    vertexAttributes[3].buffer_slot = 1;

    // Location 4: instance color (vec4)
    vertexAttributes[4].location = 4;
    vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    vertexAttributes[4].offset = 48;  // Changed from 36 due to padding
    vertexAttributes[4].buffer_slot = 1;

    // Define vertex buffers
    SDL_GPUVertexBufferDescription vertexBuffers[2] = {};

    // Buffer 0: per-vertex (position)
    vertexBuffers[0].slot = 0;
    vertexBuffers[0].pitch = sizeof(CubeMesh::Vertex);
    vertexBuffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBuffers[0].instance_step_rate = 0;
    
    // Buffer 1: per-instance (transform + color)
    vertexBuffers[1].slot = 1;
    vertexBuffers[1].pitch = sizeof(InstanceData);
    vertexBuffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
    vertexBuffers[1].instance_step_rate = 0;
    
    SDL_GPUVertexInputState vertexInputState = {};
    vertexInputState.vertex_buffer_descriptions = vertexBuffers;
    vertexInputState.num_vertex_buffers = 2;
    vertexInputState.vertex_attributes = vertexAttributes;
    vertexInputState.num_vertex_attributes = 5;
    
    // Color target
    SDL_GPUColorTargetDescription colorTarget = {};
    colorTarget.format = SDL_GetGPUSwapchainTextureFormat(GpuDevice, EngineWindow);
    
    // Graphics pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.vertex_shader = VertexShader;
    pipelineInfo.fragment_shader = FragmentShader;
    pipelineInfo.vertex_input_state = vertexInputState;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipelineInfo.target_info.num_color_targets = 1;
    pipelineInfo.target_info.color_target_descriptions = &colorTarget;
    
    Pipeline = SDL_CreateGPUGraphicsPipeline(GpuDevice, &pipelineInfo);
    if (!Pipeline)
    {
        std::cerr << "Failed to create pipeline: " << SDL_GetError() << std::endl;
    }
    else
    {
        std::cout << "Graphics pipeline created successfully!" << std::endl;
    }
}

void Window::DrawInstances(const InstanceData* Instances, size_t Count, const uint8_t* WrapStart, size_t WrapCount)
{
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);
    
    if (Count == 0 || !Pipeline)
        return;
    
    // Frame pacing
    FramePacer.BeginFrame();

    int frame_index = FramePacer.GetFrameIndex();
    auto InstanceBuffer = InstanceBuffers[frame_index];
    // Resize instance buffer if needed
    if (Count > InstanceBufferCapacities[frame_index] || !InstanceBuffer)
    {
        STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);
        if (InstanceBuffer)
        {
            SDL_ReleaseGPUBuffer(GpuDevice, InstanceBuffer);
        }
        CreateInstanceBuffer(Count * 2, frame_index);
        InstanceBuffer = InstanceBuffers[frame_index];  // Update local variable
    }

    // Upload instance data to GPU using cached transfer buffer
    size_t requiredSize = sizeof(InstanceData) * Count;

    auto TransferBuffer = TransferBuffers[frame_index];
    // Resize transfer buffer if needed (with 2x growth strategy)
    if (requiredSize > TransferBufferCapacities[frame_index] || !TransferBuffer)
    {
        STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);
        if (TransferBuffer)
        {
            SDL_ReleaseGPUTransferBuffer(GpuDevice, TransferBuffer);
        }

        TransferBufferCapacities[frame_index] = requiredSize * 2;  // Over-allocate to avoid frequent resizes

        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<uint32_t>(TransferBufferCapacities[frame_index]);

        TransferBuffers[frame_index] = SDL_CreateGPUTransferBuffer(GpuDevice, &transferInfo);
        TransferBuffer = TransferBuffers[frame_index];  // Update local variable
    }
    
    // Acquire command buffer and swapchain texture FIRST (fail fast if unavailable)
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(GpuDevice);
    if (!cmdBuf)
    {
        // No command buffer available, skip this frame
        return;
    }

    void* mapped = SDL_MapGPUTransferBuffer(GpuDevice, TransferBuffer, true);
    if (!mapped)
    {
        // GPU is still using the buffer, skip this frame
        SDL_CancelGPUCommandBuffer(cmdBuf);
        return;
    }
    
    if (WrapStart)
    {
        STRIGID_ZONE_N("Wrapping Buffer");
        std::memcpy(mapped, Instances, sizeof(uint8_t) * WrapCount);
        std::memcpy(mapped, WrapStart, sizeof(uint8_t) * (requiredSize - WrapCount));
    }
    else
    {
        std::memcpy(mapped, Instances, requiredSize);   
    }
    SDL_UnmapGPUTransferBuffer(GpuDevice, TransferBuffer);

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTransferBufferLocation src = {};
    src.transfer_buffer = TransferBuffer;  // Use cached transfer buffer
    src.offset = 0;

    SDL_GPUBufferRegion dst = {};
    dst.buffer = InstanceBuffer;
    dst.offset = 0;
    dst.size = static_cast<uint32_t>(requiredSize);

    SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
    SDL_EndGPUCopyPass(copyPass);
    
    // Create simple perspective camera matrix
    static float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    static float Fov = 60.0f * 3.14159f / 180.0f; // 60 degrees in radians
    static float ZNear = 0.1f;
    static float ZFar = 1000.0f;
    
    // Perspective projection matrix (column-major for GLSL)
    static float F = 1.0f / std::tan(Fov / 2.0f);
    static float ViewProjMatrix[16] = {
        // Column 0
        F / AspectRatio, 0.0f, 0.0f, 0.0f,
        // Column 1
        0.0f, F, 0.0f, 0.0f,  // Note: No Y-flip, SDL3 handles NDC conversion
        // Column 2
        0.0f, 0.0f, ZFar / (ZFar - ZNear), -(ZFar * ZNear) / (ZFar - ZNear),
        // Column 3
        0.0f, 0.0f, 1.0f, 0.0f
    };
    
    // Push uniform data to shader BEFORE beginning render pass (mat4 = 64 bytes)
    SDL_PushGPUVertexUniformData(cmdBuf, 0, ViewProjMatrix, sizeof(ViewProjMatrix));

    SDL_GPUTexture* swapchainTex;
    // Wait for swapchain texture (blocks until vsync/available)
    if (!SDL_AcquireGPUSwapchainTexture(cmdBuf, EngineWindow, &swapchainTex, nullptr, nullptr) || !swapchainTex)
    {
        // Failed to acquire texture
        SDL_CancelGPUCommandBuffer(cmdBuf);
        return;
    }
    
    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = swapchainTex;
    colorTarget.clear_color = {0.1f, 0.1f, 0.1f, 1.0f};
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;
    
    
    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTarget, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);
    
    
    SDL_GPUBufferBinding vertexBinding = {VertexBuffer, 0};
    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
    
    SDL_GPUBufferBinding instanceBinding = {InstanceBuffer, 0};
    SDL_BindGPUVertexBuffers(renderPass, 1, &instanceBinding, 1);
    
    SDL_GPUBufferBinding indexBinding = {IndexBuffer, 0};
    SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    
    SDL_DrawGPUIndexedPrimitives(renderPass, CubeMesh::IndexCount, static_cast<uint32_t>(Count), 0, 0, 0);
    
    SDL_EndGPURenderPass(renderPass);

    FramePacer.EndFrame(cmdBuf);
}
