struct VSInput
{
    float4 position : POSITION;
    float2 coord : TEXCOORD0;
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4 animPos;
    float3 animData;
    float opacity;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 fTexS : TEXCOORD0;
    float progress : TEXCOORD1;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

PSInput VS_Sprite(VSInput input)
{
    PSInput result;

    result.position = mul(mvp, input.position);
    result.progress = animData.z;

    // Calculate frame location in texture
    result.fTexS.xy = animPos.xy + input.coord.xy * animData.xy;
    // Next frame is also passed, for interpolation
    result.fTexS.zw = animPos.zw + input.coord.xy * animData.xy;

    return result;
}

float4 PS_Sprite(PSInput input) : SV_TARGET
{
    return lerp(tex.Sample(samp, input.fTexS.xy), tex.Sample(samp, input.fTexS.zw), input.progress) * opacity;
}
