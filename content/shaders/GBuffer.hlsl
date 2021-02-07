struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;
};

struct PSInput
{
    float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
	float4 position : SV_POSITION;
};

cbuffer GPrepassCB : register(b0)
{
	float4x4 ViewProjection;
	float4x4 InverseViewProjection;
	float4  CameraPosition;
    float4 ScreenSize;
};

cbuffer perModelInstanceCB : register(b1)
{
	float4x4	World;
    float4		DiffuseColor;
};

Texture2D<float4> Textures[] : register(t0);
SamplerState SamplerLinear : register(s0);

PSInput VSMain(VSInput input)
{
    PSInput result;

	result.normal = mul((float3x3)World, input.normal.xyz);
	result.tangent = mul((float3x3)World, input.tangent.xyz);
	result.position = mul(World, float4(input.position.xyz, 1));
	result.worldPos = result.position.xyz;
	result.position = mul(ViewProjection, result.position);
	result.uv = input.uv;

    return result;
}

struct PSOutput
{
	float4 colour : SV_Target0;
	float4 normal : SV_Target1;
	float4 worldpos : SV_Target2;
};

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;

	//TODO albedo texture support
    output.colour = DiffuseColor;
    output.normal = float4(input.normal, 1.0f);
    output.worldpos = float4(input.worldPos, 1.0f);
    return output;
}
