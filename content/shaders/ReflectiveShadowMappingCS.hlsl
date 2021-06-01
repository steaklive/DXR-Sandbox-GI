#include "Common.hlsl"

#define RSM_SAMPLES_COUNT 512

Texture2D<float4> worldPosLSBuffer : register(t0);   // light space
Texture2D<float4> normalLSBuffer : register(t1);     // light space
Texture2D<float4> fluxLSBuffer : register(t2);       // light space

Texture2D<float4> worldPosWSBuffer : register(t3);   // world space
Texture2D<float4> normalWSBuffer : register(t4);     // world space

RWTexture2D<float4> Output : register(u0);

SamplerState RSMSampler : register(s0);

cbuffer RSMConstantBuffer : register(b0)
{
    float4x4 ShadowViewProjection;
    float RSMIntensity;
    float RSMRMax;
    float2 UpsampleRatio;
};
cbuffer RSMConstantBuffer2 : register(b1)
{
    float4 xi[RSM_SAMPLES_COUNT];
}

float3 CalculateRSM(float3 pos, float3 normal)
{
    float4 texSpacePos = mul(ShadowViewProjection, float4(pos, 1.0f));
    texSpacePos.rgb /= texSpacePos.w;
    texSpacePos.rg = texSpacePos.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    
    float3 indirectIllumination = float3(0.0, 0.0, 0.0);
    
    uint width = 0;
    uint height = 0;
    uint nol = 0;
    normalLSBuffer.GetDimensions(0, width, height, nol);
    
    for (int i = 0; i < RSM_SAMPLES_COUNT; i++)
    {
        float2 coord = texSpacePos.rg + RSMRMax * float2(xi[i].x * sin(2.0f * PI * xi[i].y), xi[i].x * cos(2.0f * PI * xi[i].y));
        if (coord.x < 0.0f || coord.x > 1.0f || coord.y < 0.0f || coord.y > 1.0f)
            continue;
        
        float2 texcoord = coord * float2(width, height);
        float3 vplPosWS = worldPosLSBuffer.Load(uint3(uint2(texcoord), 0)).rgb;
        float3 vplNormalWS = normalLSBuffer.SampleLevel(RSMSampler, coord, 0.0f).rgb;
        float3 flux = fluxLSBuffer.Load(uint3(uint2(texcoord), 0)).rgb;
                
        float3 vplPosDir = (pos - vplPosWS);

        float3 res = flux * ((max(0.0, dot(vplNormalWS, vplPosDir)) * max(0.0, dot(normal, -vplPosDir))) / (dot(vplPosDir, vplPosDir) * dot(vplPosDir, vplPosDir)));
        res *= xi[i].x * xi[i].x;
        
        indirectIllumination += res;
    }
    
    return indirectIllumination;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{    
    float4 normalWS = normalWSBuffer[DTid.xy * UpsampleRatio];
    float4 worldPosWS = worldPosWSBuffer[DTid.xy * UpsampleRatio];
    
    Output[DTid.xy] = saturate(float4(CalculateRSM(worldPosWS.rgb, normalWS.rgb), 1.0f));
}
