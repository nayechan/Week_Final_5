//================================================================================================
// Filename:      UberLit.hlsl
// Description:   오브젝트 표면 렌더링을 위한 기본 Uber 셰이더.
//                Extends StaticMeshShader with full lighting support (Gouraud, Lambert, Phong)
//================================================================================================

// --- 조명 모델 선택 ---
// #define LIGHTING_MODEL_GOURAUD 1
// #define LIGHTING_MODEL_LAMBERT 1
// #define LIGHTING_MODEL_PHONG 1

// --- Material 구조체 (OBJ 머티리얼 정보) ---
// 주의: SPECULAR_COLOR 매크로에서 사용하므로 include 전에 정의 필요
struct FMaterial
{
    float3 DiffuseColor;        // Kd - Diffuse 색상
    float OpticalDensity;       // Ni - 광학 밀도 (굴절률)
    float3 AmbientColor;        // Ka - Ambient 색상
    float Transparency;         // Tr or d - 투명도 (0=불투명, 1=투명)
    float3 SpecularColor;       // Ks - Specular 색상
    float SpecularExponent;     // Ns - Specular 지수 (광택도)
    float3 EmissiveColor;       // Ke - 자체발광 색상
    uint IlluminationModel;     // illum - 조명 모델
    float3 TransmissionFilter;  // Tf - 투과 필터 색상
    float Padding;              // 정렬을 위한 패딩
};

// --- 상수 버퍼 (Constant Buffers) ---
// 조명과 StaticMeshShader 기능을 모두 지원하도록 확장

// b0: ModelBuffer (VS) - ModelBufferType과 정확히 일치 (128 bytes)
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;              // 64 bytes
    row_major float4x4 WorldInverseTranspose;    // 64 bytes - 올바른 노멀 변환을 위함
};

// b1: ViewProjBuffer (VS) - ViewProjBufferType과 일치
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
};

// b3: ColorBuffer (PS) - 색상 블렌딩/lerp용
cbuffer ColorBuffer : register(b3)
{
    float4 LerpColor;   // 블렌드할 색상 (알파가 블렌드 양 제어)
    uint UUID;
};

// b4: PixelConstBuffer (VS+PS) - OBJ 파일의 머티리얼 정보
// FPixelConstBufferType과 정확히 일치해야 함!
// 주의: GOURAUD 조명 모델에서는 Vertex Shader에서 사용됨
cbuffer PixelConstBuffer : register(b4)
{
    FMaterial Material;         // 64 bytes
    uint bHasMaterial;          // 4 bytes (HLSL)
    uint bHasTexture;           // 4 bytes (HLSL)
    uint bHasNormalTexture;
};

cbuffer FLightShadowmBufferType : register(b5)
{
    row_major float4x4 LightShadowView;
    row_major float4x4 LightShadowViewUV;
    row_major float4x4 LightShadowProj;
    float ShadowBias;
    float SlopeScaledBias;
    float2 ShadowPadding;
};

// --- Material.SpecularColor 지원 매크로 ---
// LightingCommon.hlsl의 CalculateSpecular에서 Material.SpecularColor를 사용하도록 설정
// 금속 재질의 컬러 Specular 지원
#define SPECULAR_COLOR (bHasMaterial ? Material.SpecularColor : float3(1.0f, 1.0f, 1.0f))

// --- 텍스처 및 샘플러 리소스 ---
Texture2D g_DiffuseTexColor : register(t0);
Texture2D g_NormalTexColor : register(t1);
Texture2D g_DirectionalShadowMap : register(t5);
TextureCubeArray g_ShadowAtlasCube : register(t8);
Texture2D g_ShadowAtlas2D : register(t9);
Texture2D<float2> g_VSMShadowAtlas : register(t10);
TextureCubeArray<float2> g_VSMShadowCube : register(t11);   // TODO: 지금은 전달 안 되고, 안 쓰는 중

SamplerState g_Sample : register(s0);
SamplerState g_Sample2 : register(s1);
SamplerComparisonState g_ShadowSample : register(s2);
SamplerState g_VSMSampler : register(s3);

// --- 공통 조명 시스템 include ---
#include "../Common/LightStructures.hlsl"
#include "../Common/LightingBuffers.hlsl"
#include "../Common/LightingCommon.hlsl"

