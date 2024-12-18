#pragma pack_matrix(row_major)

struct COLOR
{
    float4 rgba;
};

struct VERTEX
{
    float2 pos : POSITION;
    COLOR color : COLOR;
};

struct VERTEX_OUT
{
    float4 pos : SV_POSITION;
    COLOR color : COLOR;
};

[[vk::push_constant]]
cbuffer SHADER_VARS
{
    matrix worldMatrix;
};

VERTEX_OUT main(VERTEX inputVertex)
{	
    VERTEX_OUT output;
    output.pos = mul(float4(inputVertex.pos, 0, 1), worldMatrix);
    output.color = inputVertex.color;
    
    return output;
}