// an ultra simple hlsl fragment shader
#pragma pack_matrix(row_major)

cbuffer SHADER_SCENE_DATA
{
    float4   lightDirection, lightAmbient, lightColor, cameraPos;
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
    float4x4       worldMatrix;
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

float4 main(V_OUT vOut) : SV_TARGET
{
    float4 diffuseColor = float4(DrawInfo[vOut.index].material.Kd, 0);

    float3 lightDir = normalize(lightDirection.xyz);
    
    vOut.normW = normalize(vOut.normW);
    float lightRatio = dot(-lightDir, vOut.normW);
    
    float3 viewDir = normalize(cameraPos.xyz - vOut.posW);
    
    float3 halfVector = normalize((-lightDir) + viewDir);
    
    float intensity = max(pow(saturate(dot(normalize(vOut.normW), halfVector)), DrawInfo[vOut.index].material.Ns + 0.000001f), 0);
    
    float4 totalReflected = float4(DrawInfo[vOut.index].material.Ks, 0) * intensity;
    
    return saturate(lightRatio + lightAmbient) * lightColor * diffuseColor + totalReflected; 
}