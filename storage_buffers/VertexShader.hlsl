// an ultra simple hlsl vertex shader
#pragma pack_matrix(row_major)

// TODO: Part 1f
struct _OBJ_VERT_
{
    float3 pos : POS; // 0
    float3 uvw : UVW; // 1
    float3 nrm : NRM; // 2
};
// TODO: Part 2c // TODO: Part 4d
cbuffer SHADER_SCENE_DATA
{
    float4 lightDirection, lightAmbient, lightColor, cameraPos;
    float4x4 viewMatrix, projectionMatrix;
};
// TODO: Part 3b
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
// TODO: Part 3c
struct INSTANCE_DATA
{
    float4x4 worldMatrix;
    OBJ_ATTRIBUTES material;
};

StructuredBuffer<INSTANCE_DATA> DrawInfo : register(b1, space0);
// TODO: Part 4a
// TODO: Part 4b
// TODO: Part 3g
// TODO: Part 3h
struct V_OUT
{
    float4 posH : SV_POSITION;
    float3 posW : WORLD;
    float3 uvW : UV;
    float3 normW : NORMAL;
    nointerpolation uint index : INDEX;
};

V_OUT main(_OBJ_VERT_ inputVertex,
            uint index : SV_InstanceID) 
{
	// TODO: Part 1h
    // inputVertex.pos.y += 0.75f;
    // inputVertex.pos.z += 0.75f;
	// TODO: Part 2f & Part 3g
    V_OUT vOut; 
    vOut.posH = float4(inputVertex.pos, 1);
    
    vOut.posH = mul(vOut.posH, DrawInfo[index].worldMatrix);
    vOut.index = index;
	// TODO: Part 3h
    vOut.posW = vOut.posH;
	// TODO: Part 4b
    vOut.uvW = inputVertex.uvw;
    vOut.normW = inputVertex.nrm;
    
    
    vOut.posH = mul(vOut.posH, viewMatrix);
    vOut.posH = mul(vOut.posH, projectionMatrix);
    return vOut;
}