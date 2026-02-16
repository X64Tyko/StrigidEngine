#include "RenderThread.h"
#include "LogicThread.h"
#include "FramePacket.h"
#include "Registry.h"
#include "EngineConfig.h"
#include "RenderCommandBuffer.h" // For InstanceData definition
#include "Profiler.h"
#include "Logger.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <cstring>
#include <iostream>

#include "ColorData.h"
#include "CubeMesh.h"
#include "Transform.h"
#include "CompiledShaders.h"

void RenderThread::Initialize(Registry* registry, LogicThread* logic, const EngineConfig* config, SDL_GPUDevice* device,
                              SDL_Window* window)
{
    RegistryPtr = registry;
    LogicPtr = logic;
    ConfigPtr = config;
    GpuDevice = device;
    EngineWindow = window;

    LOG_INFO("[RenderThread] Initialized");
}

void RenderThread::Start()
{
    CreateCubeMesh();
    CreateRenderPipeline();
    bIsRunning.store(true, std::memory_order_release);
    Thread = std::thread(&RenderThread::ThreadMain, this);
    LOG_INFO("[RenderThread] Started");
}

void RenderThread::Stop()
{
    bIsRunning.store(false, std::memory_order_release);
    LOG_INFO("[RenderThread] Stop requested");
}

void RenderThread::Join()
{
    auto threadID = Thread.get_id();
    if (Thread.joinable())
    {
        Thread.join();
        LOG_INFO("[RenderThread] Joined");
    }

    // Cleanup transfer buffer
    if (TransferBuffer)
    {
        SDL_ReleaseGPUTransferBuffer(GpuDevice, TransferBuffer);
        TransferBuffer = nullptr;
    }
}

void RenderThread::ProvideGPUResources(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swapchain)
{
    CmdBufferAtomic.store(cmd, std::memory_order_release);
    SwapchainTextureAtomic.store(swapchain, std::memory_order_release);
    bNeedsGPUResources.store(false, std::memory_order_release);

    LOG_TRACE("[RenderThread] GPU resources provided");
}

SDL_GPUCommandBuffer* RenderThread::TakeCommandBuffer()
{
    SDL_GPUCommandBuffer* cmd = CmdBufferAtomic.exchange(nullptr, std::memory_order_acq_rel);
    bReadyToSubmit.store(false, std::memory_order_release);

    LOG_TRACE("[RenderThread] Command buffer taken for submission");
    return cmd;
}

void RenderThread::ThreadMain()
{
    // Allocate our own VisualPacket (third packet in triple buffer)
    auto visualPacket = std::make_shared<FramePacket>();

    // TODO: Cache sparse array pointers (once they exist)
    // TransformArrayPtr = RegistryPtr->GetSparseArray<Transform>();
    // ColorArrayPtr = RegistryPtr->GetSparseArray<ColorData>();

    while (bIsRunning.load(std::memory_order_acquire))
    {
        STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);

        // FPS tracking - measure at start of frame
        if (LastFpsCounter == 0)
        {
            LastFpsCounter = SDL_GetPerformanceCounter();
        }

        uint64_t currentCounter = SDL_GetPerformanceCounter();
        uint64_t counterElapsed = currentCounter - LastFpsCounter;
        double dt = static_cast<double>(counterElapsed) / static_cast<double>(SDL_GetPerformanceFrequency());
        LastFpsCounter = currentCounter;

        FpsFrameCount++;
        FpsTimer += dt;

        if (FpsTimer >= 1.0)
        {
            double fps = FpsFrameCount / FpsTimer;
            double ms = (FpsTimer / FpsFrameCount) * 1000.0;

            LOG_DEBUG_F("Render FPS: %d | Frame: %.2fms", static_cast<int>(fps), ms);

            FpsFrameCount = 0;
            FpsTimer = 0.0;
        }

        // Don't start another frame if the previous one hasn't been submitted
        while (!bFrameSubmitted.load(std::memory_order_acquire) && bIsRunning.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        bFrameSubmitted.store(false, std::memory_order_release);

        // Poll mailbox for new frame - exchange our visualPacket with LogicThread's mailbox
        std::shared_ptr<FramePacket> newPacket = LogicPtr->ExchangeMailbox(visualPacket);
        if (newPacket->FrameNumber > LastFrameNumber)
        {
            visualPacket = newPacket;
            CurrentFramePacket = visualPacket;
            STRIGID_ZONE_N("Render_NewFrame");
            LastFrameNumber = visualPacket->FrameNumber;

            // Snapshot sparse arrays using cached pointers
            SnapshotSparseArrays(visualPacket);
        }

        // TODO: temp safety until snapshot interp is a bit smarter.
        if (SnapshotCurrent.size() == 0)
        {
            bFrameSubmitted.store(true, std::memory_order_release);
            continue;
        }

        // Request GPU resources early (before interpolation work)
        RequestGPUResources();

        // Calculate our own interpolation alpha (RenderThread may be faster than LogicThread)
        float alpha = CalculateInterpolationAlpha();

        if (!InterpolateToTransferBuffer(alpha))
        {
            WaitForCommandBuffer(); // Still need to wait for resources
            WaitForSwapchainTexture();
            SignalReadyToSubmit(); // Submit empty/partial frame
            continue;
        }

        WaitForCommandBuffer();

        if (!BuildCopyPassAndUniforms())
        {
            WaitForSwapchainTexture();
            SignalReadyToSubmit(); // Submit partial frame
            continue;
        }

        WaitForSwapchainTexture();

        BuildRenderPass();

        SignalReadyToSubmit();
    }

    visualPacket = nullptr;
}

