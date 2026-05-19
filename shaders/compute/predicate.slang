// predicate.slang — Pass 1 of 3.
//
// Reads the active bit (bit 31) from each entity's flags and the MeshID.
// An entity is GPU-visible only when active AND meshID != 0. MeshID == 0
// means the mesh has not yet loaded; the entity stays alive in logic and
// physics but is invisible to the GPU until the slot arrives via Checkout.
//
// Flags are read from CurrFieldAddrs[0] (SemFlags, always field index 0).
// MeshID is read from CurrFieldAddrs[SemMeshID - 1] (field index 15).

#include "common/GpuFrameData.slang"

[[vk::push_constant]]
cbuffer PushConstants { uint64_t gFrameDataAddr; }

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    GpuFrameData* fd = (GpuFrameData*)gFrameDataAddr;

    uint i = tid.x;
    if (i >= fd->EntityCount)
        return;

    // Flags live in the field slab at CurrFieldAddrs[0] (SemFlags, always index 0).
    uint flags  = ((uint*)fd->CurrFieldAddrs[0])[i];
    uint active = (flags >> 31u) & 1u;

    // MeshID == 0 means the mesh slot is not yet populated; skip this entity.
    // All entity types (including skeletal) carry CMeshRef, so this is the single
    // visibility gate. Must match the scatter shader's early-exit condition exactly.
    uint64_t meshFieldAddr = fd->CurrFieldAddrs[SemMeshID - 1u];
    uint meshID = meshFieldAddr != 0 ? ((uint*)meshFieldAddr)[i] : 0u;

    // Write raw predicate into scan[i].  prefix_sum will overwrite in-place.
    ((uint*)fd->ScanAddr)[i] = active & (meshID != 0u ? 1u : 0u);
}
