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

VERTEX_OUT main(VERTEX inputVertex)
{
    VERTEX_OUT output;
    output.pos = float4(inputVertex.pos, 0 , 1);
    output.color = inputVertex.color;
    return output;
}