// --- 셰이더 입출력 구조체 ---
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
    float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : POSITION;     // World position for per-pixel lighting
    float3 Normal : NORMAL0;
    row_major float3x3 TBN : TBN;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
    uint UUID : SV_Target1;
};
//================================================================================================
// 그림자
//================================================================================================
//float GetDirectionalShadowAtt(float3 WorldPos, float3 L, float3 N)
//{
//    uint Width, Height;
//    g_DirectionalShadowMap.GetDimensions(Width, Height); // width, height 읽기
//    float2 TexSizeRCP = float2(1.0f / Width, 1.0f / Height);
    
//    float4 CameraViewPos = mul(float4(WorldPos, 1), ViewMatrix);
//    float4 CameraProjPos = mul(CameraViewPos, ProjectionMatrix);
//    CameraProjPos.xyz /= CameraProjPos.w;
//    float4 LightViewPosUV = mul(float4(CameraProjPos.xyz, 1), LightShadowViewUV);
//    float4 LightProjPosForUV = mul(LightViewPosUV, LightShadowProj);

//    float4 LightViewPos = mul(float4(WorldPos, 1), LightShadowView);
//    float4 LightProjPos = mul(LightViewPos, LightShadowProj);
    
//    float2 LightShadowUV = LightProjPos.xy * 0.5f + 0.5f;
//    LightShadowUV.y = 1 - LightShadowUV.y;
//    float Bias = ShadowBias + (saturate(1 - dot(L, N)) * SlopeScaledBias);
//    float ShadowValue = 0;
//    for (int y = -1; y < 2;y++)
//    {
//        for (int x = -1; x < 2;x++)
//        {
//            float ShadowMapDepth = g_DirectionalShadowMap.Sample(g_Sample2, LightShadowUV + TexSizeRCP * float2(x, y)).r;
//            ShadowValue += LightProjPos.z - Bias > ShadowMapDepth ? 0 : 1;
//        }
//    }
    
//    return ShadowValue / 9;
//}
//float GetCascadedShadowAtt(float3 WorldPos)
//{
//    float4 ViewPos = mul(float4(WorldPos, 1), ViewMatrix);
//    int CurIdx;
//    bool bNeedLerp;
//    float LerpValue;
    
//    if (DirectionalLight.CascadedAreaShadowDebugValue != -1)
//    {

//        CurIdx = DirectionalLight.CascadedAreaShadowDebugValue;
//        float Width, Height;
//        g_ShadowAtlas2D.GetDimensions(Width, Height);
//        float2 TexSizeRCP = float2(1.0f / Width, 1.0f / Height);
//        float2 FilterRadiusUV = 1.5f * TexSizeRCP;
    
//        float3 CurUV = mul(float4(WorldPos, 1), DirectionalLight.Cascades[CurIdx].ShadowViewProjMatrix).xyz;
//        if (saturate(CurUV.x) == CurUV.x && saturate(CurUV.y) == CurUV.y)
//        {
//            float2 CurAtlasUV = CurUV.xy * DirectionalLight.Cascades[CurIdx].AtlasScaleOffset.xy + DirectionalLight.Cascades[CurIdx].AtlasScaleOffset.zw;
//            float CurShadowFactor = SampleShadowPCF(CurUV.z - 0.0025f, CurAtlasUV, DirectionalLight.Cascades[CurIdx].SampleCount, FilterRadiusUV, g_ShadowAtlas2D, g_ShadowSample);
//            return CurShadowFactor;
//        }
//        return 0;
//    }
//    else
//    {
        
    
//        for (int i = 0; i < DirectionalLight.CascadeCount; i++)
//        {
//            float CurFar = DirectionalLight.CascadedSliceDepth[(i + 1) / 4][(i + 1) % 4];
//            if (ViewPos.z < CurFar)
//            {
//                CurIdx = i;
//                break;
//            }
//        }
    
//        float PrevFar = CurIdx == 0 ? 0 : DirectionalLight.CascadedSliceDepth[CurIdx / 4][CurIdx % 4];
//        float ExtensionPrevFar = PrevFar + PrevFar * DirectionalLight.CascadedOverlapValue;
//        if (CurIdx > 0 && ViewPos.z < ExtensionPrevFar)
//        {
//            bNeedLerp = true;
//            LerpValue = (ViewPos.z - PrevFar) / (ExtensionPrevFar - PrevFar);
//        }
   
    
    
