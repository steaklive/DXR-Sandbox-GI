cbuffer VoxelizationCB : register(b0)
{
    float4x4 WorldVoxelCube;
    float4x4 ViewProjection;
    float WorldVoxelScale;
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
    float3 voxelPos : VOXEL_POSITION;
    float4 worldPos : POSITION_WORLD;
    float3 normal : NORMAL;
    int axis : AXIS;
};

RWTexture3D<float4> outputTexture : register(u0);

GS_IN VSMain(VS_IN input)
{
    GS_IN output = (GS_IN) 0;
    
    output.position = mul(World, float4(input.position.xyz, 1));
    //output.position = mul(ViewProjection, float4(output.position.xyz, 1));
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
    
    float4x4 matProj; //for rasterization only

    float3 p1 = input[1].position.rgb - input[0].position.rgb;
    float3 p2 = input[2].position.rgb - input[0].position.rgb;
    float3 n = abs(normalize(cross(p1, p2)));
       
	float axis = max(n.x, max(n.y, n.z));
    
    for (uint i = 0; i < 3; ++i)
    {
        output[0].voxelPos = input[i].position.xyz / WorldVoxelScale;
        output[1].voxelPos = input[i].position.xyz / WorldVoxelScale;
        output[2].voxelPos = input[i].position.xyz / WorldVoxelScale;
        if (axis == n.z)
        {
            output[0].axis = 1;
            output[1].axis = 1;
            output[2].axis = 1;
            output[i].position = float4(output[i].voxelPos.x, output[i].voxelPos.y, 0, 1);

        }
        else if (axis == n.x)
        {
            output[0].axis = 2;
            output[1].axis = 2;
            output[2].axis = 2;
            output[i].position = float4(output[i].voxelPos.y, output[i].voxelPos.z, 0, 1);
        }
        else
        {
            output[0].axis = 3;
            output[1].axis = 3;
            output[2].axis = 3;
            output[i].position = float4(output[i].voxelPos.x, output[i].voxelPos.z, 0, 1);
        }
    
        //output[i].position.xyz = mul(transpose(matProj), input[i].position);
        output[i].normal = input[i].normal;
        OutputStream.Append(output[i]);
    }
    OutputStream.RestartStrip();
}

void PSMain(PS_IN input)
{
    uint width;
    uint height;
    uint depth;
    
    outputTexture.GetDimensions(width, height, depth);
    
    float3 voxelPos = input.voxelPos.rgb;
    voxelPos.y = -voxelPos.y; 
    
    int3 finalVoxelPos = width * float3(0.5f * voxelPos + float3(0.5f, 0.5f, 0.5f));
    float4 colorRes = float4(DiffuseColor.rgb, 1.0f);
    //TODO add shadow map contribution
    outputTexture[finalVoxelPos] = colorRes;
}