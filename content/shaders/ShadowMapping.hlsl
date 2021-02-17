struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 worldPos : TEXCOORD0;
};

struct PSOutput
{
    float4 worldPos : SV_Target0;
    float4 normal : SV_Target1;
    float4 flux : SV_Target2;
};

cbuffer ShadowMappingCB : register(b0)
{
    float4x4 LightViewProj;
    float4 LightColor;
    float4 LightDir;
};

cbuffer perModelInstanceCB : register(b1)
{
    float4x4 World;
    float4 DiffuseColor;
};

float4 VSOnlyMain(VSInput input) : SV_Position
{
    float4 result;   
    result = mul(World, float4(input.position.xyz, 1));
    result = mul(LightViewProj, result);
    result.z *= result.w;

    return result;
}

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    output.position = mul(World, float4(input.position.xyz, 1));
    output.worldPos = output.position.xyz;
    output.position = mul(LightViewProj, output.position);
    output.normal = mul(World, float4(input.normal, 0.0f));
    return output;
}

PSOutput PSRSM(VSOutput input)
{
    PSOutput output;
    
    output.worldPos = float4(input.worldPos, 1.0);
    output.normal = normalize(float4(reflect(input.normal, LightDir.rgb), 0.0f));
    output.flux = DiffuseColor * LightColor;
    
    return output;
}