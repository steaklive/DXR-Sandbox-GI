Texture3D<float4> voxelTexture : register(t0);

//unfortunately, there is no "RWTexture3DArray"
RWTexture3D<float4> voxelTextureResultPosX : register(u0);
RWTexture3D<float4> voxelTextureResultNegX : register(u1);
RWTexture3D<float4> voxelTextureResultPosY : register(u2);
RWTexture3D<float4> voxelTextureResultNegY : register(u3);
RWTexture3D<float4> voxelTextureResultPosZ : register(u4);
RWTexture3D<float4> voxelTextureResultNegZ : register(u5);

cbuffer MipmapCB : register(b0)
{
    int MipDimension;
    int MipLevel;
}

static const int3 anisoOffsets[8] =
{
    int3(1, 1, 1),
	int3(1, 1, 0),
	int3(1, 0, 1),
	int3(1, 0, 0),
	int3(0, 1, 1),
	int3(0, 1, 0),
	int3(0, 0, 1),
	int3(0, 0, 0)
};

[numthreads(8, 8, 8)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= MipDimension || DTid.y >= MipDimension ||	DTid.z >= MipDimension)
        return;
    
    float4 values[8];
    
    int3 sourcePos = DTid * 2;
    [unroll]
    for (int i = 0; i < 8; i++)
        values[i] = voxelTexture.Load(int4(sourcePos + anisoOffsets[i], 0));
    
    voxelTextureResultPosX[DTid] =
        (values[4] + values[0] * (1 - values[4].a) + values[5] + values[1] * (1 - values[5].a) +
        values[6] + values[2] * (1 - values[6].a) +	values[7] + values[3] * (1 - values[7].a)) * 0.25f;
    
    voxelTextureResultNegX[DTid] =
        (values[0] + values[4] * (1 - values[0].a) + values[1] + values[5] * (1 - values[1].a) +
		values[2] + values[6] * (1 - values[2].a) + values[3] + values[7] * (1 - values[3].a)) * 0.25f;
    
    voxelTextureResultPosY[DTid] =
	    (values[2] + values[0] * (1 - values[2].a) + values[3] + values[1] * (1 - values[3].a) +
    	values[7] + values[5] * (1 - values[7].a) +	values[6] + values[4] * (1 - values[6].a)) * 0.25f;
    
    voxelTextureResultNegY[DTid] =
	    (values[0] + values[2] * (1 - values[0].a) + values[1] + values[3] * (1 - values[1].a) +
    	values[5] + values[7] * (1 - values[5].a) + values[4] + values[6] * (1 - values[4].a)) * 0.25f;
    
    voxelTextureResultPosZ[DTid] =
	    (values[1] + values[0] * (1 - values[1].a) + values[3] + values[2] * (1 - values[3].a) +
    	values[5] + values[4] * (1 - values[5].a) + values[7] + values[6] * (1 - values[7].a)) * 0.25f;
    
    voxelTextureResultNegZ[DTid] =
	    (values[0] + values[1] * (1 - values[0].a) + values[2] + values[3] * (1 - values[2].a) +
    	values[4] + values[5] * (1 - values[4].a) + values[6] + values[7] * (1 - values[6].a)) * 0.25f;
    
}