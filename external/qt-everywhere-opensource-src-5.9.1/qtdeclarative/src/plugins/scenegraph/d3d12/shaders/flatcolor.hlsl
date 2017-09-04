struct VSInput
{
    float4 position : POSITION;
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 color;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

PSInput VS_FlatColor(VSInput input)
{
    PSInput result;
    result.position = mul(mvp, input.position);
    return result;
}

float4 PS_FlatColor(PSInput input) : SV_TARGET
{
    return color;
}
