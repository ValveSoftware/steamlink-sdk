cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 coord : TEXCOORD0;
};

Texture2D source : register(t0);
SamplerState sourceSampler : register(s0);

PSInput VS_DefaultShaderEffect(float4 position : POSITION, float2 coord : TEXCOORD0)
{
    PSInput result;
    result.position = mul(qt_Matrix, position);
    result.coord = coord;
    return result;
}

float4 PS_DefaultShaderEffect(PSInput input) : SV_TARGET
{
    return source.Sample(sourceSampler, input.coord) * qt_Opacity;
}
