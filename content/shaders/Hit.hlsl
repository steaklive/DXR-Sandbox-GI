#include "Common.hlsl"

//RaytracingAccelerationStructure SceneBVH : register(t0);

Texture2D<float4> GBufferNormals : register(t1);
Texture2D<float4> GBufferAlbedo : register(t3);

// Mesh info (naive approach, proper way is to combine all meshes from the scene with textures, etc)
ByteAddressBuffer MeshIndices : register(t4, space0);
StructuredBuffer<Vertex> MeshVertices : register(t5, space0);

cbuffer MeshInfo : register(b2)
{
    float4 MeshColor;
}

uint3 Load3x32BitIndices(ByteAddressBuffer buffer, uint offsetBytes)
{
    return buffer.Load3(offsetBytes);
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
    
    //float3 worldPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

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
    
    float3 outputColor = (lightIntensity * NdotL) * lightColor * albedoColor;

    outputColor = gOutput[DispatchRaysIndex().xy].rgb + reflectivity * outputColor;
    
    gOutput[DispatchRaysIndex().xy] = float4(outputColor, 1.0);

}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    // Get the base index of the triangle's first 32 bit index.
    uint indexSizeInBytes = 4;
    uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;

    // Load up three 32 bit indices for the triangle.
    const uint3 indices = Load3x32BitIndices(MeshIndices, baseIndex);
    
    float3 triangleNormal = MeshVertices[indices[0]].normal;
    float4 normal = float4(triangleNormal, 1.0f);
    float3 lightDir = LightDirection.xyz;
    float NdotL = saturate(dot(normal.xyz, lightDir));
    
    float shadowFactor = 1.0f - ShadowIntensity;
    
    if (!payload.isHit && !NdotL)
        gOutput[DispatchRaysIndex().xy] *= float4(shadowFactor, shadowFactor, shadowFactor, 1.0f);
}
