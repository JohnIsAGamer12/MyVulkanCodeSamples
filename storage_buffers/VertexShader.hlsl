// an ultra simple hlsl vertex shader
#pragma pack_matrix(row_major)

struct _OBJ_VERT_
{
    float3 pos : POS;
    float3 uvw : UVW;
    float3 nrm : NRM;
};

cbuffer SHADER_SCENE_DATA
{
    float4 lightDirection, lightAmbient, lightColor, cameraPos;
    float4x4 viewMatrix, projectionMatrix;
};

struct OBJ_ATTRIBUTES
{
    float3  Kd;
    float   d;
    float3  Ks;
    float   Ns;
    float3  Ka;
    float   sharpness;
    float3  Tf;
    float   Ni;
    float3  Ke;
    uint    illum;
};

struct INSTANCE_DATA
{
    float4x4 worldMatrix;
    OBJ_ATTRIBUTES material;
};

StructuredBuffer<INSTANCE_DATA> DrawInfo : register(b1, space0);

struct V_OUT
{
    float4               posH  : SV_POSITION;
    float3               posW  : WORLD;
    float3               uvW   : UV;
    float3               normW : NORMAL;
    nointerpolation uint index : INDEX;
};

V_OUT main(_OBJ_VERT_ inputVertex,
            uint index : SV_InstanceID) 
{
    V_OUT vOut; 
    vOut.posH  = float4(inputVertex.pos, 1);
    
    vOut.posH  = mul(vOut.posH, DrawInfo[index].worldMatrix);
    vOut.index = index;

    vOut.posW  = vOut.posH;
    
    vOut.uvW   = inputVertex.uvw;
    vOut.normW = mul(float4(inputVertex.nrm, 0), DrawInfo[index].worldMatrix);
    
    vOut.posH  = mul(vOut.posH, viewMatrix);
    vOut.posH  = mul(vOut.posH, projectionMatrix);
    return vOut;
}