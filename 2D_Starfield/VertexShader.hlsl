// an ultra simple hlsl vertex shader
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    [[vk::builtin("PointSize")]] // uncomment for point size
    float PointSize : POINT_SIZE; // uncomment for point size
};

VS_OUTPUT main(float2 inputVertex : POSITION)
{
    VS_OUTPUT output;
    output.pos = float4(inputVertex, 0, 1);
    output.PointSize = 1.0f; // uncomment for point size
    return output;
}