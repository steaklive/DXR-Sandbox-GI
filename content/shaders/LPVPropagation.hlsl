#define SH_COSINE_LOBE_C0 0.886226925f // sqrt(pi)/2
#define SH_COSINE_LOBE_C1 1.02332671f // sqrt(pi/3)

#define SH_c0 0.282094792f // 1 / 2sqrt(pi)
#define SH_c1 0.488602512f // sqrt(3/pi) / 2

#define PI 3.14159265359f

Texture3D<float4> redSH : register(t0);
Texture3D<float4> greenSH : register(t1);
Texture3D<float4> blueSH : register(t2);

static const float3 cellDirections[6] = 
{
    float3(0, 0, 1),
    float3(1, 0, 0),
    float3(0, 0,-1),
    float3(-1,0, 0),
    float3(0, 1, 0),
    float3(0,-1, 0)
};

struct VS_IN
{
    uint vertex : SV_VertexID;
    uint depthIndex : SV_InstanceID;
};

struct VS_OUT
{
    float4 screenPos : SV_Position;
    uint depthIndex : DEPTHINDEX;
};

struct GS_OUT
{
    float4 screenPos : SV_Position;
    uint depthIndex : SV_RenderTargetArrayIndex;
};

struct PS_OUT
{
    float4 redSH : SV_Target0;
    float4 greenSH : SV_Target1;
    float4 blueSH : SV_Target2;
    float4 acc_redSH : SV_Target3;
    float4 acc_greenSH : SV_Target4;
    float4 acc_blueSH : SV_Target5;
};

VS_OUT VSMain(VS_IN input)
{
    VS_OUT output = (VS_OUT) 0;

    if (input.vertex == 0)
        output.screenPos.xy = float2(-1.0, 1.0);
    else if (input.vertex == 1)
        output.screenPos.xy = float2(3.0, 1.0);
    else
        output.screenPos.xy = float2(-1.0, -3.0);
    output.screenPos.zw = float2(0.0, 1.0);
    output.depthIndex = input.depthIndex;

    return output;
}

[maxvertexcount(3)]
void GSMain(triangle VS_OUT input[3], inout TriangleStream<GS_OUT> OutputStream)
{
    for (int i = 0; i < 3; i++)
    {
        GS_OUT output = (GS_OUT)0;
        output.depthIndex = input[i].depthIndex;
        output.screenPos = input[i].screenPos;
		
        OutputStream.Append(output);
    }
}

struct SHContribution
{
    float4 red, green, blue;
};

static const float2 cellsides[4] = { float2(1.0, 0.0), float2(0.0, 1.0), float2(-1.0, 0.0), float2(0.0, -1.0) };
static const float directFaceSubtendedSolidAngle = 0.4006696846f / PI;
static const float sideFaceSubtendedSolidAngle = 0.4234413544f / PI;

float4 dirToSH(float3 direction)
{
    return float4(SH_c0, -SH_c1 * direction.y, SH_c1 * direction.z, -SH_c1 * direction.x);
}

float4 dirCosLobeToSH(float3 direction)
{
    return float4(SH_COSINE_LOBE_C0, -SH_COSINE_LOBE_C1 * direction.y, SH_COSINE_LOBE_C1 * direction.z, -SH_COSINE_LOBE_C1 * direction.x);
}

float3 getEvalSideDirection(int index, int3 orientation)
{
    const float smallComponent = 0.4472135; // 1 / sqrt(5)
    const float bigComponent = 0.894427; // 2 / sqrt(5)
	
    const int2 side = cellsides[index];
    float3 tmp = float3(side.x * smallComponent, side.y * smallComponent, bigComponent);
    return float3(orientation.x * tmp.x, orientation.y * tmp.y, orientation.z * tmp.z);
}

float3 getReprojSideDirection(int index, int3 orientation)
{
    const int2 side = cellsides[index];
    return float3(orientation.x * side.x, orientation.y * side.y, 0);
}

SHContribution GetSHGatheringContribution(int4 cellIndex)
{
    SHContribution result = (SHContribution) 0;
    
    for (int neighbourCell = 0; neighbourCell < 6; neighbourCell++)
    {
        int4 neighbourPos = cellIndex + int4(cellDirections[neighbourCell], 0); //TODO maybe "-"
        
        SHContribution neighbourContribution = (SHContribution) 0;
        neighbourContribution.red = redSH.Load(neighbourPos);
        neighbourContribution.green = greenSH.Load(neighbourPos);
        neighbourContribution.blue = blueSH.Load(neighbourPos);
        
        // add contribution from main Direction
        float4 directionCosLobeSH = dirCosLobeToSH(cellDirections[neighbourCell]);
        float4 directionSH = dirToSH(cellDirections[neighbourCell]);
        result.red += directFaceSubtendedSolidAngle * dot(neighbourContribution.red, directionSH) * directionCosLobeSH;
        result.green += directFaceSubtendedSolidAngle * dot(neighbourContribution.green, directionSH) * directionCosLobeSH;
        result.blue += directFaceSubtendedSolidAngle * dot(neighbourContribution.blue, directionSH) * directionCosLobeSH;
        
        // contributions from side direction
        for (int face = 0; face < 4; face++)
        {
            //float3 faceDir = cellDirections[face] * 0.5f - cellDirections[neighbourCell];
            //float faceDirLength = length(faceDir);

            float3 evaluatedSideDir = getEvalSideDirection(face, cellDirections[face]);
            float3 reproSideDir = getReprojSideDirection(face, cellDirections[face]);
            
            float4 evalSideDirSH = dirToSH(evaluatedSideDir);
            float4 reproSideDirCosLobeSH = dirCosLobeToSH(reproSideDir);
            
            result.red += sideFaceSubtendedSolidAngle * dot(neighbourContribution.red, evalSideDirSH) * reproSideDirCosLobeSH;
            result.green += sideFaceSubtendedSolidAngle * dot(neighbourContribution.green, evalSideDirSH) * reproSideDirCosLobeSH;
            result.blue += sideFaceSubtendedSolidAngle * dot(neighbourContribution.blue, evalSideDirSH) * reproSideDirCosLobeSH;
        }

    }
    
    return result;
}

PS_OUT PSMain(GS_OUT input)
{
    PS_OUT output = (PS_OUT) 0;

    int4 cellIndex = int4(input.screenPos.xy - 0.5f, input.depthIndex, 0);
    SHContribution resultContribution = GetSHGatheringContribution(cellIndex);
    
    output.acc_redSH = output.redSH = resultContribution.red;
    output.acc_greenSH = output.greenSH = resultContribution.green;
    output.acc_blueSH = output.blueSH = resultContribution.blue;
    
    return output;
}