#include "Common.hlsl"

[shader("miss")] 
void Miss(inout Payload payload : SV_RayPayload)
{
    //uncomment for debugging missed rays
    //gOutput[DispatchRaysIndex().xy] = float4(1, 1, 0, 1);
}

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload : SV_RayPayload)
{
    //uncomment for debugging missed rays
    //gOutput[DispatchRaysIndex().xy] = float4(0.3, 0, 0, 1);
}