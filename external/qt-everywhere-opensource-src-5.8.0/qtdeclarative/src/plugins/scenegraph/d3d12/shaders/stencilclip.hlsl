struct VSInput
{
    float4 position : POSITION;
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

PSInput VS_StencilClip(VSInput input)
{
    PSInput result;
    result.position = mul(mvp, input.position);
    return result;
}

float4 PS_StencilClip(PSInput input) : SV_TARGET
{
    return float4(0.81, 0.83, 0.12, 1.0); // Trolltech green ftw!
}
