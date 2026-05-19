// cube.vert.slang — Vertex shader.
//
// All data arrives via buffer device address (BDA) — no vertex-attribute
// bindings are used.  A single uint64_t push constant carries the address of
// GpuFrameData, from which the shader obtains:
//   - VerticesAddr        — packed Vertex[N] (32 bytes each, oct-encoded normals)
//   - InstancesAddr       — SoA compact instances written by scatter.slang
//   - Position/Rotation   — current camera state (quat, position, FoV, AspectRatio)
//   - OldPosition/OldRotation — previous-frame camera state for interpolation
//   - Alpha               — sub-frame interpolation weight [0, 1]
//
// The VP matrix is reconstructed each vertex from the interpolated camera state.
// Rotation is stored as quaternion (x,y,z,w). Entity quats are normalized before
// use to correct for linear interpolation drift from scatter.slang.
//
// SoA read for instance instIdx:
//   field k = semantic - 1;  value = InstancesAddr[ k * OutFieldStride + instIdx ]
//
// Skeletal meshes:
//   scatter always writes entity cache index at slot SemEntityCacheIdx-1 = 16.
//   SkeletalIdxByEntityAddr[cacheIdx] gives compact skeletal index (UINT32_MAX = static).
//   SkinMatrix[skelIdx * MAX_BONES_PER_ENTITY + boneIdx] is world-space (entity TRS baked in
//   by skinning.slang), so skeletal entities skip buildModel and use LBS directly.
//   SkinWeights are parallel to the global vertex buffer, indexed by globalVertIdx.

#include "common/GpuFrameData.slang"

[[vk::push_constant]]
cbuffer PushConstants { uint64_t gFrameDataAddr; }

// 32-byte vertex matching Vertex in VertexFormat.h.
// float3 pos (12B) + oct normal uint32 (4B) + float2 UV (8B) +
// oct tangent uint32 (4B) + uint16 mask (2B) + uint8 flags (1B) + uint8 pad (1B)
struct Vertex
{
    float3   pos;           // 12 bytes
    uint     n_oct16x2;     //  4 bytes — octahedral-encoded normal
    float2   uv;            //  8 bytes
    uint     t_oct16x2;     //  4 bytes — octahedral-encoded tangent direction
    uint     mask_flags;    //  4 bytes — mask(16) + flags(8) + pad(8)
};

struct VSOut
{
    float4 pos      : SV_Position;
    float4 color    : COLOR0;
    float3 worldNrm : NORMAL0;
#ifdef TNX_GPU_PICKING
    nointerpolation uint entityCacheIdx : ENTITYID0;
#endif
};

// Decode octahedral-encoded normal from packed uint32 (2× snorm16).
float3 octDecode(uint packed)
{
    float2 e = float2(
        float(int(packed << 16) >> 16) / 32767.0,
        float(int(packed) >> 16) / 32767.0
    );
    float3 n = float3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0)
    {
        float2 s = float2(n.x >= 0 ? 1 : -1, n.y >= 0 ? 1 : -1);
        n.xy = (1.0 - abs(n.yx)) * s;
    }
    return normalize(n);
}

// Build a TRS model matrix from translation t, unit quaternion q (x,y,z,w),
// and per-axis scale s.  Returns a row-major float4x4 (Slang M[row][col]).
//
// Quaternion is normalized before use to handle interpolation drift.
// Rotation matrix R from q, then M[row][col] = R[row,col] * scale[col].
float4x4 buildModel(float3 t, float4 q, float3 s)
{
    // Normalize to handle lerp drift from scatter pass
    float invLen = rsqrt(dot(q, q));
    q *= invLen;

    float x = q.x, y = q.y, z = q.z, w = q.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    float4x4 m;

    // Row 0
    m[0][0] = (1.0f - (yy + zz)) * s.x;
    m[0][1] = (xy - wz)          * s.y;
    m[0][2] = (xz + wy)          * s.z;
    m[0][3] = t.x;

    // Row 1
    m[1][0] = (xy + wz)          * s.x;
    m[1][1] = (1.0f - (xx + zz)) * s.y;
    m[1][2] = (yz - wx)          * s.z;
    m[1][3] = t.y;

    // Row 2
    m[2][0] = (xz - wy)          * s.x;
    m[2][1] = (yz + wx)          * s.y;
    m[2][2] = (1.0f - (xx + yy)) * s.z;
    m[2][3] = t.z;

    // Row 3
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;

    return m;
}

