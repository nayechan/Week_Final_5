// C++에서 상수 버퍼를 통해 전달될 데이터
cbuffer CameraInfo : register(b0)
{
    float3 textWorldPos;
    row_major matrix viewMatrix;
    row_major matrix projectionMatrix;
    row_major matrix viewInverse;
    //float3 cameraRight_worldspace;
    //float3 cameraUp_worldspace;
};

struct VS_INPUT
{
    float3 centerPos : WORLDPOSITION;
    float2 size : SIZE;
    float4 uvRect : UVRECT;
    uint vertexId : SV_VertexID; // GPU가 자동으로 부여하는 고유 정점 ID
};

struct PS_INPUT
{
    float4 pos_screenspace : SV_POSITION;
    float2 tex : TEXCOORD0;
};

Texture2D fontAtlas : register(t0);
SamplerState linearSampler : register(s0);


PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // 고정 방향(월드 기준)으로 배치. Z가 Up인 좌표계를 가정하여
    // 로컬 XY를 월드 XZ로 매핑해 수직 면(정면 노멀=-X)을 만든다.
    // 더 이상 카메라를 따라 회전시키지 않음 (billboard 해제)
    //float3 pos_aligned = float3(input.centerPos.x, 0.0f, input.centerPos.y);
    float3 pos_aligned = float3(0.0f, input.centerPos.x, input.centerPos.y);
    float3 finalPos_worldspace = textWorldPos + pos_aligned;

    // 월드 → 뷰 → 프로젝션
    output.pos_screenspace = mul(float4(finalPos_worldspace, 1.0f), mul(viewMatrix, projectionMatrix)) * 0.1;

    // UV는 C++에서 각 정점별로 계산해 전달됨
    output.tex = input.uvRect.xy;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float4 color = fontAtlas.Sample(linearSampler, input.tex);

    clip(color.a - 0.5f); // alpha - 0.5f < 0 이면 해당픽셀 렌더링 중단

    return color;
}
