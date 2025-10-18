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

cbuffer FireBallCB : register(b7)
{
    float3 gCenterW;
    float gRadius;

    float gIntensity;
    float gFalloff; // RadiusFallOff
    float2 _pad;
    float4 gColor;
}

struct VS_INPUT
{
    float3 position : POSITION; // Input position from vertex buffer
    float3 normal : NORMAL0;
    float4 color : COLOR; // Input color from vertex buffer
    float2 texCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_Position; // Clip Space Position
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL0;
    // float2 uv : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output; // 월드 좌표, 클립 좌표
    
    float4 worldPos = mul(float4(input.position, 1.0f), WorldMatrix);
    output.worldPos = worldPos.xyz;
    
    float4x4 VP = mul(ViewMatrix, ProjectionMatrix);
    output.position = mul(worldPos, VP);
    
    output.normal = input.normal;
    
    // float2 ndc = output.position.xy / output.position.w;
    // output.uv = ndc * 0.5f + 0.5f;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float3 dir = gCenterW - input.worldPos;
    
    // 1) exp dist^2 falloff 계산해 Blend Factor 산출
    // f는 fire color 쪽에 곱해지는 값
    float dist = length(gCenterW - input.worldPos);
    if (dist > gRadius)
    {
        discard; // radius 밖은 그리지 않음
    }
    
    float t = saturate(dist / gRadius);
    float f = exp(-gFalloff * t * t);   

    // f > 0이라 경계 뚝 끊기지 않게 smoothstep로 처리
    const float edgeEps = 0.1; // 상수 혹은 cbuffer로 노출
    float edge = 1.0 - smoothstep(1.0 - edgeEps, 1.0, t);
    f *= edge;
    
    // 3) C_out = (1 - f) * C_dest + f * I * C_src
    // src = fireColor, dest = 기존 색상
    // lerp는 블렌드 스테이트 설정을 통해 진행. blend factor 설정을 위해 f를 src_alpha로 설정
    float3 fireColor = gIntensity * gColor.rgb * dot(input.normal, dir);

    return float4(fireColor, f);
}