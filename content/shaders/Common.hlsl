static const float FLT_MAX = asfloat(0x7F7FFFFF);

cbuffer LightParams : register(b1)
{
    float4 LightDirection;
    float4 LightColor;
    float LightIntensity;
    float ShadowIntensity;
    float2 pad;
}

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);

struct Payload
{
    bool skipShading;
    float rayHitT;
};

struct ShadowPayload
{
    bool isHit;
};

float3 ReconstructWorldPosFromDepth(float2 uv, float depth, float4x4 invProj, float4x4 invView)
{
    //float ndcX = uv.x * 2 - 1;
    //float ndcY = 1 - uv.y * 2; // Remember to flip y!!!
    float4 viewPos = mul(float4( /*ndcX*/ uv.x, /*ndcY*/ uv.y, depth, 1.0f), invProj);
    viewPos = viewPos / viewPos.w;
    return mul(invView, viewPos).xyz;
}

struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 texcoord;
};
