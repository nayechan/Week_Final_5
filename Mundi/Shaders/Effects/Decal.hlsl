//================================================================================================
// Filename:      Decal.hlsl
// Description:   Decal projection shader with lighting support
//                Supports GOURAUD, LAMBERT, PHONG lighting models
//================================================================================================

// --- 조명 모델 선택 ---
// ViewMode에서 동적으로 설정됨 (SceneRenderer::RenderDecalPass)
// 가능한 매크로:
// - LIGHTING_MODEL_GOURAUD
// - LIGHTING_MODEL_LAMBERT
// - LIGHTING_MODEL_PHONG
// - (매크로 없음 = Unlit)

// --- 공통 조명 시스템 include ---
#include "../Common/LightStructures.hlsl"
#include "../Common/LightingBuffers.hlsl"
#include "../Common/LightingCommon.hlsl"

// --- Decal 전용 상수 버퍼 ---
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 WorldInverseTranspose;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
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
    float FadeProgress;     // Fade progress (0-1)
    uint FadeStyle;         // 0:Standard, 1:WipeLtoR, 2:Dissolve, 3:Iris
    float _pad_decal;
}

// --- 텍스처 리소스 ---
Texture2D g_DecalTexColor : register(t0);
TextureCubeArray g_ShadowAtlasCube : register(t8);
Texture2D g_ShadowAtlas2D : register(t9);
Texture2D<float2> g_VSMShadowAtlas : register(t10);
TextureCubeArray<float2> g_VSMShadowCube : register(t11);

SamplerState g_Sample : register(s0);
SamplerComparisonState g_ShadowSample : register(s2);
SamplerState g_VSMSampler : register(s3);

// --- 유틸리티 함수 ---
// UV 좌표로부터 무작위처럼 보이는 값을 생성하는 해시 함수
float hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

// --- 입출력 구조체 ---
struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL0;
    float2 texCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 decalPos : POSITION0;    // Decal projection 좌표

#if defined(LIGHTING_MODEL_GOURAUD) || defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_PHONG)
    float3 worldPos : POSITION1;    // 조명 계산용
    float3 normal : NORMAL0;        // 조명 계산용

    #ifdef LIGHTING_MODEL_GOURAUD
        float4 litColor : COLOR0;   // Pre-calculated lighting (Gouraud)
    #endif
#endif
};

//================================================================================================
// 버텍스 셰이더
//================================================================================================
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // World position
    float4 worldPos = mul(float4(input.position, 1.0f), WorldMatrix);
    float4 viewPos = mul(worldPos, ViewMatrix);

    // 샘플 방식: worldPos를 그대로 PS로 전달 (PS에서 DecalMatrix 적용)
    output.decalPos = worldPos;

    // Screen position
    float4x4 VP = mul(ViewMatrix, ProjectionMatrix);
    output.position = mul(worldPos, VP);

#if defined(LIGHTING_MODEL_GOURAUD) || defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_PHONG)
    // 조명 계산을 위한 데이터
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul(input.normal, (float3x3) WorldInverseTranspose));

    #ifdef LIGHTING_MODEL_GOURAUD
        // Gouraud: Vertex shader에서 조명 계산
        // Note: 여기서는 texture를 샘플링할 수 없으므로 white를 base color로 사용
        // Pixel shader에서 texture를 곱함
        float4 baseColor = float4(1, 1, 1, 1);
        float3 viewDir = normalize(CameraPosition - output.worldPos);
        float specPower = 32.0f;

        float3 litColor = CalculateAllLights(
            output.worldPos,
            viewPos.xyz,
            output.normal,
            viewDir,
            baseColor,
            specPower,
            output.position,
            g_ShadowSample,
            g_ShadowAtlas2D,
            g_ShadowAtlasCube,
            g_VSMSampler,
            g_VSMShadowAtlas,
            g_VSMShadowCube
        );

        output.litColor = float4(litColor, 1.0f);
    #endif
#endif

    return output;
}