void RenderThread::ResizeTransferBuffer(size_t NewSize)
{
    // Release old buffer if it exists
    if (TransferBuffer)
    {
        SDL_ReleaseGPUTransferBuffer(GpuDevice, TransferBuffer);
        TransferBuffer = nullptr;
    }

    size_t allocSize = NewSize;

    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = static_cast<uint32_t>(allocSize);

    TransferBuffer = SDL_CreateGPUTransferBuffer(GpuDevice, &transferInfo);
    if (!TransferBuffer)
    {
        LOG_ERROR_F("[RenderThread] Failed to create transfer buffer of size %zu bytes", allocSize);
        TransferBufferCapacity = 0;
        return;
    }

    TransferBufferCapacity = allocSize;
    LOG_INFO_F("[RenderThread] Transfer buffer resized to %zu bytes", TransferBufferCapacity);
}

void RenderThread::ResizeInstanceBuffer(size_t NewSize)
{
    // Release old buffer if it exists
    if (InstanceBuffer)
    {
        SDL_ReleaseGPUBuffer(GpuDevice, InstanceBuffer);
        InstanceBuffer = nullptr;
    }

    size_t allocCount = NewSize;

    SDL_GPUBufferCreateInfo bufferInfo = {};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferInfo.size = sizeof(InstanceData) * static_cast<uint32_t>(allocCount);

    InstanceBuffer = SDL_CreateGPUBuffer(GpuDevice, &bufferInfo);
    if (!InstanceBuffer)
    {
        LOG_ERROR_F("[RenderThread] Failed to create instance buffer for %zu instances", allocCount);
        InstanceBufferCapacity = 0;
        return;
    }

    InstanceBufferCapacity = allocCount;
    LOG_INFO_F("[RenderThread] Instance buffer resized to %zu instances", InstanceBufferCapacity);
}

void RenderThread::SnapshotSparseArrays(std::shared_ptr<FramePacket> packet)
{
    STRIGID_ZONE_N("Render_Snapshot");

    uint32_t entityCount = packet->ActiveEntityCount;
    // Resize snapshot buffer
    SnapshotCurrent.resize(entityCount);
    std::vector<Archetype*> archetypes = RegistryPtr->Query<Transform, ColorData>();

    size_t writeIdx = 0;
    constexpr size_t MAX_FIELD_ARRAYS = 256;

    for (Archetype* arch : archetypes)
    {
        for (size_t chunkIdx = 0; chunkIdx < arch->Chunks.size(); ++chunkIdx)
        {
            Chunk* chunk = arch->Chunks[chunkIdx];
            uint32_t chunkEntityCount = arch->GetChunkCount(chunkIdx);

            if (chunkEntityCount == 0)
                continue;

            // Build field array table
            void* fieldArrayTable[MAX_FIELD_ARRAYS];
            arch->BuildFieldArrayTable(chunk, fieldArrayTable);

            // Get Transform field arrays (indices 0-11)
            auto posXArray = static_cast<float*>(fieldArrayTable[0]);
            auto posYArray = static_cast<float*>(fieldArrayTable[1]);
            auto posZArray = static_cast<float*>(fieldArrayTable[2]);
            auto rotXArray = static_cast<float*>(fieldArrayTable[4]);
            auto rotYArray = static_cast<float*>(fieldArrayTable[5]);
            auto rotZArray = static_cast<float*>(fieldArrayTable[6]);
            auto scaleXArray = static_cast<float*>(fieldArrayTable[8]);
            auto scaleYArray = static_cast<float*>(fieldArrayTable[9]);
            auto scaleZArray = static_cast<float*>(fieldArrayTable[10]);

            // Get ColorData field arrays (starts at index 12 for CubeEntity)
            auto rArray = static_cast<float*>(fieldArrayTable[12]);
            auto gArray = static_cast<float*>(fieldArrayTable[13]);
            auto bArray = static_cast<float*>(fieldArrayTable[14]);
            auto aArray = static_cast<float*>(fieldArrayTable[15]);

            // Copy data to snapshot
            for (uint32_t i = 0; i < chunkEntityCount; ++i)
            {
                SnapshotEntry& entry = SnapshotCurrent[writeIdx++];

                // Copy transform data
                entry.PositionX = posXArray[i];
                entry.PositionY = posYArray[i];
                entry.PositionZ = posZArray[i];
                entry.RotationX = rotXArray[i];
                entry.RotationY = rotYArray[i];
                entry.RotationZ = rotZArray[i];
                entry.ScaleX = scaleXArray[i];
                entry.ScaleY = scaleYArray[i];
                entry.ScaleZ = scaleZArray[i];

                // Copy color data
                entry.ColorR = rArray[i];
                entry.ColorG = gArray[i];
                entry.ColorB = bArray[i];
                entry.ColorA = aArray[i];
            }
        }
    }
}

