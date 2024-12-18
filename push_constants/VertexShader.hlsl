// an ultra simple hlsl vertex shader
#pragma pack_matrix(row_major)
// TODO: Part 2b 
[[vk::push_constant]]
cbuffer SHADER_VARS
{
    matrix worldMatrix;
};

float4 main(float2 inputVertex : POSITION) : SV_POSITION
{
	// TODO: Part 2d 
    float4 outVertex = mul(float4(inputVertex, 0, 1), worldMatrix);
    return outVertex;
}