//================================================================================================
// 픽셀 셰이더
//================================================================================================
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // 샘플 코드 방식: 월드 좌표를 데칼 로컬 공간으로 변환

    // 1. input.decalPos는 월드 좌표 (VS에서 그대로 전달됨)
    // DecalMatrix(DecalInverseWorld)를 곱하여 로컬 공간으로 변환
    float3 localPos = mul(input.decalPos, DecalMatrix).xyz;

    // 2. 데칼 볼륨 범위 체크: [-0.5, 0.5] 범위 밖이면 discard
    if (abs(localPos.x) > 0.5f ||
        abs(localPos.y) > 0.5f ||
        abs(localPos.z) > 0.5f)
    {
        discard;
    }

    // 3. 픽셀의 3D 위치를 X축에서 바라보고 2D 평면(YZ)에 투사하여 UV 생성
    float2 uv = localPos.yz * float2(-1.0f, -1.0f) + 0.5f;
    uv += UVScrollSpeed * UVScrollTime;

    float4 decalTexture = g_DecalTexColor.Sample(g_Sample, uv);

    // 4. 알파값 기본 체크
    if (decalTexture.a < 0.01f)
    {
        discard;
    }

    // 5. FadeStyle에 따라 다른 효과 적용
    float softness = 0.1f; // 경계선의 부드러움 정도
    float scaledProgress = FadeProgress * (1.0f + softness);

    switch (FadeStyle)
    {
        case 0: // Standard Alpha Fade
            decalTexture.a *= FadeProgress;
            break;
        case 1: // Wipe Left to Right
        {
            float threshold = uv.x;
            float t = saturate((scaledProgress - threshold) / softness);
            decalTexture.a *= t * t * (3.0f - 2.0f * t); // Smoothstep
            break;
        }
        case 2: // Procedural Random Dissolve
        {
            float threshold = hash(uv);
            float t = saturate((scaledProgress - threshold) / softness);
            decalTexture.a *= t * t * (3.0f - 2.0f * t); // Smoothstep
            break;
        }
        case 3: // Iris (중앙에서 확장/축소)
        {
            float threshold = distance(uv, float2(0.5f, 0.5f)) * 1.414f;
            float t = saturate((scaledProgress - threshold) / softness);
            decalTexture.a *= t * t * (3.0f - 2.0f * t); // Smoothstep
            break;
        }
    }

    // 6. Edge hardening: 매우 낮은 알파값을 부드럽게 제거하여 프린지 방지
    float edge = saturate((decalTexture.a - 0.05f) / 0.05f);
    decalTexture.a *= edge;
    clip(decalTexture.a - 0.001f);

    // 7. 조명 계산 및 최종 알파 적용 (매크로에 따라)
#ifdef LIGHTING_MODEL_GOURAUD
    // Gouraud: VS에서 계산한 조명 결과 사용
    float4 finalColor = input.litColor;
    finalColor.rgb *= decalTexture.rgb;  // Texture modulation
    finalColor.a = decalTexture.a * DecalOpacity;

    // Premultiply alpha to avoid white wash with standard alpha blending
    finalColor.rgb *= finalColor.a;
    return finalColor;

#elif defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_PHONG)
    // Lambert/Phong: PS에서 조명 계산
    float3 normal = normalize(input.normal);
    float4 baseColor = decalTexture;
    float specPower = 32.0f;
    float4 viewPos = mul(float4(input.worldPos, 1.0f), ViewMatrix);

    #ifdef LIGHTING_MODEL_PHONG
        float3 viewDir = normalize(CameraPosition - input.worldPos);
    #else
        float3 viewDir = float3(0, 0, 0);  // Lambert는 사용 안 함
    #endif

    float3 litColor = CalculateAllLights(
        input.worldPos,
        viewPos.xyz,
        normal,
        viewDir,
        baseColor,
        specPower,
        input.position,
        g_ShadowSample,
        g_ShadowAtlas2D,
        g_ShadowAtlasCube,
        g_VSMSampler,
        g_VSMShadowAtlas,
        g_VSMShadowCube
    );

    float4 finalColor = float4(litColor, decalTexture.a * DecalOpacity);

    // Premultiply alpha to avoid white wash with standard alpha blending
    finalColor.rgb *= finalColor.a;
    return finalColor;

#else
    // No lighting model - 기존 방식 (단순 텍스처)
    float4 finalColor = decalTexture;
    finalColor.a *= DecalOpacity;

    // Premultiply alpha to avoid white wash with standard alpha blending
    finalColor.rgb *= finalColor.a;
    return finalColor;
#endif
}
