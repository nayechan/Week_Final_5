//================================================================================================
// Filename:      Skybox.hlsl
// Description:   6-sided skybox shader
//                - Renders a cube from inside with 6 separate textures
//                - Uses camera rotation only (no translation) to create infinite distance effect
//================================================================================================

// --- Constant Buffers ---

// b1: ViewProjBuffer (VS) - Camera matrices
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

// b3: SkyboxParams (PS) - Skybox parameters
cbuffer SkyboxParams : register(b3)
{
    float4 SkyboxColor;  // RGB = tint, A = intensity
    uint FaceIndex;      // Current face being rendered (0-5)
    float3 Padding;
}

// --- Textures ---
Texture2D g_SkyboxTexture : register(t0);
SamplerState g_LinearSampler : register(s0);

// --- Input/Output Structures ---

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 WorldDir : TEXCOORD1;
};

//================================================================================================
// Vertex Shader
//================================================================================================
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // 카메라 위치 추출 (View 행렬의 역행렬에서)
    float3 cameraPos = InverseViewMatrix[3].xyz;

    // 스카이박스는 항상 카메라 위치에 중심을 두고 렌더링
    // 매우 큰 스케일로 렌더링하여 다른 모든 오브젝트보다 뒤에 있도록
    float3 worldPos = input.Position * 1000.0f + cameraPos;

    // World -> View -> Projection 변환
    float4 viewPos = mul(float4(worldPos, 1.0f), ViewMatrix);
    output.Position = mul(viewPos, ProjectionMatrix);

    // 깊이를 최대값으로 설정하여 항상 가장 뒤에 렌더링
    output.Position.z = output.Position.w * 0.99999f;

    output.TexCoord = input.TexCoord;
    output.WorldDir = normalize(input.Position);

    return output;
}

//================================================================================================
// Pixel Shader
//================================================================================================
float4 mainPS(PS_INPUT input) : SV_Target0
{
    // 텍스처 샘플링
    float4 texColor = g_SkyboxTexture.Sample(g_LinearSampler, input.TexCoord);

    // 밝기/색조 적용
    float3 finalColor = texColor.rgb * SkyboxColor.rgb * SkyboxColor.a;

    return float4(finalColor, 1.0f);
}
