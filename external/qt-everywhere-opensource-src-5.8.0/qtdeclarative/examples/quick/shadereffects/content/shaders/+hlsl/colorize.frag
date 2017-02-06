cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;
    float4 tint;
};

Texture2D source : register(t0);
SamplerState sourceSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 coord : TEXCOORD0) : SV_TARGET
{
    float4 c = source.Sample(sourceSampler, coord);
    float lo = min(min(c.x, c.y), c.z);
    float hi = max(max(c.x, c.y), c.z);
    return float4(lerp(float3(lo, lo, lo), float3(hi, hi, hi), tint.xyz), c.w) * qt_Opacity;
}
