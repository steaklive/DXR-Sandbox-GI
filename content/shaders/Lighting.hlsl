#include "Common.hlsl"

SamplerState BilinearSampler : register(s0);
SamplerComparisonState PcfShadowMapSampler : register(s1);
SamplerState samplerLPV : register(s2);

cbuffer LightingConstantBuffer : register(b0)
{
	float4x4 InvViewProjection;
    float4x4 ShadowViewProjection;
	float4 CameraPos;
	float4 ScreenSize;
    float2 ShadowTexelSize;
    float ShadowIntensity;
    float pad0;
};

cbuffer LightsConstantBuffer : register(b1)
{
    float4 LightDirection;
    float4 LightColor;
    float LightIntensity;
    float3 pad1;
};

cbuffer LPVConstantBuffer : register(b2)
{
    float4x4 worldToLPV;
    float LPVCutoff;
    float LPVPower;
    float LPVAttenuation;
}

cbuffer IlluminationFlagsBuffer : register(b3)
{
    int useDirect;
    int useShadows;
    int useRSM;
    int useLPV;
    int useVCT;
    int showOnlyAO;
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
	float4 diffuse : SV_Target0;
};

PSInput VSMain(VSInput input)
{
	PSInput result;

	result.position = float4(input.position.xyz, 1);
	result.uv = input.uv;
    
	return result;
}

Texture2D<float4> albedoBuffer : register(t0);
Texture2D<float4> normalBuffer : register(t1);
Texture2D<float4> worldPosBuffer : register(t2);
Texture2D<float4> rsmBuffer : register(t3);
Texture2D<float> depthBuffer : register(t4);
Texture2D<float> shadowBuffer : register(t5);

Texture3D<float4> redSH : register(t6);
Texture3D<float4> greenSH : register(t7);
Texture3D<float4> blueSH : register(t8);

Texture2D<float4> vctBuffer : register(t9);

float CalculateShadow(float3 ShadowCoord)
{
    const float Dilation = 2.0;
    float d1 = Dilation * ShadowTexelSize.x * 0.125;
    float d2 = Dilation * ShadowTexelSize.x * 0.875;
    float d3 = Dilation * ShadowTexelSize.x * 0.625;
    float d4 = Dilation * ShadowTexelSize.x * 0.375;
    float result = (
        2.0 *
            shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, ShadowCoord.xy, ShadowCoord.z) +
            shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, ShadowCoord.xy + float2(-d2, d1), ShadowCoord.z) +
            shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, ShadowCoord.xy + float2(-d1, -d2), ShadowCoord.z) +
            shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, ShadowCoord.xy + float2(d2, -d1), ShadowCoord.z) +
            shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, ShadowCoord.xy + float2(d1, d2), ShadowCoord.z) +
            shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, ShadowCoord.xy + float2(-d4, d3), ShadowCoord.z) +
            shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, ShadowCoord.xy + float2(-d3, -d4), ShadowCoord.z) +
            shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, ShadowCoord.xy + float2(d4, -d3), ShadowCoord.z) +
            shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, ShadowCoord.xy + float2(d3, d4), ShadowCoord.z)
        ) / 10.0;
    
    return result * result;
}

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;
    float2 inPos = input.position.xy;    
    
    float depth = depthBuffer[inPos].x;
    float3 normal = normalize(normalBuffer[inPos].rgb);
    float4 albedo = albedoBuffer[inPos];
    float4 worldPos = worldPosBuffer[inPos];
    
    float3 indirectLighting = float3(0.0f, 0.0f, 0.0f);
    float3 directLighting = float3(0.0f, 0.0f, 0.0f);
    
    float ao = 1.0f;
    
    // RSM
    if (useRSM)
    {
        uint gWidth = 0;
        uint gHeight = 0;
        albedoBuffer.GetDimensions(gWidth, gHeight);
        float3 rsm = rsmBuffer.Sample(BilinearSampler, inPos * float2(1.0f / gWidth, 1.0f / gHeight)).rgb;
        
        indirectLighting += rsm;
    }
    
    // LPV
    if (useLPV)
    {
    
        float3 lpv = float3(0.0f, 0.0f, 0.0f);
        float4 SHintensity = DirToSH(normal.rgb);
        float3 lpvCellCoords = (worldPos.rgb * LPV_SCALE + float3(LPV_DIM_HALF, LPV_DIM_HALF, LPV_DIM_HALF)) * LPV_DIM_INVERSE;
        float4 lpvIntensity =
        float4(
	    	max(0.0f, dot(SHintensity, redSH.Sample(samplerLPV, lpvCellCoords))),
	    	max(0.0f, dot(SHintensity, greenSH.Sample(samplerLPV, lpvCellCoords))),
	    	max(0.0f, dot(SHintensity, blueSH.Sample(samplerLPV, lpvCellCoords))),
	    1.0f) / PI;
    
        lpv = LPVAttenuation * min(lpvIntensity.rgb * LPVPower, float3(LPVCutoff, LPVCutoff, LPVCutoff)) * albedo.rgb;
        
        indirectLighting += lpv;
    }
    
    //VCT
    if (useVCT)
    {
        uint gWidth = 0;
        uint gHeight = 0;
        albedoBuffer.GetDimensions(gWidth, gHeight);
        float4 vct = vctBuffer.Sample(BilinearSampler, inPos * float2(1.0f / gWidth, 1.0f / gHeight));
        
        indirectLighting += vct.rgb;
        ao = vct.a;
    }  
    
    float shadow = 1.0f;
    if (useShadows)
    {
        float4 lightSpacePos = mul(ShadowViewProjection, worldPos);
        float4 shadowcoord = lightSpacePos / lightSpacePos.w;
        shadowcoord.rg = shadowcoord.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
        shadow = CalculateShadow(shadowcoord.rgb);
    }
    
    if (useDirect)
    {
        float3 viewDir = normalize(CameraPos.xyz - worldPos.xyz);

        float3 lightDir = LightDirection.xyz;
        float3 lightColor = LightColor.xyz;
        float lightIntensity = LightIntensity;
        float NdotL = saturate(dot(normal.xyz, lightDir));
    
        directLighting = (lightIntensity * NdotL) * lightColor * albedo.rgb;
    }
    
    output.diffuse.rgb = ao * indirectLighting + directLighting * shadow;
    
    if (showOnlyAO)
        output.diffuse.rgb = float3(ao, ao, ao);
    
    return output;
}
