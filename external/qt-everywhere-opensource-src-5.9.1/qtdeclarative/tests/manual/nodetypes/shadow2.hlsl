cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;
    float2 offset;
    float darkness;
    float2 delta;
};

Texture2D source : register(t0);
Texture2D shadow : register(t1);
SamplerState samp : register(s0);
// Use the same sampler for both textures. In fact the engine will create an extra static sampler
// in any case (to match the number of textures) due to some internals, but that won't hurt, the
// shader works either way.

float4 PS_Shadow2(float4 position : SV_POSITION, float2 coord : TEXCOORD0) : SV_TARGET
{
    float4 fg = source.Sample(samp, coord);
    float4 bg = shadow.Sample(samp, coord + delta);
    return (fg + float4(0.0, 0.0, 0.0, darkness * bg.a) * (1.0 - fg.a)) * qt_Opacity;
}
