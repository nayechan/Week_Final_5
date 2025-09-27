cbuffer CBPerObject : register(b0)
{
    float4x4 gModel;
    float4x4 gView;
    float4x4 gProj;
};

struct VSIN
{
    float3 pos : POSITION;
};
struct VSOUT
{
    float4 svpos : SV_POSITION;
};

VSOUT VSMain(VSIN i)
{
    VSOUT o;
    float4 wp = mul(float4(i.pos, 1), gModel); // row-vector 가정
    float4 vp = mul(wp, gView);
    o.svpos = mul(vp, gProj);
    return o;
}

float4 PSMain(VSOUT i) : SV_TARGET
{
    return 0; // ColorWriteMask=0 (OM 설정)
}