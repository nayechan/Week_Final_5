Texture2D g_COCTex : register(t0);

SamplerState g_LinearClampSample : register(s0);
SamplerState g_PointClampSample : register(s1);

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

cbuffer DepthOfFieldCB : register(b2)
{
    float FocusDistance;
    float FocusRange;
    float COCSize;
    float padding;
}
cbuffer DOFGaussianCB : register(b3)
{
    float Weight;
    uint Range;
    uint bHorizontal;
    uint bNear;
}

cbuffer ViewportConstants : register(b10)
{
    // x: Viewport TopLeftX, y: Viewport TopLeftY
    // z: Viewport Width,   w: Viewport Height
    float4 ViewportRect;
    
    // x: Screen Width      (전체 렌더 타겟 너비)
    // y: Screen Height     (전체 렌더 타겟 높이)
    // z: 1.0f / Screen W,  w: 1.0f / Screen H
    float4 ScreenSize;
}

float GetGaussian(float dis)
{
    return exp(-dis * dis / (2 * Weight * Weight));
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    uint TexWidth, TexHeight;
    g_COCTex.GetDimensions(TexWidth, TexHeight);
    float2 uv = float2(input.position.x / TexWidth, input.position.y / TexHeight);
    float2 InvTexSize = float2(1.0f / TexWidth, 1.0f / TexHeight);
   
    float TotalCOC_R = 0;
    float TotalGaussian_R = 0;
    float TotalCOC_G = 0;
    float TotalGaussian_G = 0;
    
    float2 UVDir = float2(InvTexSize.x, 0);
    if (bHorizontal == 0)
    {
        UVDir = float2(0, InvTexSize.y);
    }
    
    int halfRange = Range / 2;

    //앞에있는건 뒤로만 퍼질 수 있다.
    //r은 클수록 앞에있음
    //g는 작을수록 앞에있음
    
    //2가지 방법
    //1. 가우시안 각 샘플마다 r은 클경우만 블러에 추가
    //2. 가우시안 최종 결과가 r이 클경우만 교체
    float3 COC = g_COCTex.Sample(g_LinearClampSample, uv).rgb;

    for (int i = -halfRange; i <= halfRange; ++i)
    {
        float CurGaussian = GetGaussian(i);
        float2 CurUV = uv + UVDir * i * COCSize;
        float3 CurCOC = g_COCTex.Sample(g_LinearClampSample, CurUV).rgb;
        //if (COC.r <= CurCOC.r) //뒤에께 앞으로 블러됨을 막는부분
        //{
        //    TotalCOC_R += CurCOC.r * CurGaussian;
        //    TotalGaussian_R += CurGaussian;
        //}
        //if (COC.g >= CurCOC.g)
        //{
        //    TotalCOC_G += CurCOC.g * CurGaussian;
        //    TotalGaussian_G += CurGaussian;
        //}
        TotalCOC_R += CurCOC.r * CurGaussian;
        TotalGaussian_R += CurGaussian;
        TotalCOC_G += CurCOC.g * CurGaussian;
        TotalGaussian_G += CurGaussian;
    }
    float3 FinalColor = float3(0, 0, 0);
    FinalColor.g = TotalCOC_G / TotalGaussian_G;
    FinalColor.r =  TotalCOC_R / TotalGaussian_R;
    return float4(FinalColor.r, COC.gb, 1); //R만 블러
    //return float4(COC, 1); //블러X
    return float4(FinalColor.rgb, 1); //RG 둘다 블러
}

