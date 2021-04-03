#include "Common.hlsl"

struct RSMTexel
{
    float3 flux;
    float3 normalWS;
    float3 positionWS;
};

Texture2D<float4> worldPosLSBuffer : register(t0); // light space
Texture2D<float4> normalLSBuffer : register(t1); // light space
Texture2D<float4> fluxLSBuffer : register(t2); // light space

cbuffer LPVConstantBuffer : register(b0)
{
    float4x4 worldToLPV;
    float LPVCutoff;
    float LPVPower;
    float LPVAttenuation;
};

struct VS_IN
{
    uint vertexID : SV_VertexID;
};

struct GS_IN
{
    float4 cellPos : SV_POSITION;
    float3 normal : NORMAL;
    float3 flux : FLUX;
};

struct PS_IN
{
    float4 screenPos : SV_POSITION;
    float3 normal : NORMAL;
    float3 flux : FLUX;
    uint layerID : SV_RenderTargetArrayIndex;
};

struct PS_OUT
{
    float4 redSH : SV_Target0;
    float4 greenSH : SV_Target1;
    float4 blueSH : SV_Target2;
};

RSMTexel GetRSMTexel(uint2 coords)
{
    RSMTexel texel = (RSMTexel)0;
    texel.positionWS = worldPosLSBuffer.Load(int3(coords, 0)).xyz; 
    texel.normalWS = normalLSBuffer.Load(int3(coords, 0)).xyz;
    texel.flux = fluxLSBuffer.Load(int3(coords, 0)).xyz;
    return texel;
}

GS_IN VSMain(VS_IN input)
{
    GS_IN output = (GS_IN)0;
    
    uint2 RSMsize;
    worldPosLSBuffer.GetDimensions(RSMsize.x, RSMsize.y);
    uint2 rsmCoords = uint2(input.vertexID % RSMsize.x, input.vertexID / RSMsize.x);

    RSMTexel texel = GetRSMTexel(rsmCoords.xy);

    float3 pos = texel.positionWS;
    output.cellPos = float4(int3(pos * LPV_SCALE + float3(LPV_DIM_HALF, LPV_DIM_HALF, LPV_DIM_HALF) + 0.5f * texel.normalWS), 1.0f);
    output.normal = texel.normalWS;
    output.flux = texel.flux;
    
    return output;
}

[maxvertexcount(1)]
void GSMain(point GS_IN input[1], inout PointStream<PS_IN> OutputStream)
{
    PS_IN output = (PS_IN)0;
    output.layerID = floor(input[0].cellPos.z);
    
    output.screenPos = float4((input[0].cellPos.xy + 0.5f) * LPV_DIM_INVERSE * 2.0f - 1.0f, 0.0f, 1.0f);
    output.screenPos.y = -output.screenPos.y;

    output.normal = input[0].normal;
    output.flux = input[0].flux;

    OutputStream.Append(output);
}

PS_OUT PSMain(PS_IN input)
{
    PS_OUT output = (PS_OUT) 0;
    
    float4 SH_coef = DirCosLobeToSH(input.normal) / PI;
    output.redSH = SH_coef * input.flux.r;
    output.greenSH = SH_coef * input.flux.g;
    output.blueSH = SH_coef * input.flux.b;
    
    return output;
}