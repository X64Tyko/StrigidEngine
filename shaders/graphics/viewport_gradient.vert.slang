// Full-screen triangle for the viewport background gradient.
// Positions and UVs are generated from SV_VertexID — no vertex buffer needed.
// Vertex 0: NDC(-1,-1), UV(0,0)   Vertex 1: NDC(3,-1), UV(2,0)   Vertex 2: NDC(-1,3), UV(0,2)
// The visible area is [-1,1]×[-1,1] NDC, corresponding to UV [0,1]×[0,1].

struct VsOut
{
    float4 Position : SV_Position;
    float2 Uv       : TEXCOORD0;
};

[shader("vertex")]
VsOut main(uint vid : SV_VertexID)
{
    float2 uv = float2(float(vid & 1u) * 2.0f, float(vid >> 1u) * 2.0f);
    VsOut o;
    o.Uv       = uv;
    o.Position = float4(uv * 2.0f - float2(1.0f, 1.0f), 0.0f, 1.0f);
    return o;
}
