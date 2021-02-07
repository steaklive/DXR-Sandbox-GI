#include "Common.hlsl"

cbuffer CameraParams : register(b0)
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewI;
    float4x4 projectionI;
    float4x4 camToWorld;
    float4 camPosition;
    float2 resolution;
}

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

Texture2D<float4> GBufferNormals : register(t1);
Texture2D<float> GBufferDepth : register(t2);
Texture2D<float4> GBufferAlbedo : register(t3);


[shader("raygeneration")] 
void RayGen() {
    // Initialize the ray payload
    
    uint2 DTid = DispatchRaysIndex().xy;
    float2 xy = DTid.xy + 0.5;

    // Screen position for the ray
    float2 screenPos = xy / resolution * 2.0 - 1.0;
    // Invert Y for DirectX-style coordinates
    screenPos.y = -screenPos.y;

    float2 readGBufferAt = xy;

    // Read depth and normal
    float sceneDepth = GBufferDepth.Load(int3(readGBufferAt, 0));
    float4 normalData = GBufferNormals.Load(int3(readGBufferAt, 0));
    float reflectivity = GBufferAlbedo.Load(int3(readGBufferAt, 0)).w;
    if (reflectivity == 0.0)
        return;
    
    float3 normal = normalData.xyz;
    
    // Unproject into the world position using depth
    //float4 unprojected = mul(camToWorld, float4(screenPos, sceneDepth, 1));
    //float3 world = unprojected.xyz / unprojected.w;
    
    float3 world = ReconstructWorldPosFromDepth(screenPos, sceneDepth, projectionI, viewI);
    float3 primaryRayDirection = normalize(camPosition.rgb - world);
    
    // R
    float3 direction = normalize(-primaryRayDirection - 2 * dot(-primaryRayDirection, normal) * normal);
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

[shader("raygeneration")]
void ShadowRayGen()
{
    uint2 DTid = DispatchRaysIndex().xy;
    float2 xy = DTid.xy + 0.5;

    // Screen position for the ray
    float2 screenPos = xy / resolution * 2.0 - 1.0;
    // Invert Y for DirectX-style coordinates
    screenPos.y = -screenPos.y;

    float2 readGBufferAt = xy;
    float sceneDepth = GBufferDepth.Load(int3(readGBufferAt, 0));
    
    // Initialize the ray payload
    float3 worldOrigin =  ReconstructWorldPosFromDepth(screenPos, sceneDepth, projectionI, viewI);
    float3 lightDir = LightDirection.xyz;

    RayDesc ray;
    ray.Origin = worldOrigin;
    ray.Direction = lightDir;
    ray.TMin = 0.001f;
    ray.TMax = 100000;
    bool hit = true;

    ShadowPayload shadowPayload;
    shadowPayload.isHit = false;

    // do not forget to offset shadow hit group and shadow miss shader
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, ray, shadowPayload);

}