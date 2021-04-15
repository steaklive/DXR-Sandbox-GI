struct VS_IN
{
    uint vertexID : SV_VertexID;
};

struct GS_IN
{
    float4 position : SV_POSITION;
};

struct PS_IN
{
    float4 pos : SV_Position;
    float4 color : POSITION;
};

struct PS_OUT
{
    float4 result : SV_Target0;
};

RWTexture3D<float4> voxelTexture : register(u0);

GS_IN VSMain(VS_IN input)
{
    GS_IN output = (GS_IN) 0;
    
    uint width;
    uint height;
    uint depth;
    
    voxelTexture.GetDimensions(width, height, depth);
    
    float3 centerVoxelPos;
    centerVoxelPos.x = input.vertexID % width;
    centerVoxelPos.z = (input.vertexID / width) % width;
    centerVoxelPos.y = input.vertexID / (width * width);
    
    output.position = float4(0.5f * centerVoxelPos + float3(0.5f, 0.5f, 0.5f), 1.0f); //TODO add sampled alpha
    return output;
}

[maxvertexcount(36)]
void GSMain(point GS_IN input[1], inout TriangleStream<PS_IN> OutputStream)
{
    PS_IN output[36];
    for (int i = 0; i < 36; i++)
        output[i] = (PS_IN) 0;
    
    float4 v1 = /*ProjectionMatrix * ModelViewMatrix * */(input[0].position + float4(-0.5, 0.5, 0.5, 0));
    float4 v2 = /*ProjectionMatrix * ModelViewMatrix * */(input[0].position + float4(0.5, 0.5, 0.5, 0));
    float4 v3 = /*ProjectionMatrix * ModelViewMatrix * */(input[0].position + float4(-0.5, -0.5, 0.5, 0));
    float4 v4 = /*ProjectionMatrix * ModelViewMatrix * */(input[0].position + float4(0.5, -0.5, 0.5, 0));
    float4 v5 = /*ProjectionMatrix * ModelViewMatrix * */(input[0].position + float4(-0.5, 0.5, -0.5, 0));
    float4 v6 = /*ProjectionMatrix * ModelViewMatrix * */(input[0].position + float4(0.5, 0.5, -0.5, 0));
    float4 v7 = /*ProjectionMatrix * ModelViewMatrix * */(input[0].position + float4(-0.5, -0.5, -0.5, 0));
    float4 v8 = /*ProjectionMatrix * ModelViewMatrix * */(input[0].position + float4(0.5, -0.5, -0.5, 0));
    
    // +Z
    output[0].color = v1;
    OutputStream.Append(output[0]);
    output[1].color = v3;
    OutputStream.Append(output[1]);
    output[2].color = v4;
    OutputStream.Append(output[2]);
    OutputStream.RestartStrip();
    output[3].color = v1;
    OutputStream.Append(output[4]);
    output[4].color = v4;
    OutputStream.Append(output[5]);
    output[5].color = v2;
    OutputStream.Append(output[6]);
    OutputStream.RestartStrip();

    // -Z
    output[6].color = v6;
    OutputStream.Append(output[6]);
    output[7].color = v8;
    OutputStream.Append(output[7]);
    output[8].color = v7;
    OutputStream.Append(output[8]);
    OutputStream.RestartStrip();
    output[9].color = v6;
    OutputStream.Append(output[9]);
    output[10].color = v7;
    OutputStream.Append(output[10]);
    output[11].color = v5;
    OutputStream.Append(output[11]);
    OutputStream.RestartStrip();

    // +X
    output[12].color = v2;
    OutputStream.Append(output[12]);
    output[13].color = v4;
    OutputStream.Append(output[13]);
    output[14].color = v8;
    OutputStream.Append(output[14]);
    OutputStream.RestartStrip();
    output[15].color = v2;
    OutputStream.Append(output[15]);
    output[16].color = v8;
    OutputStream.Append(output[16]);
    output[17].color = v6;
    OutputStream.Append(output[17]);
    OutputStream.RestartStrip();

    // -X
    output[18].color = v5;
    OutputStream.Append(output[18]);
    output[19].color = v7;
    OutputStream.Append(output[19]);
    output[20].color = v3;
    OutputStream.Append(output[20]);
    OutputStream.RestartStrip();
    output[21].color = v5;
    OutputStream.Append(output[21]);
    output[22].color = v3;
    OutputStream.Append(output[22]);
    output[23].color = v1;
    OutputStream.Append(output[23]);
    OutputStream.RestartStrip();

    // +Y
    output[24].color = v5;
    OutputStream.Append(output[24]);
    output[25].color = v1;
    OutputStream.Append(output[25]);
    output[26].color = v2;
    OutputStream.Append(output[26]);
    OutputStream.RestartStrip();
    output[27].color = v5;
    OutputStream.Append(output[27]);
    output[28].color = v2;
    OutputStream.Append(output[28]);
    output[29].color = v6;
    OutputStream.Append(output[29]);
    OutputStream.RestartStrip();

    // -Y
    output[30].color = v3;
    OutputStream.Append(output[30]);
    output[31].color = v7;
    OutputStream.Append(output[31]);
    output[32].color = v8;
    OutputStream.Append(output[32]);
    OutputStream.RestartStrip();
    output[33].color = v3;
    OutputStream.Append(output[33]);
    output[34].color = v8;
    OutputStream.Append(output[34]);
    output[35].color = v4;
    OutputStream.Append(output[35]);
    OutputStream.RestartStrip();
}

PS_OUT PSMain(PS_IN input)
{
    PS_OUT output = (PS_OUT) 0;
    
    if (input.color.a < 0.5f)
        discard;
    
    output.result = input.color;
    return output;
}