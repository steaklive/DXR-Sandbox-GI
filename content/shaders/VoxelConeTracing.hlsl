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

Texture2D<float4> normalBuffer : register(t0);
Texture2D<float4> worldPosBuffer : register(t1);
Texture3D<float4> voxelTexturePosX : register(t2);
Texture3D<float4> voxelTextureNegX : register(t3);
Texture3D<float4> voxelTexturePosY : register(t4);
Texture3D<float4> voxelTextureNegY : register(t5);
Texture3D<float4> voxelTexturePosZ : register(t6);
Texture3D<float4> voxelTextureNegZ : register(t7);

RWTexture3D<float4> voxelTexture : register(u0);

cbuffer VoxelizationCB : register(b0)
{
    float4x4 WorldVoxelCube;
    float4x4 ViewProjection;
    float WorldVoxelScale;
};

cbuffer VCTMainCB : register(b1)
{
    float Strength;
    float MaxConeTraceDistance;
    float AOFalloff;
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

float4 GetAnisotropicSample(float3 uv, float3 weight, float lod, bool posX, bool posY, bool posZ)
{
    int anisoLevel = max(lod - 1.0f, 0.0f);
    
    uint width;
    uint height;
    uint depth;
    voxelTexturePosX.GetDimensions(width, height, depth);
    
    width >>= anisoLevel;
    height >>= anisoLevel;
    depth >>= anisoLevel;
    
    int3 coord = uv * int3(width, height, depth);
    
    float4 anisoSample = 
    weight.x * ((posX) ? voxelTexturePosX.Load(int4(coord, anisoLevel)) : voxelTextureNegX.Load(int4(coord, anisoLevel))) +
    weight.y * ((posY) ? voxelTexturePosY.Load(int4(coord, anisoLevel)) : voxelTextureNegY.Load(int4(coord, anisoLevel))) +
    weight.z * ((posZ) ? voxelTexturePosZ.Load(int4(coord, anisoLevel)) : voxelTextureNegZ.Load(int4(coord, anisoLevel)));

    if (lod < 1.0f)
    {
        uint width;
        uint height;
        uint depth;
        voxelTexture.GetDimensions(width, height, depth);
        
        coord = uv * int3(width, height, depth);
        float4 baseColor = voxelTexture.Load(int4(coord, 0));
        anisoSample = lerp(baseColor, anisoSample, clamp(lod, 0.0f, 1.0f));
    }

    return anisoSample;
}

float4 GetVoxel(float3 worldPosition, float3 weight, float lod, bool posX, bool posY, bool posZ)
{
    float3 voxelTextureUV = worldPosition / (WorldVoxelScale);
    voxelTextureUV.y = -voxelTextureUV.y;
    voxelTextureUV = voxelTextureUV * 0.5f + 0.5f;
    
    return GetAnisotropicSample(voxelTextureUV, weight, lod, posX, posY, posZ);
}

float4 TraceCone(float3 pos, float3 normal, float3 direction, float aperture, out float occlusion)
{
    uint3 visibleFace;
    visibleFace.x = (direction.x < 0.0) ? 0 : 1;
    visibleFace.y = (direction.y < 0.0) ? 2 : 3;
    visibleFace.z = (direction.z < 0.0) ? 4 : 5;
    
    float lod = 0.0;
    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    occlusion = 0.0;

    float voxelWorldSize = 1.0f;//    WorldVoxelScale / VoxelDimensions;
    float dist = voxelWorldSize;
    float3 startPos = pos + normal * dist;
    
    float3 weight = direction * direction;
    
    const float samplingFactor = 0.5f;

    while (dist < MaxConeTraceDistance && color.a < 0.95f)
    {
        float diameter = 2.0f * aperture * dist;
        float lodLevel = log2(diameter / voxelWorldSize);
        float4 voxelColor = GetVoxel(startPos + dist * direction, weight, lodLevel, direction.x > 0.0, direction.y > 0.0, direction.z > 0.0);
    
        // front-to-back
        color += (1.0 - color.a) * voxelColor;
        if (occlusion < 1.0f)
            occlusion += ((1.0f - occlusion) * voxelColor.a) / (1.0f + AOFalloff * diameter);
        
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
    
    return Strength * result;
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