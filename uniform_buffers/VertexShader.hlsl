// an ultra simple hlsl vertex shader
#pragma pack_matrix(row_major)

struct VERTEX
{
    float4 xyzw;
};
cbuffer SHADER_VARS : register(b0, space0)
{
    float4x4 worldMatrix[6];
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

float4 main(VERTEX inputVertex : POSITION,
			uint matrix_index : SV_InstanceID) : SV_POSITION
{
    inputVertex.xyzw = mul(inputVertex.xyzw, worldMatrix[matrix_index]);

	inputVertex.xyzw = mul(inputVertex.xyzw, viewMatrix);
    inputVertex.xyzw = mul(inputVertex.xyzw, projectionMatrix);

	
    return inputVertex.xyzw;
}