void RenderThread::RequestGPUResources()
{
    bNeedsGPUResources.store(true, std::memory_order_release);
}

void RenderThread::WaitForGPUResources()
{
    STRIGID_ZONE_N("Render_WaitGPU");

    // Spin-wait for main thread to provide resources
    while (CmdBufferAtomic.load(std::memory_order_acquire) == nullptr && bIsRunning.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
}

float RenderThread::CalculateInterpolationAlpha()
{
    // RenderThread may run faster than LogicThread (144Hz vs 60Hz)
    // We need to calculate our own alpha based on Logic's accumulator
    double accumulator = LogicPtr->GetAccumulator();
    double fixedStep = ConfigPtr->GetFixedStepTime();

    if (fixedStep <= 0.0)
        return 1.0f;

    float alpha = static_cast<float>(accumulator / fixedStep);
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    return alpha;
}

bool RenderThread::InterpolateToTransferBuffer(float alpha)
{
    STRIGID_ZONE_N("Render_Interpolate");
    size_t entityCount = SnapshotCurrent.size();

    if (entityCount == 0)
        return false;

    // Calculate required size
    size_t requiredSize = sizeof(InstanceData) * entityCount;
    if (requiredSize > TransferBufferCapacity)
    {
        ResizeTransferBuffer(requiredSize);
    }

    // Map transfer buffer
    void* mapped = SDL_MapGPUTransferBuffer(GpuDevice, TransferBuffer, true); // true = cycle
    if (!mapped)
    {
        LOG_ERROR("[RenderThread] Failed to map transfer buffer");
        return false;
    }

    auto instances = static_cast<InstanceData*>(mapped);

    // Interpolate between SnapshotPrevious and SnapshotCurrent
    for (size_t i = 0; i < entityCount; ++i)
    {
        const SnapshotEntry& prev = (i < SnapshotPrevious.size()) ? SnapshotPrevious[i] : SnapshotCurrent[i];
        const SnapshotEntry& curr = SnapshotCurrent[i];

        // Lerp position
        instances[i].PositionX = prev.PositionX + (curr.PositionX - prev.PositionX) * alpha;
        instances[i].PositionY = prev.PositionY + (curr.PositionY - prev.PositionY) * alpha;
        instances[i].PositionZ = prev.PositionZ + (curr.PositionZ - prev.PositionZ) * alpha;

        // Lerp rotation
        instances[i].RotationX = prev.RotationX + (curr.RotationX - prev.RotationX) * alpha;
        instances[i].RotationY = prev.RotationY + (curr.RotationY - prev.RotationY) * alpha;
        instances[i].RotationZ = prev.RotationZ + (curr.RotationZ - prev.RotationZ) * alpha;

        // Lerp scale
        instances[i].ScaleX = prev.ScaleX + (curr.ScaleX - prev.ScaleX) * alpha;
        instances[i].ScaleY = prev.ScaleY + (curr.ScaleY - prev.ScaleY) * alpha;
        instances[i].ScaleZ = prev.ScaleZ + (curr.ScaleZ - prev.ScaleZ) * alpha;

        // Copy color (no interpolation)
        instances[i].ColorR = curr.ColorR;
        instances[i].ColorG = curr.ColorG;
        instances[i].ColorB = curr.ColorB;
        instances[i].ColorA = curr.ColorA;
    }

    SDL_UnmapGPUTransferBuffer(GpuDevice, TransferBuffer);

    //LOG_DEBUG_F("[RenderThread] Interpolated %zu instances (alpha=%.3f)", entityCount, alpha);

    return true;
}

void RenderThread::WaitForCommandBuffer()
{
    STRIGID_ZONE_N("Render_CmdBuf");

    // Spin-wait for main thread to provide resources
    while (CmdBufferAtomic.load(std::memory_order_acquire) == nullptr && bIsRunning.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
}

bool RenderThread::BuildCopyPassAndUniforms()
{
    SDL_GPUCommandBuffer* cmdBuf = CmdBufferAtomic.load(std::memory_order_acquire);
    size_t entityCount = SnapshotCurrent.size();

    if (entityCount == 0 || !Pipeline)
    {
        LOG_WARN("[RenderThread] No entities or pipeline missing");
        return false;
    }

    // Calculate required size for instance data
    size_t requiredSize = sizeof(InstanceData) * entityCount;

    // 1. Begin Copy Pass - Upload transfer buffer to instance buffer
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    if (requiredSize > InstanceBufferCapacity)
    {
        ResizeInstanceBuffer(requiredSize);
    }

    SDL_GPUTransferBufferLocation src = {};
    src.transfer_buffer = TransferBuffer;
    src.offset = 0;

    SDL_GPUBufferRegion dst = {};
    dst.buffer = InstanceBuffer;
    dst.offset = 0;
    dst.size = static_cast<uint32_t>(requiredSize);

    SDL_UploadToGPUBuffer(copyPass, &src, &dst, true);
    SDL_EndGPUCopyPass(copyPass);

    // 2. Push vertex uniforms (view/projection matrix from FramePacket)
    if (CurrentFramePacket)
    {
        SDL_PushGPUVertexUniformData(cmdBuf, 0, CurrentFramePacket->View.ProjectionMatrix.m,
                                     sizeof(CurrentFramePacket->View.ProjectionMatrix.m));
    }
    else
    {
        // Fallback: identity matrix
        float ViewProjMatrix[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        SDL_PushGPUVertexUniformData(cmdBuf, 0, ViewProjMatrix, sizeof(ViewProjMatrix));
    }

    CmdBufferAtomic.store(cmdBuf, std::memory_order_release);

    return true;
}

void RenderThread::WaitForSwapchainTexture()
{
    STRIGID_ZONE_N("Render_Swapchain");

    // Spin-wait for main thread to provide resources
    while (SwapchainTextureAtomic.load(std::memory_order_acquire) == nullptr && bIsRunning.load(
        std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
}

void RenderThread::BuildRenderPass()
{
    size_t entityCount = SnapshotCurrent.size();

    SDL_GPUCommandBuffer* cmdBuf = CmdBufferAtomic.load(std::memory_order_acquire);
    SDL_GPUTexture* swapchainTex = SwapchainTextureAtomic.load(std::memory_order_acquire);

    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = swapchainTex;
    colorTarget.clear_color = {0.5f, 0.0f, 0.1f, 1.0f};
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTarget, 1, nullptr);

    // 4. Bind pipeline and buffers
    SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);

    SDL_GPUBufferBinding vertexBinding = {};
    vertexBinding.buffer = VertexBuffer;
    vertexBinding.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

    SDL_GPUBufferBinding instanceBinding = {};
    instanceBinding.buffer = InstanceBuffer;
    instanceBinding.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 1, &instanceBinding, 1);

    SDL_GPUBufferBinding indexBinding = {};
    indexBinding.buffer = IndexBuffer;
    indexBinding.offset = 0;
    SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    // 5. Draw indexed primitives (36 indices for cube)
    SDL_DrawGPUIndexedPrimitives(renderPass, 36, static_cast<uint32_t>(entityCount), 0, 0, 0);

    // 6. End render pass
    SDL_EndGPURenderPass(renderPass);

    CmdBufferAtomic.store(cmdBuf, std::memory_order_release);
    SwapchainTextureAtomic.store(swapchainTex, std::memory_order_release);
}

void RenderThread::SignalReadyToSubmit()
{
    // Store command buffer back in atomic for main thread to retrieve
    // (already stored, just signal)
    bReadyToSubmit.store(true, std::memory_order_release);

    LOG_TRACE("[RenderThread] Signaled ready to submit");
}

void RenderThread::CreateCubeMesh()
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

void RenderThread::CreateInstanceBuffer(size_t Capacity)
{
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);

    InstanceBufferCapacity = Capacity;

    SDL_GPUBufferCreateInfo bufferInfo = {};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferInfo.size = sizeof(InstanceData) * static_cast<uint32_t>(Capacity);

    InstanceBuffer = SDL_CreateGPUBuffer(GpuDevice, &bufferInfo);
    if (!InstanceBuffer)
    {
        std::cerr << "Failed to create instance buffer: " << SDL_GetError() << std::endl;
    }
}

void RenderThread::CreateRenderPipeline()
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
    vertexAttributes[2].offset = 16; // Changed from 12 due to padding
    vertexAttributes[2].buffer_slot = 1;

    // Location 3: instance scale (vec3)
    vertexAttributes[3].location = 3;
    vertexAttributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[3].offset = 32; // Changed from 24 due to padding
    vertexAttributes[3].buffer_slot = 1;

    // Location 4: instance color (vec4)
    vertexAttributes[4].location = 4;
    vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    vertexAttributes[4].offset = 48; // Changed from 36 due to padding
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
