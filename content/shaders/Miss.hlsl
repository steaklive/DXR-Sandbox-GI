#include "Common.hlsl"
RWTexture2D<float4> gOutputReflections: register(u0);
RWTexture2D<float4> gOutputAo: register(u1);

[shader("miss")] 
void Miss(inout Payload payload : SV_RayPayload)
{
    //uncomment for debugging missed rays
    gOutputReflections[DispatchRaysIndex().xy] = float4(0.9f, 0.9f, 0.9f, 1);
}

[shader("miss")]
void AoMiss(inout Payload payload : SV_RayPayload)
{
    //uncomment for debugging missed rays
    //gOutputAo[DispatchRaysIndex().xy] = float4(0.3, 0, 0.9, 1);
}

//[shader("miss")]
//void ShadowMiss(inout ShadowPayload payload : SV_RayPayload)
//{
//    //uncomment for debugging missed rays
//    //gOutput[DispatchRaysIndex().xy] = float4(0.3, 0, 0, 1);
//}