cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;
    float2 delta;
};

Texture2D source : register(t0);
SamplerState sourceSampler : register(s0);

float4 main(float4 position : SV_POSITION,
            float2 coord0 : TEXCOORD0,
            float2 coord1 : TEXCOORD1,
            float2 coord2 : TEXCOORD2,
            float2 coord3 : TEXCOORD3) : SV_TARGET
{
    return (source.Sample(sourceSampler, coord0) + source.Sample(sourceSampler, coord1)
            + source.Sample(sourceSampler, coord2) + source.Sample(sourceSampler, coord3)) * 0.25 * qt_Opacity;
}
