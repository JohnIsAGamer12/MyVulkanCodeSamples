// an ultra simple hlsl vertex shader
#pragma pack_matrix(row_major)
// TODO: Part 1c
struct VERTEX
{
    float4 xyzw;
};
// TODO: Part 2b
cbuffer SHADER_VARS : register(b0, space0)
{
    float4x4 worldMatrix[6];
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};
	// TODO: Part 3b
	// TODO: Part 3f
// TODO: Part 3g
float4 main(VERTEX inputVertex : POSITION,
			uint matrix_index : SV_InstanceID) : SV_POSITION
{
	// TODO: Part 2i
    inputVertex.xyzw = mul(inputVertex.xyzw, worldMatrix[matrix_index]);
	// TODO: Part 3b
	inputVertex.xyzw = mul(inputVertex.xyzw, viewMatrix);
    inputVertex.xyzw = mul(inputVertex.xyzw, projectionMatrix);
	// TODO: Part 3g
	
    return inputVertex.xyzw;
}