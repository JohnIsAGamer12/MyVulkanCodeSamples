// an ultra simple hlsl vertex shader
#pragma pack_matrix(row_major)

struct OUT_V
{
    float4 pos : SV_POSITION;
    float4 posW : WORLD;
    float2 uv : TEXTCOORD;
    float3 nrm : NRM;
    float4 tan : TAN;
};

cbuffer SHADER_VARS : register(b0, space0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 sunDir;
    float4 camPos;
};

OUT_V main( float3 inputVertex : POSITION,
			float3 inputNormal : NORMAL,
			float2 inputUV : UV,
			float4 inputTangent : TANGENT) 
{
    OUT_V vOut;
    
    vOut.pos = mul(float4(inputVertex, 1.0f), world);
    
    // get the vertex position in worldspace
    vOut.posW = vOut.pos;
    
    vOut.pos = mul(vOut.pos, view);
    vOut.pos = mul(vOut.pos, projection);
    
    // Not used but initalize to avoid improper strides
    vOut.uv = inputUV;
    vOut.nrm = inputNormal;
    vOut.tan = inputTangent;
    
    return vOut;
}