#include "Common.hlsl"

Texture2D<float4> worldPosLSBuffer : register(t0); // light space
Texture2D<float4> normalLSBuffer : register(t1); // light space
Texture2D<float4> fluxLSBuffer : register(t2); // light space

struct RSMTexel
{
    float3 flux;
    float3 normalWS;
    float3 positionWS;
};

cbuffer RSMDownsampleCB : register(b0)
{
    float4 LightDir;
    int ScaleSize;
};

struct VS_INPUT
{
    float4 position : POSITION;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
};

struct PS_OUTPUT
{
    float4 worldPosDownsampled : SV_Target0;
    float4 normalDownsampled : SV_Target1;
    float4 fluxDownsampled : SV_Target2;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT result;
    result.position = float4(input.position.xyz, 1);
    return result;
}

RSMTexel GetRSMTexel(uint2 coords)
{
    RSMTexel texel = (RSMTexel) 0;
    texel.positionWS = worldPosLSBuffer.Load(int3(coords, 0)).xyz;
    texel.normalWS = normalLSBuffer.Load(int3(coords, 0)).xyz;
    texel.flux = fluxLSBuffer.Load(int3(coords, 0)).xyz;
    return texel;
}

float GetLuminance(RSMTexel texel)
{
    return (texel.flux.r * 0.299f + texel.flux.g * 0.587f + texel.flux.b * 0.114f) + max(0.0f, dot(texel.normalWS, -LightDir.rgb));
}

PS_OUTPUT PSMain(VS_OUTPUT input)
{
    PS_OUTPUT output = (PS_OUTPUT) 0;
    uint2 rsmCoords = uint2(input.position.x, input.position.y);
    
    RSMTexel resultTexel = (RSMTexel) 0;
    uint numSamples = 0;
    float maxLuminance = 0.0f;
    int3 brightestCellPos = int3(0, 0, 0);
    
    for (int y = 0; y < ScaleSize; y++)
    {
        for (int x = 0; x < ScaleSize; x++)
        {
            int2 texIdx = rsmCoords.xy * ScaleSize + int2(x, y);
            RSMTexel rsmTexel = GetRSMTexel(texIdx);
            float texLum = GetLuminance(rsmTexel);
            if (texLum > maxLuminance)
            {
                brightestCellPos = int3(rsmTexel.positionWS * LPV_SCALE + float3(LPV_DIM_HALF, LPV_DIM_HALF, LPV_DIM_HALF));
                maxLuminance = texLum;
            }
        }
    }
    
    for (int y = 0; y < ScaleSize; y++)
    {
        for (int x = 0; x < ScaleSize; x++)
        {
            int2 texIdx = rsmCoords.xy * ScaleSize + int2(x, y);
            RSMTexel rsmTexel = GetRSMTexel(texIdx);
            int3 texelPos = int3(rsmTexel.positionWS * LPV_SCALE + float3(LPV_DIM_HALF, LPV_DIM_HALF, LPV_DIM_HALF));
            int3 deltaPos = texelPos - brightestCellPos;
            if (dot(deltaPos, deltaPos) < 10)
            {
                resultTexel.flux += rsmTexel.flux;
                resultTexel.positionWS += rsmTexel.positionWS;
                resultTexel.normalWS += rsmTexel.normalWS;
                numSamples++;
            }
        }
    }
    
    // normalize
    if (numSamples > 0)
    {
        resultTexel.positionWS /= numSamples;
        resultTexel.normalWS /= numSamples;
        resultTexel.normalWS = normalize(resultTexel.normalWS);
        resultTexel.flux /= numSamples;
    }
    
    output.worldPosDownsampled = float4(resultTexel.positionWS, 1.0f);
    output.normalDownsampled = float4(resultTexel.normalWS, 1.0f);
    output.fluxDownsampled = float4(resultTexel.flux, 1.0f);
    return output;
}