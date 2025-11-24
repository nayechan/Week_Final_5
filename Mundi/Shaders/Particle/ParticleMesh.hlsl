// Particle Mesh Instancing Shader
// GPU 인스턴싱을 사용하여 메시 파티클 렌더링
// 슬롯 0: 메시 버텍스, 슬롯 1: 인스턴스 데이터

// b1: ViewProjBuffer (VS)
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
};

// 슬롯 0: 메시 버텍스 (FVertexDynamic)
struct VS_MESH_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL0;
    float2 UV : TEXCOORD0;
    float4 Tangent : TANGENT0;
    float4 VertexColor : COLOR;
};

// 슬롯 1: 인스턴스 데이터 (FMeshParticleInstanceVertex) - 표준 시맨틱 사용
struct VS_INSTANCE_INPUT
{
    float4 InstanceColor : COLOR1;      // 파티클 색상
    float4 Transform0 : TEXCOORD1;      // 행렬 첫번째 행
    float4 Transform1 : TEXCOORD2;      // 행렬 두번째 행
    float4 Transform2 : TEXCOORD3;      // 행렬 세번째 행
    float RelativeTime : TEXCOORD4;     // 상대 시간
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL0;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
    float RelativeTime : TEXCOORD1;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
};

Texture2D MeshTexture : register(t0);
SamplerState LinearSampler : register(s0);

PS_INPUT mainVS(VS_MESH_INPUT mesh, VS_INSTANCE_INPUT inst)
{
    PS_INPUT output;

    // 3x4 행렬에서 Transform 추출 (전치되어 저장됨)
    float4x4 InstanceTransform = float4x4(
        float4(inst.Transform0.x, inst.Transform1.x, inst.Transform2.x, 0.0f),
        float4(inst.Transform0.y, inst.Transform1.y, inst.Transform2.y, 0.0f),
        float4(inst.Transform0.z, inst.Transform1.z, inst.Transform2.z, 0.0f),
        float4(inst.Transform0.w, inst.Transform1.w, inst.Transform2.w, 1.0f)
    );

    // 월드 위치 계산
    float4 worldPos = mul(float4(mesh.Position, 1.0f), InstanceTransform);

    // View-Projection 변환
    output.Position = mul(worldPos, mul(ViewMatrix, ProjectionMatrix));

    // 노멀 변환 (회전만 적용)
    float3x3 RotationMatrix = (float3x3)InstanceTransform;
    output.Normal = normalize(mul(mesh.Normal, RotationMatrix));

    // Tangent 사용 (입력 시그니처 유지를 위해 필요)
    float3 worldTangent = normalize(mul(mesh.Tangent.xyz, RotationMatrix));
    output.Normal = normalize(output.Normal - worldTangent * 0.0f);

    // UV 복사
    output.UV = mesh.UV;

    // 인스턴스 색상과 버텍스 색상 곱하기
    output.Color = inst.InstanceColor * mesh.VertexColor;

    // RelativeTime
    output.RelativeTime = inst.RelativeTime;

    return output;
}

PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT output;

    // 텍스처 샘플링
    float4 texColor = MeshTexture.Sample(LinearSampler, input.UV);

    // 간단한 조명 (Hemisphere lighting)
    float3 lightDir = normalize(float3(0.5f, 1.0f, 0.3f));
    float NdotL = saturate(dot(input.Normal, lightDir));
    float lighting = lerp(0.3f, 1.0f, NdotL);  // Ambient + Diffuse

    // 최종 색상
    output.Color = texColor * input.Color;
    output.Color.rgb *= lighting;

    return output;
}
