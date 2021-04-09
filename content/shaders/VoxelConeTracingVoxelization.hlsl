cbuffer VoxelizationCB : register(b0)
{
    float4x4 ViewProjection;
    float4x4 InverseViewProjection;
    float4 CameraPosition;
    float4 ScreenSize;
    float4 LightColor;
};

cbuffer perModelInstanceCB : register(b1)
{
    float4x4 World;
    float4 DiffuseColor;
};

struct VS_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct GS_IN
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

struct PS_IN
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

RWTexture3D<float4> outputTexture : register(u0);

GS_IN VSMain(VS_IN input)
{
    GS_IN output = (GS_IN) 0;
    
    output.position = mul(World, float4(input.position.xyz, 1));
    output.position = mul(ViewProjection, float4(output.position.xyz, 1));
    output.normal = input.normal; //TODO *inverse ViewProj??

    return output;
}

[maxvertexcount(3)]
void GSMain(triangle GS_IN input[3], inout TriangleStream<PS_IN> OutputStream)
{
    PS_IN output[3];
    output[0] = (PS_IN) 0;
    output[1] = (PS_IN) 0;
    output[2] = (PS_IN) 0;

    
    float3 p1 = input[1].position.rgb - input[0].position.rgb;
    float3 p2 = input[2].position.rgb - input[0].position.rgb;
    float3 p = abs(cross(p1, p2));
    for (uint i = 0; i < 3; ++i)
    {
        float3 pos = input[1].position.rgb;
        if (p.z > p.x && p.z > p.y)
            output[i].position = float4(pos.x, pos.y, 0.0f, 1.0f);
        else if (p.x > p.y && p.x > p.z)
            output[i].position = float4(pos.y, pos.z, 0.0f, 1.0f);
        else
            output[i].position = float4(pos.x, pos.z, 0.0f, 1.0f);
        
        output[i].normal = float3(0.0f, 0.0f, 0.0f);
        OutputStream.Append(output[i]);
    }
}

void PSMain(PS_IN input)
{
    uint width;
    uint height;
    uint depth;
    
    outputTexture.GetDimensions(width, height, depth);
    float4 colorRes = float4(DiffuseColor.rgb, 1.0f);
    float3 voxelPos = float3(0.5f * input.position.rgb + float3(0.5f, 0.5f, 0.5f));
    
    outputTexture[uint3(voxelPos.r * width, voxelPos.g * height, voxelPos.b * depth)] = colorRes;
}