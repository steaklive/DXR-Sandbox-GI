#include "Common.hlsl"

#define NUM_CONES 6
static const float coneAperture = 0.577f; // 6 cones, 60deg each, tan(30deg) = aperture
static const float3 diffuseConeDirections[] =
{
    float3(0.0f, 1.0f, 0.0f),
    float3(0.0f, 0.5f, 0.866025f),
    float3(0.823639f, 0.5f, 0.267617f),
    float3(0.509037f, 0.5f, -0.7006629f),
    float3(-0.50937f, 0.5f, -0.7006629f),
    float3(-0.823639f, 0.5f, 0.267617f)
};
static const float diffuseConeWeights[] =
{
    PI / 4.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
    3.0f * PI / 20.0f,
};

RWTexture3D<float4> voxelTexture : register(u0);
Texture2D<float4> normalBuffer : register(t0);
Texture2D<float4> worldPosBuffer : register(t1);
//Texture2D<float> depthBuffer : register(t3);

cbuffer VoxelizationCB : register(b0)
{
    float4x4 WorldVoxelCube;
    float4x4 ViewProjection;
    float WorldVoxelScale;
};

struct VS_IN
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct PS_OUT
{
    float4 result : SV_Target0;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN result = (PS_IN)0;

    result.position = float4(input.position.xyz, 1);
    result.uv = input.uv;
    
    return result;
}
float4 GetVoxel(float3 worldPosition, float lod)
{
    uint width;
    uint height;
    uint depth;
    
    voxelTexture.GetDimensions(width, height, depth);
    float3 voxelTextureUV = worldPosition / (WorldVoxelScale);
    voxelTextureUV.y = -voxelTextureUV.y;
    voxelTextureUV = voxelTextureUV * 0.5f + 0.5f;
    
    int3 coord = voxelTextureUV * int3(width, height, depth);
    return voxelTexture.Load(int4(coord, 0));
}

float4 TraceCone(float3 pos, float3 normal, float3 direction, float aperture, out float occlusion)
{
    float lod = 0.0;
    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    occlusion = 0.0;

    float voxelWorldSize = 1.0f;//    WorldVoxelScale / VoxelDimensions;
    float dist = voxelWorldSize;
    float3 startPos = pos + normal * dist;
    
    const float maxDistance = 100.0f;
    const float samplingFactor = 0.5f;
    const float aoFalloff = 700.0f;
    //float falloff = 0.5f * aoFalloff * voxelScale;
    
    while (dist < maxDistance && color.a < 0.95f)
    {
        float diameter = 2.0f * aperture * dist;
        float lodLevel = log2(diameter / voxelWorldSize);
        float4 voxelColor = GetVoxel(startPos + dist * direction, lodLevel);
    
        // front-to-back
        color += (1.0 - color.a) * voxelColor;
        if (occlusion < 1.0f)
            occlusion += ((1.0f - occlusion) * voxelColor.a) / (1.0f + /*falloff*/0.03 * diameter);
        
        dist += diameter * samplingFactor;
    }

    return color;
}


float4 CalculateIndirectDiffuse(float3 worldPos, float3 normal, out float ao)
{
    float4 result;
    float3 coneDirection;
    
    float3 upDir = float3(0.0f, 1.0f, 0.0f);
    if (abs(dot(normal, upDir)) == 1.0f)
        upDir = float3(0.0f, 0.0f, 1.0f);

    float3 right = normalize(upDir - dot(normal, upDir) * normal);
    float3 up = cross(right, normal);
    
    for (int i = 0; i < NUM_CONES; i++)
    {
        coneDirection = normal;
        coneDirection += diffuseConeDirections[i].x * right + diffuseConeDirections[i].z * up;
        coneDirection = normalize(coneDirection);

        result += TraceCone(worldPos, normal, coneDirection, coneAperture, ao) * diffuseConeWeights[i];
    }
    
    return result;
}

PS_OUT PSMain(PS_IN input)
{
    PS_OUT output = (PS_OUT) 0;
    float2 inPos = input.position.xy;
    
    //float depth = depthBuffer[inPos].x;
    float4 normal = normalize(normalBuffer[inPos]);
    float4 worldPos = worldPosBuffer[inPos];
    
    float ao = 0.0f;
    float4 indirectDiffuse = CalculateIndirectDiffuse(worldPos.rgb, normal.rgb, ao);

    output.result = /*float4(ao, ao, ao, 1.0f); //    */indirectDiffuse;
    return output;
}