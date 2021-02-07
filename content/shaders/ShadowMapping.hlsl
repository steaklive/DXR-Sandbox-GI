struct VSInput
{
    float3 position : POSITION;
};

cbuffer ShadowMappingCB : register(b0)
{
    float4x4 LightViewProj;
};

cbuffer perModelInstanceCB : register(b1)
{
    float4x4 World;
    float4 DiffuseColor;
};

float4 VSMain(VSInput input) : SV_Position
{
    float4 result;   
    result = mul(World, float4(input.position.xyz, 1));
    result = mul(LightViewProj, result);
    result.z *= result.w;

    return result;
}
