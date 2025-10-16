// ShaderW0.hlsl

cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
}

cbuffer HighLightBuffer : register(b2)
{
    int Picked;
    float3 Color;
    int X;
    int Y;
    int Z;
    int GIzmo;
}

cbuffer ColorBuffer : register(b3)
{
    float4 LerpColor;
}

struct VS_INPUT
{
    float3 position : POSITION; // Input position from vertex buffer
    float4 color : COLOR; // Input color from vertex buffer
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float4 color : COLOR; // Color to pass to the pixel shader
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    // 상수버퍼를 통해 넘겨 받은 Offset을 더해서 버텍스를 이동 시켜 픽셀쉐이더로 넘김
    // float3 scaledPosition = input.position.xyz * Scale;
    // output.position = float4(Offset + scaledPosition, 1.0);
    
    float4x4 MVP = mul(mul(WorldMatrix, ViewMatrix), ProjectionMatrix);
    
    output.position = mul(float4(input.position, 1.0f), MVP);    
    
    
    // change color
    float4 c = input.color;

    
    
    // Picked가 1이면 전달된 하이라이트 색으로 완전 덮어쓰기
    if (Picked == 1)
    {
       // const float highlightAmount = 0.35f; // 필요시 조절
     //   float3 highlighted = saturate(lerp(c.rgb, Color, highlightAmount));
        
        // 정수 Picked(0/1)를 마스크로 사용해 분기 없이 적용
     //   float mask = (Picked != 0) ? 1.0f : 0.0f;
     //   c.rgb = lerp(c.rgb, highlighted, mask);
        
        //알파 값 설정
      //  c.a = 0.5;
    }
    
    if(GIzmo == 1)
    {
        if(Y == 1)
        {
            c = float4(1.0, 1.0, 0.0, c.a); // Yellow
        }
        else
        {
            if (X == 1)
                c = float4(1.0, 0.0, 0.0, c.a); // Red
            else if (X == 2)
                c = float4(0.0, 1.0, 0.0, c.a); // Green
            else if (X == 3)
                c = float4(0.0, 0.0, 1.0, c.a); // Blue
        }
        
    }
    
    
    // Pass the color to the pixel shader
    output.color = c;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // Lerp the incoming color with the global LerpColor
    float4 finalColor = input.color;
    finalColor.rgb = lerp(finalColor.rgb, LerpColor.rgb, LerpColor.a);
    return finalColor;
}

