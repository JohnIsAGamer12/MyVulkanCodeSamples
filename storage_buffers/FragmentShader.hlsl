// an ultra simple hlsl fragment shader
#pragma pack_matrix(row_major)

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
// TODO: Part 4a (optional)
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

float4 main(V_OUT vOut) : SV_TARGET
{
	// TODO: Part 3e & Part 3h
    float4 diffuseColor = float4(DrawInfo[vOut.index].material.Kd, 0);
	// TODO: Part 4c
    float3 lightDir = normalize(lightDirection.xyz);
    
    float3 normals = mul(float4(vOut.normW, 1), DrawInfo[vOut.index].worldMatrix);
    float lightRatio = dot(-lightDir, normals);
    
	// TODO: Part 4d (half-vector or reflect method, your choice)
    float3 viewDir = normalize(cameraPos.xyz - vOut.posW);
    
    float3 halfVector = normalize((-lightDir) + viewDir);
    
    float intensity = max(pow(saturate(dot(normalize(normals), halfVector)), DrawInfo[vOut.index].material.Ns + 0.000001f), 0);
    
    float4 totalReflected = float4(DrawInfo[vOut.index].material.Ks, 0) * intensity;
    
    return saturate(lightRatio + lightAmbient) * lightColor * diffuseColor + totalReflected; // TODO: Part 1a (optional)
    
    // return finalColor;
}