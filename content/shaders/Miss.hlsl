#include "Common.hlsl"
RWTexture2D<float4> gOutput : register(u0);

[shader("miss")] 
void Miss(inout Payload payload : SV_RayPayload)
{
    //uncomment for debugging missed rays
    gOutput[DispatchRaysIndex().xy] = float4(0.9f, 0.9f, 0.9f, 1);
}

//[shader("miss")]
//void ShadowMiss(inout ShadowPayload payload : SV_RayPayload)
//{
//    //uncomment for debugging missed rays
//    //gOutput[DispatchRaysIndex().xy] = float4(0.3, 0, 0, 1);
//}