struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float opacity;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VS_VertexColor(VSInput input)
{
    PSInput result;

    result.position = mul(mvp, input.position);
    result.color = input.color * opacity;

    return result;
}

float4 PS_VertexColor(PSInput input) : SV_TARGET
{
    return input.color;
}