//        float Width, Height;
//        g_ShadowAtlas2D.GetDimensions(Width, Height);
//        float2 TexSizeRCP = float2(1.0f / Width, 1.0f / Height);
//        float2 FilterRadiusUV = 1.5f * TexSizeRCP;
    
//        float3 CurUV = mul(float4(WorldPos, 1), DirectionalLight.Cascades[CurIdx].ShadowViewProjMatrix).xyz;
//        float2 CurAtlasUV = CurUV.xy * DirectionalLight.Cascades[CurIdx].AtlasScaleOffset.xy + DirectionalLight.Cascades[CurIdx].AtlasScaleOffset.zw;
//        float CurShadowFactor = SampleShadowPCF(CurUV.z - 0.0025f, CurAtlasUV, DirectionalLight.Cascades[CurIdx].SampleCount, FilterRadiusUV, g_ShadowAtlas2D, g_ShadowSample);
//        if (bNeedLerp)
//        {
//            int PrevIdx = CurIdx - 1;
//            float3 PrevUV = mul(float4(WorldPos, 1), DirectionalLight.Cascades[PrevIdx].ShadowViewProjMatrix).xyz;
//            float2 PrevAtlasUV = PrevUV.xy * DirectionalLight.Cascades[PrevIdx].AtlasScaleOffset.xy + DirectionalLight.Cascades[PrevIdx].AtlasScaleOffset.zw;
//            float PrevShadowFactor = SampleShadowPCF(PrevUV.z - 0.0025f, PrevAtlasUV, DirectionalLight.Cascades[PrevIdx].SampleCount, FilterRadiusUV, g_ShadowAtlas2D, g_ShadowSample);
//            return LerpValue * CurShadowFactor + (1 - LerpValue) * PrevShadowFactor;
//            //return min(CurShadowFactor, PrevShadowFactor);
//        }
//        else
//        {
//            return CurShadowFactor;
//        }
//    }
    
 
//}
    
//================================================================================================
// 버텍스 셰이더 (Vertex Shader)
//================================================================================================
PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Out;
    
    // 위치를 월드 공간으로 먼저 변환
    float4 worldPos = mul(float4(Input.Position, 1.0f), WorldMatrix);
    Out.WorldPos = worldPos.xyz;
    
    // 뷰 공간으로 변환
    float4 viewPos = mul(worldPos, ViewMatrix);

    // 최종적으로 클립 공간으로 변환
    Out.Position = mul(viewPos, ProjectionMatrix);

    // 노멀을 월드 공간으로 변환
    // 비균등 스케일에서 올바른 노멀 변환을 위해 WorldInverseTranspose 사용
    // 노멀 벡터는 transpose(inverse(WorldMatrix))로 변환됨
    float3 worldNormal = normalize(mul(Input.Normal, (float3x3) WorldInverseTranspose));
    Out.Normal = worldNormal;
    float3 Tangent = normalize(mul(Input.Tangent.xyz, (float3x3) WorldMatrix));
    float3 BiTangent = normalize(cross(Tangent, worldNormal) * Input.Tangent.w);
    row_major float3x3 TBN;
    TBN._m00_m01_m02 = Tangent;
    TBN._m10_m11_m12 = BiTangent;
    TBN._m20_m21_m22 = worldNormal;

    Out.TBN = TBN;

    Out.TexCoord = Input.TexCoord;

    // 머티리얼의 SpecularExponent 사용, 머티리얼이 없으면 기본값 사용
    float specPower = bHasMaterial ? Material.SpecularExponent : 32.0f;

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: 정점별 조명 계산 (diffuse + specular)
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);

    // Specular를 위한 뷰 방향 계산
    float3 viewDir = normalize(CameraPosition - Out.WorldPos);

    // 베이스 색상 결정 (일관성을 위해 Lambert/Phong과 동일한 로직)
    float4 baseColor = Input.Color;
    if (bHasMaterial)
    {
        // 머티리얼 diffuse 색상 사용
        // 주의: 텍스처는 픽셀 셰이더에서 곱해짐
        baseColor.rgb = Material.DiffuseColor;
        baseColor.a = 1.0f;  // 알파가 올바르게 설정되도록 보장
    }

    // Ambient light (OBJ/MTL 표준: La × Ka)
    // 하이브리드 접근: Ka가 (0,0,0) 또는 (1,1,1)이면 Kd(baseColor) 사용
    float3 Ka = bHasMaterial ? Material.AmbientColor : baseColor.rgb;
    bool bIsDefaultKa = all(abs(Ka) < 0.01f) || all(abs(Ka - 1.0f) < 0.01f);
    if (bIsDefaultKa)
    {
        Ka = baseColor.rgb;
    }
    finalColor += CalculateAmbientLight(AmbientLight, Ka);

    // Directional light (diffuse + specular) - 그림자 제외
    FDirectionalLightInfo dirLightNoShadow = DirectionalLight;
    dirLightNoShadow.bCastShadows = 0;
    finalColor += CalculateDirectionalLight(dirLightNoShadow, Out.WorldPos, viewPos.xyz, worldNormal, viewDir, baseColor, true, specPower, g_ShadowAtlas2D, g_ShadowSample);

    // Point lights (diffuse + specular) - 그림자 제외
    for (int i = 0; i < PointLightCount; i++)
    {
        FPointLightInfo pointLightNoShadow = g_PointLightList[i];
        pointLightNoShadow.bCastShadows = 0;
        finalColor += CalculatePointLight(pointLightNoShadow, Out.WorldPos, worldNormal, viewDir, baseColor, true, specPower, g_ShadowAtlasCube, g_ShadowSample);
    }

    // Spot lights (diffuse + specular) - 그림자 제외
    for (int j = 0; j < SpotLightCount; j++)
    {
        FSpotLightInfo spotLightNoShadow = g_SpotLightList[j];
        spotLightNoShadow.bCastShadows = 0;
        finalColor += CalculateSpotLight(spotLightNoShadow, Out.WorldPos, worldNormal, viewDir, baseColor, true, specPower, g_ShadowAtlas2D, g_ShadowSample);
    }

    Out.Color = float4(finalColor, baseColor.a);

