#include "Common.hlsl"

struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 texcoord;
};

//RaytracingAccelerationStructure SceneBVH : register(t0);
RWTexture2D<float4> gOutput : register(u0);

Texture2D<float4> GBufferNormals : register(t1);
Texture2D<float4> GBufferWorldPos : register(t2);
Texture2D<float4> GBufferAlbedo : register(t3);

// Mesh info (naive approach, proper way is to combine all meshes from the scene with textures, etc)
ByteAddressBuffer MeshIndices : register(t4, space0);
StructuredBuffer<Vertex> MeshVertices : register(t5, space0);

Texture2D<float4> ShadowTexture : register(t6);

cbuffer DXRConstantBuffer : register(b0)
{
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 InvViewMatrix;
    float4x4 InvProjectionMatrix;
    float4x4 ShadowViewProjection;
    float4 CamPosition;
    float2 ScreenResolution;
}

cbuffer LightsConstantBuffer : register(b1)
{
    float4 LightDirection;
    float4 LightColor;
    float LightIntensity;
    float3 pad1;
};

cbuffer MeshInfo : register(b2)
{
    float4 MeshColor;
}

uint3 Load3x32BitIndices(ByteAddressBuffer buffer, uint offsetBytes)
{
    return buffer.Load3(offsetBytes);
}

float CalculateShadow(float3 ShadowCoord)
{
    uint width = 0;
    uint height = 0;
    ShadowTexture.GetDimensions(width, height);
    float v = ShadowTexture.Load(int3(ShadowCoord.xy * float2(width, height), 0)).r;
    v = (v <= ShadowCoord.z) ? 0.0f : ShadowCoord.z; // cant use SampleCmpLevelZero
    v *= 2.0f;
    return v * v;
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    // Get the base index of the triangle's first 32 bit index.
    uint indexSizeInBytes = 4; 
    uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;

    // Load up three 32 bit indices for the triangle.
    const uint3 indices = Load3x32BitIndices(MeshIndices, baseIndex);
    // Retrieve corresponding vertex normals for the triangle vertices.
    float3 triangleNormal = MeshVertices[indices[0]].normal;
    float4 normal = float4(triangleNormal, 1.0f);
    
    float4 worldPosition = float4(WorldRayOrigin() + WorldRayDirection() * RayTCurrent(), 1.0f);

    float reflectivity = GBufferAlbedo[DispatchRaysIndex().xy].w;
    if (reflectivity == 0.0) //skip the surface if its not reflective
        return;

    //const float3 viewDir = normalize(-WorldRayDirection());
    
    float3 lightDir = LightDirection.xyz;
    float3 lightColor = LightColor.xyz;
    float lightIntensity = LightIntensity;
    float NdotL = saturate(dot(normal.xyz, lightDir));

    // proper way is to pass some material info of the mesh with local textures, etc.
    // however, sampling those textures will require extra work as we have to calculate UVs here as well...
    float3 albedoColor = MeshColor.rgb;
    
    float4 lightSpacePos = mul(ShadowViewProjection, worldPosition);
    float4 shadowcoord = lightSpacePos / lightSpacePos.w;
    shadowcoord.rg = shadowcoord.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    float shadow = CalculateShadow(shadowcoord.rgb);
    
    float3 outputColor = (lightIntensity * NdotL) * lightColor * albedoColor * shadow;

    outputColor = gOutput[DispatchRaysIndex().xy] + reflectivity * outputColor;
    
    gOutput[DispatchRaysIndex().xy] = float4(outputColor, 1.0);

}