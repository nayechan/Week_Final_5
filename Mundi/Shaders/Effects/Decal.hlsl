cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 WorldInverseTranspose;  // For normal transformation (not used in this shader)
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
}

cbuffer PSScrollCB : register(b5)
{
    float2 UVScrollSpeed;
    float UVScrollTime;
    float _pad_scrollcb;
}

cbuffer DecalBuffer : register(b6)
{
    row_major float4x4 DecalMatrix;
    float DecalOpacity;
}

Texture2D g_DecalTexColor : register(t0);
SamplerState g_Sample : register(s0);

struct VS_INPUT
{
    float3 position : POSITION; // Input position from vertex buffer
    float3 normal : NORMAL0;
    float4 color : COLOR; // Input color from vertex buffer
    float2 texCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float4 decalPos : POSITION; // decal projection 공간 좌표(w값 유지)
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    float4 worldPos = mul(float4(input.position, 1.0f), WorldMatrix);
    
    float4 decalPos = mul(worldPos, DecalMatrix);
    output.decalPos = decalPos;
    
    float4x4 VP = mul(ViewMatrix, ProjectionMatrix);
    output.position = mul(worldPos, VP);
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float3 ndc = input.decalPos.xyz / input.decalPos.w;
    
    // decal의 forward가 +x임 -> x방향 projection
    if (ndc.x < 0.0f || 1.0f < ndc.x || ndc.y < -1.0f || 1.0f < ndc.y || ndc.z < -1.0f || 1.0f < ndc.z)
    {
        discard;
    }
    
    // ndc to uv
    float2 uv = (ndc.yz + 1.0f) / 2.0f;
    uv.y = 1.0f - uv.y;
    
    uv += UVScrollSpeed * UVScrollTime;
    
    float4 finalColor = g_DecalTexColor.Sample(g_Sample, uv);
    finalColor *= DecalOpacity; // ui로 조절한 decal의 opacity
    
    return finalColor;
}
