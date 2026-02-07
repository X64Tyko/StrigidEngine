# Week 4 Completion - Manual Fix Required

## Window.cpp is corrupted

**Issue**: Lines 327-333+ are duplicate/broken code from failed sed operations.

**Solution**: 
1. Delete everything in Window.cpp after line 318 (the closing brace of CreateRenderPipeline)
2. Add the complete DrawInstances implementation (see below)

### Complete DrawInstances Implementation

```cpp
void Window::DrawInstances(const InstanceData* instances, uint32_t count)
{
    STRIGID_ZONE_C(STRIGID_COLOR_RENDERING);
    
    if (count == 0 || !m_Pipeline)
        return;
    
    // Resize instance buffer if needed
    if (count > m_InstanceBufferCapacity)
    {
        if (m_InstanceBuffer)
        {
            SDL_ReleaseGPUBuffer(gpuDevice, m_InstanceBuffer);
        }
        CreateInstanceBuffer(count * 2);
    }
    
    // Upload instance data to GPU
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = sizeof(InstanceData) * count;
    
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);
    void* mapped = SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false);
    std::memcpy(mapped, instances, sizeof(InstanceData) * count);
    SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);
    
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
    
    SDL_GPUTransferBufferLocation src = {};
    src.transfer_buffer = transferBuffer;
    src.offset = 0;
    
    SDL_GPUBufferRegion dst = {};
    dst.buffer = m_InstanceBuffer;
    dst.offset = 0;
    dst.size = sizeof(InstanceData) * count;
    
    SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
    SDL_EndGPUCopyPass(copyPass);
    
    // Get swapchain texture
    SDL_GPUTexture* swapchainTex;
    if (!SDL_AcquireGPUSwapchainTexture(cmdBuf, window, &swapchainTex, nullptr, nullptr))
    {
        SDL_SubmitGPUCommandBuffer(cmdBuf);
        SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);
        return;
    }
    
    if (swapchainTex)
    {
        SDL_GPUColorTargetInfo colorTarget = {};
        colorTarget.texture = swapchainTex;
        colorTarget.clear_color = {0.1f, 0.1f, 0.1f, 1.0f};
        colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;
        
        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTarget, 1, nullptr);
        
        SDL_BindGPUGraphicsPipeline(renderPass, m_Pipeline);
        
        SDL_GPUBufferBinding vertexBinding = {};
        vertexBinding.buffer = m_VertexBuffer;
        vertexBinding.offset = 0;
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
        
        SDL_GPUBufferBinding instanceBinding = {};
        instanceBinding.buffer = m_InstanceBuffer;
        instanceBinding.offset = 0;
        SDL_BindGPUVertexBuffers(renderPass, 1, &instanceBinding, 1);
        
        SDL_GPUBufferBinding indexBinding = {};
        indexBinding.buffer = m_IndexBuffer;
        indexBinding.offset = 0;
        SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        
        SDL_DrawGPUIndexedPrimitives(renderPass, CubeMesh::IndexCount, count, 0, 0, 0);
        
        SDL_EndGPURenderPass(renderPass);
    }
    
    SDL_SubmitGPUCommandBuffer(cmdBuf);
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);
}
```

## Next Steps After Fixing Window.cpp

1. Update Window::Render() to call DrawInstances with test data
2. Create 1000 test entities in Main.cpp
3. Add movement system to StrigidEngine::FixedUpdate()
4. Verify 60fps with Tracy

Let me know when Window.cpp is fixed and I'll continue with the remaining implementation.
