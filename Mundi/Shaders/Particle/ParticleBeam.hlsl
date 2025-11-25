// ------------------------------------------------------------
// Global constants
// ------------------------------------------------------------

// b1: ViewProjBuffer (VS)
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
};

// ------------------------------------------------------------
// Vertex Shader
// ------------------------------------------------------------

struct VS_INPUT
{
    float3 Position : POSITION; // Beam quad world position
    float2 UV : TEXCOORD0; // Can be kept for procedural effects
    float4 Color : COLOR0; // Vertex color
    float Width : TEXCOORD1; // Optional (not used here)
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = float4(input.Position, 1.0f);

    // World → View → Projection
    float4 viewPos = mul(worldPos, ViewMatrix);
    output.Position = mul(viewPos, ProjectionMatrix);

    output.UV = input.UV;
    output.Color = input.Color;

    return output;
}

// ------------------------------------------------------------
// Pixel Shader
// ------------------------------------------------------------

float4 mainPS(PS_INPUT input) : SV_Target
{
    // 1. Settings
    float4 beamColor = float4(1.0, 0.0, 1.0, 1.0); // Magenta color

    // 2. Use the horizontal UV coordinate (UV.x)
    float u_coord = input.UV.x;

    // 3. Create a gradient that is 1.0 at the center (U=0.5) and 0.0 at the edges (U=0, U=1).
    // This value itself will be the intensity of the beam.
    float intensity = 1.0 - (abs(u_coord - 0.5) * 2.0);

    // Optional: To make the beam's core sharper and edges fall off faster,
    // you can apply a power function. A higher exponent means a sharper core.
    float exponent = 4.0f;
    intensity = pow(intensity, exponent);

    // 4. The final color is the base beam color multiplied by the intensity.
    float4 final_color;
    final_color.rgb = beamColor.rgb * intensity;
    final_color.a = beamColor.a * intensity; // Apply to alpha for smooth transparent edges

    return final_color;
}