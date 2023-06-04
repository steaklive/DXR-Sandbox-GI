#include "Common.hlsl"

#define SSAO_MAX_KERNEL_OFFSETS 16

struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct PSOutput
{
    float4 color : SV_Target0;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

    result.position = float4(input.position.xyz, 1);
    result.uv = input.uv;

    return result;
}

cbuffer SSAOCB : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 InvView;
    float4x4 InvProj;
    float4 KernelOffsets[SSAO_MAX_KERNEL_OFFSETS];
    float4 Radius_Power_NoiseScale;
    float4 ScreenSize;
};

Texture2D<float4> normalsBuffer : register(t0);
Texture2D<float3> randomVectorBuffer : register(t1);
Texture2D<float> depthBuffer : register(t2);

SamplerState LinearSampler : register(s0);
SamplerState WrapSampler : register(s1);

PSOutput PSMain(PSInput input)
{
    PSOutput output = (PSOutput) 0;
    
    float3 normal = normalsBuffer[input.position.xy].rgb;
    float depth = LinearizeDepth(depthBuffer[input.position.xy], InvProj);
    
    float2 uv = input.position.xy / ScreenSize.xy;
    float3 worldPosition = ReconstructViewPosFromDepth(uv, depth, InvProj);
    float3 randomVector = randomVectorBuffer.Sample(WrapSampler, uv * Radius_Power_NoiseScale.zw).rgb;
    
    float3 tangent = normalize(randomVector - normal * dot(randomVector, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);
    
    float occlusion = 0.0f;
    for (int i = 0; i < (int) SSAO_MAX_KERNEL_OFFSETS; i++)
    {
        float3 samplePos = mul(KernelOffsets[i].xyz, TBN);
        samplePos = samplePos * Radius_Power_NoiseScale.x + worldPosition;
        
        float3 sampleDir = normalize(samplePos - worldPosition);
        float factor = max(dot(normal, sampleDir), 0);

        float4 offset = mul(float4(samplePos, 1.0f), Proj);
        offset.xy /= offset.w;
        
        float sampleD = depthBuffer.Sample(LinearSampler, offset.xy);
        sampleD = ReconstructViewPosFromDepth(offset.xy, sampleD, InvProj).z;
        
        float range = smoothstep(0.0, 1.0, Radius_Power_NoiseScale.x / (abs(worldPosition.z - sampleD) + 0.00001f));
        occlusion += range * step(sampleD, samplePos.z) * factor;
    }
    occlusion = 1.0 - (occlusion / SSAO_MAX_KERNEL_OFFSETS);
    occlusion = pow(occlusion, Radius_Power_NoiseScale.y);
    
    output.color = float4(occlusion, occlusion, occlusion, 1.0f);
    return output;
}
