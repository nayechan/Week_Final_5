// Outline_PS.hlsl
// 깊이 기반 외곽선 검출 포스트 프로세스 셰이더
// 특정 오브젝트만 하이라이트할 수 있도록 ObjectID 필터링 지원

Texture2D<float4> g_SceneColorTex : register(t0);
Texture2D<float>  g_DepthTex      : register(t1);
Texture2D<uint>   g_IdBufferTex   : register(t2);

SamplerState g_LinearClampSampler : register(s0);
SamplerState g_PointClampSampler  : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

cbuffer OutlineCB : register(b2)
{
    float4 OutlineColor;

    float  Thickness;
    float  DepthThreshold;
    float  Intensity;
    float  Weight;

    uint   HighlightedObjectID;  // 하이라이트할 오브젝트 ID (0 = 전체 에지)
    float3 Padding;
}

cbuffer ViewportConstants : register(b10)
{
    float4 ViewportRect;
    float4 ScreenSize;
}

// 깊이 값을 선형 깊이로 변환
float LinearizeDepth(float depth)
{
    return Near * Far / (Far - depth * (Far - Near));
}

// 오브젝트 ID 기반 에지 검출 (특정 오브젝트의 외곽선만 검출)
float ObjectIdEdge(float2 uv, float2 texelSize, uint targetId)
{
    // 현재 픽셀의 ObjectID
    int2 pixelCoord = int2(uv * ScreenSize.xy);
    uint centerId = g_IdBufferTex.Load(int3(pixelCoord, 0));

    // 대상 오브젝트가 아니면 0 반환
    if (centerId != targetId)
        return 0.0;

    // 주변 픽셀들의 ObjectID 확인 (외곽선 검출)
    float edge = 0.0;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            if (x == 0 && y == 0) continue;

            int2 offset = int2(x, y) * int(Thickness);
            uint neighborId = g_IdBufferTex.Load(int3(pixelCoord + offset, 0));

            // 이웃 픽셀이 다른 오브젝트면 에지
            if (neighborId != targetId)
            {
                edge = 1.0;
            }
        }
    }

    return edge;
}

// 소벨 필터를 사용한 에지 검출 (전역 에지)
float SobelEdgeDepth(float2 uv, float2 texelSize)
{
    // 3x3 커널의 깊이 샘플링
    float depthTL = LinearizeDepth(g_DepthTex.Sample(g_PointClampSampler, uv + float2(-texelSize.x, -texelSize.y) * Thickness).r);
    float depthT  = LinearizeDepth(g_DepthTex.Sample(g_PointClampSampler, uv + float2(0, -texelSize.y) * Thickness).r);
    float depthTR = LinearizeDepth(g_DepthTex.Sample(g_PointClampSampler, uv + float2(texelSize.x, -texelSize.y) * Thickness).r);

    float depthL  = LinearizeDepth(g_DepthTex.Sample(g_PointClampSampler, uv + float2(-texelSize.x, 0) * Thickness).r);
    float depthC  = LinearizeDepth(g_DepthTex.Sample(g_PointClampSampler, uv).r);
    float depthR  = LinearizeDepth(g_DepthTex.Sample(g_PointClampSampler, uv + float2(texelSize.x, 0) * Thickness).r);

    float depthBL = LinearizeDepth(g_DepthTex.Sample(g_PointClampSampler, uv + float2(-texelSize.x, texelSize.y) * Thickness).r);
    float depthB  = LinearizeDepth(g_DepthTex.Sample(g_PointClampSampler, uv + float2(0, texelSize.y) * Thickness).r);
    float depthBR = LinearizeDepth(g_DepthTex.Sample(g_PointClampSampler, uv + float2(texelSize.x, texelSize.y) * Thickness).r);

    // 소벨 연산자
    float sobelX = depthTL + 2.0 * depthL + depthBL - depthTR - 2.0 * depthR - depthBR;
    float sobelY = depthTL + 2.0 * depthT + depthTR - depthBL - 2.0 * depthB - depthBR;

    float edge = sqrt(sobelX * sobelX + sobelY * sobelY);

    // 임계값 적용
    return saturate(edge * DepthThreshold);
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float4 sceneColor = g_SceneColorTex.Sample(g_LinearClampSampler, input.texCoord);

    // 텍셀 크기 계산
    float2 texelSize = ScreenSize.zw;

    float edge = 0.0;

    if (HighlightedObjectID > 0)
    {
        // 특정 오브젝트만 하이라이트 (ObjectID 기반)
        edge = ObjectIdEdge(input.texCoord, texelSize, HighlightedObjectID);
    }
    else
    {
        // 전체 에지 검출 (깊이 기반)
        edge = SobelEdgeDepth(input.texCoord, texelSize);
    }

    // 강도 적용
    edge = saturate(edge * Intensity);

    // 아웃라인 색상과 원본 블렌딩
    float3 outlineResult = lerp(sceneColor.rgb, OutlineColor.rgb, edge * OutlineColor.a);

    // Weight에 따라 최종 결과 블렌딩
    float3 finalColor = lerp(sceneColor.rgb, outlineResult, Weight);

    return float4(finalColor, sceneColor.a);
}
