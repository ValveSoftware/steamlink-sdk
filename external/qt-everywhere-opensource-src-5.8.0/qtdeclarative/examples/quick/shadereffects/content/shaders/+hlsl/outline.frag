cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;
    float2 delta;
};

Texture2D source : register(t0);
SamplerState sourceSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 coord : TEXCOORD0) : SV_TARGET
{
    float4 tl = source.Sample(sourceSampler, coord - delta);
    float4 tr = source.Sample(sourceSampler, coord + float2(delta.x, -delta.y));
    float4 bl = source.Sample(sourceSampler, coord - float2(delta.x, -delta.y));
    float4 br = source.Sample(sourceSampler, coord + delta);
    float4 gx = (tl + bl) - (tr + br);
    float4 gy = (tl + tr) - (bl + br);
    return float4(0.0, 0.0, 0.0,
                  clamp(dot(sqrt(gx * gx + gy * gy), float4(1.0, 1.0, 1.0, 1.0)), 0.0, 1.0) * qt_Opacity);
}