// Rotate vector v by unit quaternion q: v + 2w(q×v) + 2(q×(q×v))
float3 quatRotate(float4 q, float3 v)
{
    float3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

// Build a row-major view matrix from camera world position and orientation quaternion.
// Camera looks down local -Z; rows are (right, up, -fwd) with translation folded in.
float4x4 buildView(float3 pos, float4 q)
{
    float3 right = quatRotate(q, float3(1, 0, 0));
    float3 up    = quatRotate(q, float3(0, 1, 0));
    float3 fwd   = quatRotate(q, float3(0, 0, -1));

    float4x4 v;
    v[0] = float4(right.x, right.y, right.z, -dot(right, pos));
    v[1] = float4(up.x,    up.y,    up.z,    -dot(up,    pos));
    v[2] = float4(-fwd.x,  -fwd.y,  -fwd.z,   dot(fwd,   pos));
    v[3] = float4(0, 0, 0, 1);
    return v;
}

// Build a row-major Vulkan projection matrix (reversed-depth, Y-flipped).
// fov is full vertical field of view in radians.
float4x4 buildProj(float fov, float aspect, float zNear, float zFar)
{
    float F  = 1.0 / tan(fov * 0.5);
    float dz = zNear - zFar;

    float4x4 p = float4x4(0);
    p[0][0] =  F / aspect;
    p[1][1] = -F;                        // flip Y for Vulkan NDC
    p[2][2] =  zFar / dz;
    p[2][3] =  (zFar * zNear) / dz;
    p[3][2] = -1.0;
    return p;
}

// NLERP for quaternions — good enough for one-frame camera smoothing.
float4 quatNlerp(float4 a, float4 b, float t)
{
    if (dot(a, b) < 0.0) b = -b;        // ensure shortest-arc
    return normalize(lerp(a, b, t));
}

float4x4 buildViewProj(GpuFrameData* fd)
{
    const float alpha = fd->Alpha;

    float3 pos = lerp(float3(fd->OldPosition[0], fd->OldPosition[1], fd->OldPosition[2]),
                      float3(fd->Position[0],    fd->Position[1],    fd->Position[2]), alpha);

    float4 rot = quatNlerp(float4(fd->OldRotation[0], fd->OldRotation[1],
                                  fd->OldRotation[2], fd->OldRotation[3]),
                           float4(fd->Rotation[0],    fd->Rotation[1],
                                  fd->Rotation[2],    fd->Rotation[3]), alpha);

    float fov = lerp(fd->OldFoV, fd->FoV, alpha);

    // fov stored in degrees → convert to radians
    fov = fov * 3.14159265 / 180.0;

    return mul(buildProj(fov, fd->AspectRatio, 0.1, 5000.0), buildView(pos, rot));
}

[shader("vertex")]
VSOut main(uint vertIdx : SV_VertexID, uint baseVert : SV_StartVertexLocation,
           uint instIdx : SV_VulkanInstanceID)
{
    GpuFrameData* fd = (GpuFrameData*)gFrameDataAddr;

    // --- Mesh vertex (BDA) ---
    // Slang's SV_VertexID = gl_VertexIndex - BaseVertex (DX12 convention).
    // Since we index the mega-buffer via BDA, we need the full gl_VertexIndex
    // which includes VertexOffset from the indirect draw command.
    uint   globalVtxIdx = vertIdx + baseVert;
    Vertex vtx          = ((Vertex*)fd->VerticesAddr)[globalVtxIdx];
    float3 localNormal  = octDecode(vtx.n_oct16x2);

    // --- Per-instance SoA data ---
    // Layout: slot k for instance i  =  InstancesAddr[ k * OutFieldStride + i ]
    float* inst   = (float*)fd->InstancesAddr;
    uint   stride = fd->OutFieldStride;

    float3 pos   = float3(inst[(SemPosX   - 1u) * stride + instIdx],
                          inst[(SemPosY   - 1u) * stride + instIdx],
                          inst[(SemPosZ   - 1u) * stride + instIdx]);

    float4 rotQ  = float4(inst[(SemRotQx  - 1u) * stride + instIdx],
                          inst[(SemRotQy  - 1u) * stride + instIdx],
                          inst[(SemRotQz  - 1u) * stride + instIdx],
                          inst[(SemRotQw  - 1u) * stride + instIdx]);

    float3 scale = float3(inst[(SemScaleX - 1u) * stride + instIdx],
                          inst[(SemScaleY - 1u) * stride + instIdx],
                          inst[(SemScaleZ - 1u) * stride + instIdx]);

    float4 color = float4(inst[(SemColorR - 1u) * stride + instIdx],
                          inst[(SemColorG - 1u) * stride + instIdx],
                          inst[(SemColorB - 1u) * stride + instIdx],
                          inst[(SemColorA - 1u) * stride + instIdx]);

    // Entity cache index is always-on at slot SemEntityCacheIdx-1 = 16.
    uint cacheIdx = asuint(inst[(SemEntityCacheIdx - 1u) * stride + instIdx]);

    // --- Skeletal index lookup ---
    // UINT32_MAX means this entity has no skeleton; take the static TRS path.
    uint skelIdx = 0xFFFFFFFFu;
    if (fd->SkeletalIdxByEntityAddr != 0)
        skelIdx = ((uint*)fd->SkeletalIdxByEntityAddr)[cacheIdx];

    float4x4 vp = buildViewProj(fd);
    float4 worldPos;
    float3 worldNormal;

    if (skelIdx != 0xFFFFFFFFu && fd->SkinWeightAddr != 0)
    {
        // ---------------------------------------------------------------
        // LBS path — skeletal entity.
        // SkinWeights (8B each) are parallel to the global vertex buffer.
        // Packed as: [boneIndices: uint8×4][boneWeights: UNORM8×4]
        // SkinMatrices are world-space (entity TRS already baked in by
        // skinning.slang), so we do NOT apply the TRS model matrix here.
        // ---------------------------------------------------------------
        uint* skinRaw  = (uint*)fd->SkinWeightAddr + globalVtxIdx * 2u;
        uint  idxPack  = skinRaw[0]; // boneIndices[0..3] packed as 4× uint8
        uint  wgtPack  = skinRaw[1]; // boneWeights[0..3] packed as 4× UNORM8

        float4x4* skinMats = (float4x4*)fd->SkinMatrixAddr;
        uint      matBase  = skelIdx * MAX_BONES_PER_ENTITY;

        worldPos    = float4(0.0, 0.0, 0.0, 0.0);
        worldNormal = float3(0.0, 0.0, 0.0);

        for (uint i = 0u; i < 4u; ++i)
        {
            uint rawW = (wgtPack >> (i * 8u)) & 0xFFu;
            if (rawW == 0u) continue;

            float    w    = float(rawW) / 255.0;
            uint     bone = (idxPack >> (i * 8u)) & 0xFFu;
            float4x4 sm   = skinMats[matBase + bone];

            worldPos += w * mul(sm, float4(vtx.pos, 1.0));

            // Transform normal by the rotation part of the skin matrix (upper-left 3×3).
            // Uses the same matrix as position — correct for rigid-bone skinning without shear.
            float3x3 m3 = float3x3(sm[0].xyz, sm[1].xyz, sm[2].xyz);
            worldNormal += w * mul(m3, localNormal);
        }
        worldNormal = normalize(worldNormal);
    }
    else
    {
        // ---------------------------------------------------------------
        // Static TRS path — non-skeletal entity.
        // ---------------------------------------------------------------
        float4x4 model = buildModel(pos, rotQ, scale);
        worldPos   = mul(model, float4(vtx.pos, 1.0));
        worldNormal = localNormal + 2.0 * cross(rotQ.xyz, cross(rotQ.xyz, localNormal) + rotQ.w * localNormal);
    }

    float4 clipPos = mul(vp, worldPos);

#ifdef TNX_DEBUG_RENDERING
    if (fd->DebugDrawMode == 1u) // skin path
        color = (skelIdx != 0xFFFFFFFFu && fd->SkinWeightAddr != 0)
                    ? float4(0.0, 1.0, 1.0, 1.0)  // cyan  = skeletal
                    : float4(0.0, 1.0, 0.0, 1.0); // green = static
#endif

    VSOut o;
    o.pos      = clipPos;
    o.color    = color;
    o.worldNrm = worldNormal;
#ifdef TNX_GPU_PICKING
    o.entityCacheIdx = cacheIdx;
#endif
    return o;
}
