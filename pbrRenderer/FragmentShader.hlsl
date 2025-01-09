// an ultra simple hlsl fragment shader
#pragma pack_matrix(row_major)

static float4 specular = { 1, 1, 1, 1};
static float4 sunColor = { 0.9f, 0.9f, 1, 1 };
static float4 ambient = { 0.1f, 0.1f, 0.1f, 1};
static float4 emissive = { 0, 0, 0, 1 };
static float Ns = 100;

struct OUT_V
{
    float4 pos  : SV_POSITION;
    float4 posW : WORLD;
    float2 uv   : TEXTCOORD;
    float3 nrm  : NRM;
    float4 tan  : TAN;
};

cbuffer SHADER_VARS : register(b0, space0)
{
    float4x4 view;
    float4x4 projection;
    float4 sunDir;
    float4 camPos;
};

struct INSTANCE_DATA
{
    float4x4 world;
};

StructuredBuffer<INSTANCE_DATA> instanceData : register(b1, space0);

Texture2D textures[] : register(t0, space1);
SamplerState samplers[] : register(s0, space1);

float4 main(OUT_V inputVert) : SV_TARGET
{
    float3 lightDir = normalize(sunDir.xyz);

    float4 diffuseColor = textures[0].Sample(samplers[0], inputVert.uv);
    float4 roughnessMettalic = textures[1].Sample(samplers[1], inputVert.uv);
    
    float occulssion = roughnessMettalic.x;
    float roughness = roughnessMettalic.y;
    
    float3 normal = normalize(mul(float4(inputVert.nrm, 0), instanceData[0].world));
    float4 tangent = normalize(mul(float4(inputVert.tan.xyz * inputVert.tan.w, 0.0f), instanceData[0].world));
    
    float4 binormal = mul(float4(cross(normal, tangent.xyz), 0.0f), instanceData[0].world);
    
    float4 normalMap = textures[2].Sample(samplers[2], inputVert.uv);
    
    // invert green channel for accurate normal maps
    normalMap.g = 1.0f - normalMap.g;
    
    // scale normalMap by 2
    normalMap = normalize((normalMap * 2) - 1.0f);
    
    float3x3 TBN = { tangent.xyz, binormal.xyz, normal.xyz };
    
    float3 newNormal = normalize(mul(normalMap.xyz, TBN));
    
    float lightRatio = dot(-lightDir, newNormal);
    
    float3 viewDir = normalize(camPos.xyz - inputVert.posW.xyz);
    
    float3 halfVector = normalize((-lightDir) + viewDir);
    
    float intensity = max(pow(saturate(dot(normalize(newNormal), halfVector)), Ns + 0.000001f), 0);
    
    float4 totalReflected = ((specular - roughness) * intensity);
    
    float4 emmissiveMap = textures[3].Sample(samplers[3], inputVert.uv);
    
    return saturate(lightRatio + (ambient * occulssion)) * sunColor * diffuseColor + totalReflected + emmissiveMap;
}