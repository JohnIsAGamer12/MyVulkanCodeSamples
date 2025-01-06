// an ultra simple hlsl vertex shader
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    [[vk::builtin("PointSize")]] 
    float PointSize : POINT_SIZE; 
};

VS_OUTPUT main(float2 inputVertex : POSITION)
{
    VS_OUTPUT output;
    output.pos = float4(inputVertex, 0, 1);
    output.PointSize = 1.0f; 
    return output;
}