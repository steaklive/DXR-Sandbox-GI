#define PI 3.14159265359f
#define RSM_SAMPLES_COUNT 512

Texture2D<float4> worldPosLSBuffer : register(t0);   // light space
Texture2D<float4> normalLSBuffer : register(t1);     // light space
Texture2D<float4> fluxLSBuffer : register(t2);       // light space

Texture2D<float4> worldPosWSBuffer : register(t3);   // world space
Texture2D<float4> normalWSBuffer : register(t4);     // world space

SamplerState RSMSampler : register(s0);

cbuffer RSMConstantBuffer : register(b0)
{
    float4x4 ShadowViewProjection;
    float RSMIntensity;
    float RSMRMax;
    float2 UpsampleRatio;
};
cbuffer RSMConstantBuffer2 : register(b1)
{
    float4 xi[RSM_SAMPLES_COUNT];
}

struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float2 uv : TEXCOORD;
    float4 position : SV_POSITION;
};

struct PSOutput
{
    float4 rsm : SV_Target0;
};

float3 CalculateRSM(float3 pos, float3 normal)
{
    float4 texSpacePos = mul(ShadowViewProjection, float4(pos, 1.0f));
    texSpacePos.rgb /= texSpacePos.w;
    texSpacePos.rg = texSpacePos.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    
    float3 indirectIllumination = float3(0.0, 0.0, 0.0);
    
    uint width = 0;
    uint height = 0;
    uint nol = 0;
    normalLSBuffer.GetDimensions(0, width, height, nol);
    
    for (int i = 0; i < RSM_SAMPLES_COUNT; i++)
    {
        float2 coord = texSpacePos.rg + RSMRMax * float2(xi[i].x * sin(2.0f * PI * xi[i].y), xi[i].x * cos(2.0f * PI * xi[i].y));
        
        float2 texcoord = coord * float2(width, height);
        float3 vplPosWS = worldPosLSBuffer.Load(uint3(uint2(texcoord), 0)).rgb;
        float3 vplNormalWS = normalLSBuffer.Sample(RSMSampler, coord).rgb;
        float3 flux = fluxLSBuffer.Load(uint3(uint2(texcoord), 0)).rgb;
                
        float3 vplPosDir = (pos - vplPosWS);

        float3 res = flux * ((max(0.0, dot(vplNormalWS, vplPosDir)) * max(0.0, dot(normal, -vplPosDir))) / (dot(vplPosDir, vplPosDir) * dot(vplPosDir, vplPosDir)));
        res *= xi[i].x * xi[i].x;
        
        indirectIllumination += res;
    }
    
    return saturate(indirectIllumination * RSMIntensity);
}

PSInput VSMain(VSInput input)
{
    PSInput result;

    result.position = float4(input.position.xyz, 1);
    result.uv = input.uv;
    
    return result;
}

PSOutput PSMain(PSInput input)
{
    PSOutput output = (PSOutput) 0;
    
    float4 normalWS = normalWSBuffer[input.position.xy * UpsampleRatio];
    float4 worldPosWS = worldPosWSBuffer[input.position.xy * UpsampleRatio];
    output.rsm = float4(CalculateRSM(worldPosWS.rgb, normalWS.rgb), 1.0f);
    
    return output;
}
