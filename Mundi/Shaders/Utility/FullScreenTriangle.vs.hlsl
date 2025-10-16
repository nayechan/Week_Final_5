// 화면을 가득 채우는 삼각형을 만드는 VS
// DeviceContext->Draw(3, 0); 으로 호출해야 됨

// 셰이더의 출력 구조체 정의
struct VS_OUTPUT
{
    float4 Position : SV_POSITION; // 최종 클립 공간 좌표
    float2 TexCoord : TEXCOORD0; // 텍스처를 샘플링할 UV 좌표
};

// 정점 버퍼 입력이 없으므로, main 함수의 파라미터로 SV_VertexID만 받습니다.
VS_OUTPUT mainVS(uint VertexID : SV_VertexID)
{
    VS_OUTPUT Out;
    
    // SV_VertexID를 사용하여 하드코딩된 정점 위치와 텍스처 좌표를 생성합니다.
    // 이 트릭은 화면 전체를 덮는 거대한 삼각형 하나를 만들어냅니다.
    // X, Y는 클립 공간 좌표 (-1 to 1), Z는 깊이, W는 동차 좌표.
    if (VertexID == 0)
    {
        Out.Position = float4(-1.0f, -1.0f, 0.0f, 1.0f);
        Out.TexCoord = float2(0.0f, 1.0f);
    }
    else if (VertexID == 1)
    {
        Out.Position = float4(-1.0f, 3.0f, 0.0f, 1.0f);
        Out.TexCoord = float2(0.0f, -1.0f);
    }
    else // VertexID == 2
    {
        Out.Position = float4(3.0f, -1.0f, 0.0f, 1.0f);
        Out.TexCoord = float2(2.0f, 1.0f);
    }
    
    return Out;
}