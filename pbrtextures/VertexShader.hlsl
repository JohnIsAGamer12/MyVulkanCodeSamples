// an ultra simple hlsl vertex shader
#pragma pack_matrix(row_major)

struct OUT_V
{
    float4 pos : SV_POSITION;
    float3 posW : WORLD;
    float2 uv : TEXTCOORD;
    float3 nrm : NRM;
    float4 tan : TAN;
};

cbuffer SHADER_VARS : register(b0, space0)
{
    float4x4 view;
    float4x4 projection;
    float4 sunDir, sunColor, sunAmbient, camPos;
};

struct INSTANCE_DATA
{
    float4x4 world;
};

StructuredBuffer<INSTANCE_DATA> instanceData : register(b1, space0);

OUT_V main( float3 inputVertex : POSITION,
			float3 inputNormal : NORMAL,
			float2 inputUV : UV,
			float4 inputTangent : TANGENT) 
{
    OUT_V vOut;
    
    vOut.pos = mul(float4(inputVertex, 1.0f), instanceData[0].world);
    
    // get the vertex position in worldspace
    vOut.posW = vOut.pos.xyz;
    
    vOut.pos = mul(vOut.pos, view);
    vOut.pos = mul(vOut.pos, projection);
    vOut.nrm = mul(float4(inputNormal, 0), instanceData[0].world);
    vOut.uv = inputUV;
    vOut.tan = mul(inputTangent, instanceData[0].world);
    
    
    return vOut;
}