// an ultra simple hlsl fragment shader
struct COLOR
{
    float4 rgba;
};

struct VERTEX_IN
{
    float4 pos : SV_POSITION;
    COLOR color : COLOR;
};

float4 main(VERTEX_IN inVert) : SV_TARGET
{
    return inVert.color.rgba;
}