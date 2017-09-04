cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;

    float amplitude;
    float frequency;
    float time;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 coord : TEXCOORD0;
};

PSInput VS_Wobble(float4 position : POSITION, float2 coord : TEXCOORD0)
{
    PSInput result;
    result.position = mul(qt_Matrix, position);
    result.coord = coord;
    return result;
}

Texture2D source : register(t0);
SamplerState sourceSampler : register(s0);

float4 PS_Wobble(PSInput input) : SV_TARGET
{
    float2 p = sin(time + frequency * input.coord);
    return source.Sample(sourceSampler, input.coord + amplitude * float2(p.y, -p.x)) * qt_Opacity;
}
