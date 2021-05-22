#include "Common.hlsl"

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

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

Texture2D<float4> GBufferNormals : register(t1);
Texture2D<float4> GBufferWorldPos : register(t2);
Texture2D<float4> GBufferAlbedo : register(t3);


[shader("raygeneration")] 
void RayGen() {
    // Initialize the ray payload
    
    uint2 DTid = DispatchRaysIndex().xy;
    float2 xy = DTid.xy + 0.5;

    // Screen position for the ray
    float2 screenPos = xy / ScreenResolution * 2.0 - 1.0;
    // Invert Y for DirectX-style coordinates
    screenPos.y = -screenPos.y;

    // Read depth and normal
    float3 normalData = normalize(GBufferNormals.Load(int3(xy, 0)).rgb);
    float reflectivity = GBufferAlbedo.Load(int3(xy, 0)).w;
    
    if (reflectivity == 0.0)
        return;
       
    float3 world = GBufferWorldPos.Load(int3(xy, 0)).rgb;
    float3 primaryRayDirection = normalize(world - CamPosition.rgb);
    
    // R
    float3 direction = normalize(reflect(primaryRayDirection, normalData.xyz));
    float3 origin = world - primaryRayDirection * 0.1f; // Lift off the surface a bit

    RayDesc rayDesc =
    {
        origin,
        0.0f,
        direction,
        FLT_MAX
    };
    
    Payload payload;
    payload.skipShading = false;
    payload.rayHitT = FLT_MAX;
    TraceRay(SceneBVH, RAY_FLAG_NONE /*RAY_FLAG_CULL_BACK_FACING_TRIANGLES*/, ~0, 0, 0, 0, rayDesc, payload);
}

//[shader("raygeneration")]
//void ShadowRayGen()
//{
//    uint2 DTid = DispatchRaysIndex().xy;
//    float2 xy = DTid.xy + 0.5;
//
//    // Screen position for the ray
//    float2 screenPos = xy / ScreenResolution * 2.0 - 1.0;
//    // Invert Y for DirectX-style coordinates
//    screenPos.y = -screenPos.y;
//
//    float2 readGBufferAt = xy;
//    float sceneDepth = GBufferDepth.Load(int3(readGBufferAt, 0));
//    
//    // Initialize the ray payload
//    float3 worldOrigin = ReconstructWorldPosFromDepth(screenPos, sceneDepth, InvProjectionMatrix, InvViewMatrix);
//    float3 lightDir = LightDirection.xyz;
//
//    RayDesc ray;
//    ray.Origin = worldOrigin;
//    ray.Direction = lightDir;
//    ray.TMin = 0.001f;
//    ray.TMax = 100000;
//    bool hit = true;
//
//    ShadowPayload shadowPayload;
//    shadowPayload.isHit = false;
//
//    // do not forget to offset shadow hit group and shadow miss shader
//    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, ray, shadowPayload);
//
//}