#elif LIGHTING_MODEL_LAMBERT
    // Lambert Shading: 픽셀별 계산을 위해 픽셀 셰이더로 데이터 전달
    Out.Color = Input.Color;

#elif LIGHTING_MODEL_PHONG
    // Phong Shading: 픽셀별 계산을 위해 픽셀 셰이더로 데이터 전달
    Out.Color = Input.Color;

#else
    // 조명 모델 미정의 - 정점 색상을 그대로 전달
    Out.Color = Input.Color;

#endif

    return Out;
}

//================================================================================================
// 픽셀 셰이더 (Pixel Shader)
//================================================================================================
PS_OUTPUT mainPS(PS_INPUT Input)
{
    PS_OUTPUT Output;
    Output.UUID = UUID;
    
    //CSM 구간 시각화
    float3 Color[2] =
    {
        float3(1, 0, 0),
        float3(0, 1, 0)
    };
    int CascadeCount = DirectionalLight.CascadeCount;

    float4 ViewPos = mul(float4(Input.WorldPos, 1), ViewMatrix);

    float3 CascadeAreaDebugColor;
    float CascadeAreaDebugBlendValue = DirectionalLight.bCascaded ? DirectionalLight.CascadedAreaColorDebugValue : 0;
    for (int i = 0; i < CascadeCount; i++)
    {
        if (ViewPos.z < DirectionalLight.CascadedSliceDepth[(i + 1) / 4][(i + 1) % 4])
        {
            CascadeAreaDebugColor = Color[i % 2];
            break;
        }
    }

    // UV 스크롤링 적용 (활성화된 경우)
    float2 uv = Input.TexCoord;
    //if (bHasMaterial && bHasTexture)
    //{
    //    uv += UVScrollSpeed * UVScrollTime;
    //}
    

#ifdef VIEWMODE_WORLD_NORMAL
    // World Normal 시각화: Normal 벡터를 색상으로 변환
    // Normal 범위: -1~1 → 색상 범위: 0~1
    float3 normalColor = Input.Normal * 0.5 + 0.5;
    
    if(bHasNormalTexture)
    {
        normalColor = g_NormalTexColor.Sample(g_Sample2, uv);
        normalColor = normalColor * 2.0f - 1.0f;
        normalColor = normalize(mul(normalColor, Input.TBN));
    }
    
    Output.Color = float4(normalColor, 1.0);
    return Output;
#endif
    
    // 텍스처 샘플링 (머트리얼 색상은 Gouraud는 VS에서 적용됨)
    float4 texColor = g_DiffuseTexColor.Sample(g_Sample, uv);

    // 머트리얼의 SpecularExponent 사용, 머트리얼이 없으면 기본값 사용
    float specPower = bHasMaterial ? Material.SpecularExponent : 32.0f;

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: 조명이 이미 버텍스 셸이더에서 계산됨 (그림자 제외)
    // Pixel Shader에서 그림자 팩터만 계산해서 곱함
    float4 finalPixel = Input.Color;
    
    // 그림자 팩터 계산 (모든 라이트 통합)
    float shadowFactor = 1.0f;
    
    // Directional Light 그림자
    if (DirectionalLight.bCastShadows)
    {
        shadowFactor *= CalculateSpotLightShadowFactor(Input.WorldPos, DirectionalLight.Cascades[0], g_ShadowAtlas2D, g_ShadowSample);
    }
    
    // Point Light 그림자
    for (int i = 0; i < PointLightCount; i++)
    {
        if (g_PointLightList[i].bCastShadows)
        {
            shadowFactor *= CalculatePointLightShadowFactor(
                Input.WorldPos, g_PointLightList[i].Position, g_PointLightList[i].AttenuationRadius,
                g_PointLightList[i].LightIndex, 16, g_ShadowAtlasCube, g_ShadowSample);
        }
    }
    
    // Spot Light 그림자
    for (int j = 0; j < SpotLightCount; j++)
    {
        if (g_SpotLightList[j].bCastShadows)
        {
            shadowFactor *= CalculateSpotLightShadowFactor(
                Input.WorldPos, g_SpotLightList[j].ShadowData, g_ShadowAtlas2D, g_ShadowSample);
        }
    }
    
    // 그림자 적용 (VS에서 계산된 조명)
    finalPixel.rgb *= shadowFactor;

    // 텍스처 또는 머트리얼 색상 적용
    if (bHasTexture)
    {
        // 텍스처 모듈레이션: 조명 결과에 텍스처 곱하기
        finalPixel.rgb *= texColor.rgb;
    }
    // 주의: Material.DiffuseColor는 이미 Vertex Shader에서 적용됨
    // 여기서 추가 색상 적용 불필요

    // 자체발광 추가 (조명의 영향을 받지 않음)
    if (bHasMaterial)
    {
        finalPixel.rgb += Material.EmissiveColor;
    }

    // 비머티리얼 오브젝트의 머티리얼/색상 블렌딩 적용
    if (!bHasMaterial)
    {
        finalPixel.rgb = lerp(finalPixel.rgb, LerpColor.rgb, LerpColor.a);
    }

    // 머티리얼 투명도 적용 (0=불투명, 1=투명)
    if (bHasMaterial)
    {
        finalPixel.a *= (1.0f - Material.Transparency);
    }

    Output.Color = finalPixel;
    Output.Color.rgb = (1 - CascadeAreaDebugBlendValue) * Output.Color.rgb + CascadeAreaDebugBlendValue * CascadeAreaDebugColor;
    return Output;

#elif LIGHTING_MODEL_LAMBERT
    // Lambert Shading: 픽셀별 디퓨즈 조명 계산 (스페큘러 없음)
    float3 normal = normalize(Input.Normal);
    float4 baseColor = Input.Color;

    // 텍스처가 있으면 텍스처로 시작
    if (bHasTexture)
    {
        baseColor.rgb = texColor.rgb;
    }
    else if (bHasMaterial)
    {
        // 텍스처 없음, 머티리얼 diffuse 색상 사용
        baseColor.rgb = Material.DiffuseColor;
    }
    else
    {
        // 텍스처와 머티리얼 모두 없음, LerpColor와 블렌드
        baseColor.rgb = lerp(baseColor.rgb, LerpColor.rgb, LerpColor.a);
    }

    float3 litColor = float3(0.0f, 0.0f, 0.0f);

    // Ambient light (OBJ/MTL 표준: La × Ka)
    // 하이브리드 접근: Ka가 (0,0,0) 또는 (1,1,1)이면 Kd(baseColor) 사용
    float3 Ka = bHasMaterial ? Material.AmbientColor : baseColor.rgb;
    bool bIsDefaultKa = all(abs(Ka) < 0.01f) || all(abs(Ka - 1.0f) < 0.01f);
    if (bIsDefaultKa)
    {
        Ka = baseColor.rgb;
    }
    litColor += CalculateAmbientLight(AmbientLight, Ka);

    // Directional light (diffuse만)
    litColor += CalculateDirectionalLight(DirectionalLight, normal, float3(0, 0, 0), baseColor, false, 0.0f);

    // 타일 기반 라이트 컬링 적용 (활성화된 경우)
    if (bUseTileCulling)
    {
        // 현재 픽셀이 속한 타일 계산
        uint tileIndex = CalculateTileIndex(Input.Position, ViewportStartX, ViewportStartY);
        uint tileDataOffset = GetTileDataOffset(tileIndex);

        // 타일에 영향을 주는 라이트 개수
        uint lightCount = g_TileLightIndices[tileDataOffset];

        // 타일 내 라이트만 순회
        [loop]
        for (uint i = 0; i < lightCount; i++)
        {
            uint packedIndex = g_TileLightIndices[tileDataOffset + 1 + i];
            uint lightType = (packedIndex >> 16) & 0xFFFF;  // 상위 16비트: 타입
            uint lightIdx = packedIndex & 0xFFFF;           // 하위 16비트: 인덱스

            if (lightType == 0)  // Point Light
            {
                litColor += CalculatePointLight(g_PointLightList[lightIdx], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f, g_ShadowAtlasCube, g_ShadowSample);
            }
            else if (lightType == 1)  // Spot Light
            {
                litColor +=  CalculateSpotLight(g_SpotLightList[lightIdx], Input.WorldPos, normal,
                    float3(0, 0, 0), baseColor, false,
                    0.0f, g_ShadowAtlas2D, g_ShadowSample, g_VSMShadowAtlas, g_VSMSampler);
            }
        }
    }
    else
    {
        // 타일 컴링 비활성화: 모든 라이트 순회 (기존 방식)
        for (int i = 0; i < PointLightCount; i++)
        {
            litColor += CalculatePointLight(g_PointLightList[i], Input.WorldPos, normal, float3(0, 0, 0), baseColor, false, 0.0f, g_ShadowAtlasCube, g_ShadowSample);
        }

        [loop]
        for (int j = 0; j < SpotLightCount; j++)
        {
            litColor +=  CalculateSpotLight(g_SpotLightList[j], Input.WorldPos, normal,
                    float3(0, 0, 0), baseColor, false,
                    0.0f, g_ShadowAtlas2D, g_ShadowSample, g_VSMShadowAtlas, g_VSMSampler);
        }
    }

    // 조명 계산 후 자체발광 추가
    if (bHasMaterial)
    {
        litColor += Material.EmissiveColor;
    }

    // 원본 알파 보존 (조명은 투명도에 영향 없음)
    float finalAlpha = baseColor.a;

    // 머티리얼 투명도 적용 (0=불투명, 1=투명)
    if (bHasMaterial)
    {
        finalAlpha *= (1.0f - Material.Transparency);
    }

    Output.Color = float4(litColor, finalAlpha);
    Output.Color.rgb = (1 - CascadeAreaDebugBlendValue) * Output.Color.rgb + CascadeAreaDebugBlendValue * CascadeAreaDebugColor;
    return Output;

#elif LIGHTING_MODEL_PHONG
    // Phong Shading: 픽셀별 디퓨즈와 스페큘러 조명 계산 (Blinn-Phong)
    float3 normal = normalize(Input.Normal);
    if(bHasNormalTexture)
    {
        normal = g_NormalTexColor.Sample(g_Sample2, uv);
        normal = normal * 2.0f - 1.0f;
        normal = normalize(mul(normal, Input.TBN));
    }
    float3 viewDir = normalize(CameraPosition - Input.WorldPos);
    float4 baseColor = Input.Color;

    // 텍스처가 있으면 텍스처로 시작
    if (bHasTexture)
    {
        baseColor.rgb = texColor.rgb;
    }
    else if (bHasMaterial)
    {
        // 텍스처 없음, 머티리얼 diffuse 색상 사용
        baseColor.rgb = Material.DiffuseColor;
    }
    else
    {
        // 텍스처와 머티리얼 모두 없음, LerpColor와 블렌드
        baseColor.rgb = lerp(baseColor.rgb, LerpColor.rgb, LerpColor.a);
    }

    float3 litColor = float3(0.0f, 0.0f, 0.0f);

    // Ambient light (OBJ/MTL 표준: La × Ka)
    // 하이브리드 접근: Ka가 (0,0,0) 또는 (1,1,1)이면 Kd(baseColor) 사용
    float3 Ka = bHasMaterial ? Material.AmbientColor : baseColor.rgb;
    bool bIsDefaultKa = all(abs(Ka) < 0.01f) || all(abs(Ka - 1.0f) < 0.01f);
    if (bIsDefaultKa)
    {
        Ka = baseColor.rgb;
    }
    litColor += CalculateAmbientLight(AmbientLight, Ka);

    // Directional light (diffuse + specular)
    float3 DirectionalLightColor = CalculateDirectionalLight(DirectionalLight, Input.WorldPos, ViewPos.xyz, normal, viewDir, baseColor, true, specPower, g_ShadowAtlas2D, g_ShadowSample);
   
    
    litColor += DirectionalLightColor;

    // 타일 기반 라이트 컬링 적용 (활성화된 경우)
    if (bUseTileCulling)
    {
        // 현재 픽셀이 속한 타일 계산
        uint tileIndex = CalculateTileIndex(Input.Position, ViewportStartX, ViewportStartY);
        uint tileDataOffset = GetTileDataOffset(tileIndex);

        // 타일에 영향을 주는 라이트 개수
        uint lightCount = g_TileLightIndices[tileDataOffset];

        // 타일 내 라이트만 순회
        [loop]
        for (uint i = 0; i < lightCount; i++)
        {
            uint packedIndex = g_TileLightIndices[tileDataOffset + 1 + i];
            uint lightType = (packedIndex >> 16) & 0xFFFF;  // 상위 16비트: 타입
            uint lightIdx = packedIndex & 0xFFFF;           // 하위 16비트: 인덱스

            if (lightType == 0)  // Point Light
            {
                litColor += CalculatePointLight(g_PointLightList[lightIdx], Input.WorldPos, normal, viewDir, baseColor, true, specPower, g_ShadowAtlasCube, g_ShadowSample);
            }
            else if (lightType == 1)  // Spot Light
            {
                litColor +=  CalculateSpotLight(g_SpotLightList[lightIdx], Input.WorldPos, normal,
                    float3(0, 0, 0), baseColor, false,
                    0.0f, g_ShadowAtlas2D, g_ShadowSample, g_VSMShadowAtlas, g_VSMSampler);
            }
        }
    }
    else
    {
        // 타일 컴링 비활성화: 모든 라이트 순회 (기존 방식)
        for (int i = 0; i < PointLightCount; i++)
        {
            litColor += CalculatePointLight(g_PointLightList[i], Input.WorldPos, normal, viewDir, baseColor, true, specPower, g_ShadowAtlasCube, g_ShadowSample);
        }
        
        [loop]
        for (int j = 0; j < SpotLightCount; j++)
        {
            litColor +=  CalculateSpotLight(g_SpotLightList[j], Input.WorldPos, normal,
                    float3(0, 0, 0), baseColor, false,
                    0.0f, g_ShadowAtlas2D, g_ShadowSample, g_VSMShadowAtlas, g_VSMSampler);
        }
    }

    // 조명 계산 후 자체발광 추가
    if (bHasMaterial)
    {
        litColor += Material.EmissiveColor;
    }

    // 원본 알파 보존 (조명은 투명도에 영향 없음)
    float finalAlpha = baseColor.a;

    // 머티리얼 투명도 적용 (0=불투명, 1=투명)
    if (bHasMaterial)
    {
        finalAlpha *= (1.0f - Material.Transparency);
    }
    
    Output.Color = float4(litColor, finalAlpha);
    Output.Color.rgb = (1 - CascadeAreaDebugBlendValue) * Output.Color.rgb + CascadeAreaDebugBlendValue * CascadeAreaDebugColor;
    return Output;

#else
    // 조명 모델 미정의 - StaticMeshShader 동작 사용
    float4 finalPixel = Input.Color;

    // 머티리얼/텍스처 블렌딩 적용
    if (bHasMaterial)
    {
        finalPixel.rgb = Material.DiffuseColor;
        if (bHasTexture)
        {
            finalPixel.rgb = texColor.rgb;
        }
        // 자체발광 추가
        finalPixel.rgb += Material.EmissiveColor;
    }
    else
    {
        // LerpColor와 블렌드
        finalPixel.rgb = lerp(finalPixel.rgb, LerpColor.rgb, LerpColor.a);
        finalPixel.rgb *= texColor.rgb;
    }

    // 머티리얼 투명도 적용 (0=불투명, 1=투명)
    if (bHasMaterial)
    {
        finalPixel.a *= (1.0f - Material.Transparency);
    }
    
    Output.Color = finalPixel;
    return Output;
#endif
}