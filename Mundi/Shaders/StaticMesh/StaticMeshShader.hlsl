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

cbuffer HighLightBuffer : register(b2)
{
    int Picked;
    float3 Color;
    int X;
    int Y;
    int Z;
    int GIzmo;
}

struct FAmbientLightInfo
{
    float4 Color;
    float Intensity;
};

struct FDirectionalLightInfo
{
    float4 Color;
    float3 Direction;
    float Intensity;
};

struct FPointLightInfo
{
    float4 Color;
    float3 Position;
    float Radius;
    float3 Attenuation;
    float Intensity;
};

struct FSpotLightInfo
{
    float4 Color;
    float3 Position;
    float InnerConeAngle;
    float3 Direction;
    float OuterConeAngle;
    float Intensity;
    float Radius;
};

cbuffer LightBuffer : register(b8)
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;
    FPointLightInfo PointLights[16];
    FSpotLightInfo SpotLights[16];
    uint DirectionalLightCount;
    uint PointLightCount;
    uint SpotLightCount;
}

struct VS_INPUT
{
    float3 position : POSITION; // Input position from vertex buffer
    float3 normal : NORMAL0;
    float4 color : COLOR; // Input color from vertex buffer
    float2 texCoord : TEXCOORD0;
};


Texture2D g_DiffuseTexColor : register(t0);
SamplerState g_Sample : register(s0);

struct FMaterial
{
    float3 DiffuseColor; // Kd
    float OpticalDensity; // Ni
    
    float3 AmbientColor; // Ka
    float Transparency; // Tr Or d
    
    float3 SpecularColor; // Ks
    float SpecularExponent; // Ns
    
    float3 EmissiveColor; // Ke
    uint IlluminationModel; // illum. Default illumination model to Phong for non-Pbr materials
    
    float3 TransmissionFilter; // Tf
    float dummy;
};

cbuffer ColorBuffer : register(b3)
{
    float4 LerpColor;
    uint UUID;
}

cbuffer PixelConstData : register(b4)
{
    FMaterial Material;
    bool HasMaterial; // 4 bytes
    bool HasTexture;
}
cbuffer PSScrollCB : register(b5)
{
    float2 UVScrollSpeed;
    float  UVScrollTime;
    float  _pad_scrollcb;
}

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float3 normal : NORMAL0;
    float4 color : COLOR; // Color to pass to the pixel shader
    float2 texCoord : TEXCOORD0;
    uint UUID : UUID;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
    uint UUID : SV_Target1;
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
    
    if (GIzmo == 1)
    {
        if (Y == 1)
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
    
    output.normal = input.normal;
    output.texCoord = input.texCoord;
    
    return output;
}

PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT Output;
    // Lerp the incoming color with the global LerpColor
    float4 finalColor = input.color;
    finalColor.rgb = lerp(finalColor.rgb, LerpColor.rgb, LerpColor.a) * (1.0f - HasMaterial);
    finalColor.rgb += Material.DiffuseColor * HasMaterial;
    
    if (HasMaterial && HasTexture)
    {
        float2 uv = input.texCoord + UVScrollSpeed * UVScrollTime;
        finalColor.rgb = g_DiffuseTexColor.Sample(g_Sample, uv);
    }
    
    Output.Color = finalColor;
    Output.UUID = UUID;
    return Output;
}

