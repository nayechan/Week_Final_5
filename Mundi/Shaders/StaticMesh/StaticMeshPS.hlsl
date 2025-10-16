Texture2D g_DiffuseTexColor : register(t0);
SamplerState g_Sample : register(s0);

struct FMaterial
{
    float3 DiffuseColor; // Kd
    float OpticalDensity; // Ni
    
    float3 AmbientColor; // Ka
    float Transparency; // Tr Or d
    
    float3 SpecularColor; // Ks
    float SpecularExponent; // Ns
    
    float3 EmissiveColor; // Ke
    uint IlluminationModel; // illum. Default illumination model to Phong for non-Pbr materials
    
    float3 TransmissionFilter; // Tf
    float dummy;
};

cbuffer PixelConstData : register(b0)
{ 
    FMaterial Material;
    bool HasMaterial; // 4 bytes
    bool HasTexture;
}

cbuffer ColorBuffer : register(b3)
{
    float4 LerpColor;
}

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float3 normal : NORMAL0;
    float4 color : COLOR; // Color to pass to the pixel shader
    float2 texCoord : TEXCOORD0;
};

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // Lerp the incoming color with the global LerpColor
    float4 finalColor = input.color;
    finalColor.rgb = lerp(finalColor.rgb, LerpColor.rgb, LerpColor.a) * (1.0f - HasMaterial);
    finalColor.rgb += Material.DiffuseColor * HasMaterial;
    
    if (HasMaterial && HasTexture)
    {
        finalColor.rgb *= g_DiffuseTexColor.Sample(g_Sample, input.texCoord);
    }
    
    return finalColor;
}

