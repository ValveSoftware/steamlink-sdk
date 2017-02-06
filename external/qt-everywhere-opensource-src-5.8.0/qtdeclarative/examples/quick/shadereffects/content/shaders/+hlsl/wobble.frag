cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;
    float amplitude;
    float frequency;
    float time;
};

Texture2D source : register(t0);
SamplerState sourceSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 coord : TEXCOORD0) : SV_TARGET
{
    float2 p = sin(time + frequency * coord);
    return source.Sample(sourceSampler, coord + amplitude * float2(p.y, -p.x)) * qt_Opacity;
}
