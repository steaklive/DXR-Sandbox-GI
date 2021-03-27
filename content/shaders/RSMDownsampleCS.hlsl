#include "Common.hlsl"
#define DOWNSAMPLE_SIZE 4

Texture2D<float4> worldPosLSBuffer : register(t0); // light space
Texture2D<float4> normalLSBuffer : register(t1); // light space
Texture2D<float4> fluxLSBuffer : register(t2); // light space

RWTexture2D<float4> worldPosDownsampled : register(u0);
RWTexture2D<float4> normalDownsampled : register(u1);
RWTexture2D<float4> fluxDownsampled : register(u2);

cbuffer RSMDownsampleCB : register(b0)
{
    float4 LightDir;
    int ScaleSize;
};
struct RSMTexel
{
    float3 flux;
    float3 normalWS;
    float3 positionWS;
};

float GetLuminance(RSMTexel texel)
{
    return (texel.flux.r * 0.299f + texel.flux.g * 0.587f + texel.flux.b * 0.114f) + max(0.0f, dot(texel.normalWS, -LightDir.rgb));
}

RSMTexel GetRSMTexel(uint2 coords)
{
    RSMTexel texel = (RSMTexel) 0;
    texel.positionWS = worldPosLSBuffer.Load(int3(coords, 0)).xyz;
    texel.normalWS = normalLSBuffer.Load(int3(coords, 0)).xyz;
    texel.flux = fluxLSBuffer.Load(int3(coords, 0)).xyz;
    return texel;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    RSMTexel groupTexels[DOWNSAMPLE_SIZE * DOWNSAMPLE_SIZE];
    
    uint2 rsmCoords = DTid.xy;
    
    RSMTexel resultTexel = (RSMTexel) 0;
    uint numSamples = 0;
    float maxLuminance = 0.0f;
    int3 brightestCellPos = int3(0, 0, 0);
    
    for (int y = 0; y < DOWNSAMPLE_SIZE; y++)
    {
        for (int x = 0; x < DOWNSAMPLE_SIZE; x++)
        {
            int2 texIdx = rsmCoords.xy * DOWNSAMPLE_SIZE + int2(x, y);
            groupTexels[y * DOWNSAMPLE_SIZE + x] = GetRSMTexel(texIdx);
            float texLum = GetLuminance(groupTexels[y * DOWNSAMPLE_SIZE + x]);
            if (texLum > maxLuminance)
            {
                brightestCellPos = int3(groupTexels[y * DOWNSAMPLE_SIZE + x].positionWS * LPV_SCALE + float3(LPV_DIM_HALF, LPV_DIM_HALF, LPV_DIM_HALF));
                maxLuminance = texLum;
            }            
        }
    }
    
    //GroupMemoryBarrierWithGroupSync();
    
    for (int g = 0; g < DOWNSAMPLE_SIZE * DOWNSAMPLE_SIZE; g++)
    {
        int3 texelPos = int3(groupTexels[g].positionWS * LPV_SCALE + float3(LPV_DIM_HALF, LPV_DIM_HALF, LPV_DIM_HALF));
        int3 deltaPos = texelPos - brightestCellPos;
        if (dot(deltaPos, deltaPos) < 10)
        {
            resultTexel.flux += groupTexels[g].flux;
            resultTexel.positionWS += groupTexels[g].positionWS;
            resultTexel.normalWS += groupTexels[g].normalWS;
            numSamples++;
        }
    }
    
    //GroupMemoryBarrierWithGroupSync();
    
    // normalize
    if (numSamples > 0)
    {
        resultTexel.positionWS /= numSamples;
        resultTexel.normalWS /= numSamples;
        resultTexel.normalWS = normalize(resultTexel.normalWS);
        resultTexel.flux /= numSamples;
    }
    
    worldPosDownsampled[DTid.xy] = float4(resultTexel.positionWS, 1.0f);
    normalDownsampled[DTid.xy] = float4(resultTexel.normalWS, 1.0f);
    fluxDownsampled[DTid.xy] = float4(resultTexel.flux, 1.0f);
}
