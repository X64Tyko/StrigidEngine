// Radial vignette background for editor viewports.
// Colors are read from GpuFrameData.BgInnerColor / BgOuterColor (linear space).

#include "common/GpuFrameData.slang"

[[vk::push_constant]]
cbuffer Push { uint64_t FrameDataAddr; };

[shader("fragment")]
float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    GpuFrameData* frame = (GpuFrameData*)FrameDataAddr;
    float4 inner = float4(frame->BgInnerColor[0], frame->BgInnerColor[1],
                          frame->BgInnerColor[2], frame->BgInnerColor[3]);
    float4 outer = float4(frame->BgOuterColor[0], frame->BgOuterColor[1],
                          frame->BgOuterColor[2], frame->BgOuterColor[3]);
    float dist = length(uv - float2(0.5f, 0.5f)) * 2.0f;
    float t    = smoothstep(0.0f, 1.0f, saturate(dist));
    return lerp(inner, outer, t);
}
