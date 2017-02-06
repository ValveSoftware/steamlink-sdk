struct VSInput
{
    float4 position : POSITION;
    float2 coord : TEXCOORD0;
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float2 textureScale;
    float dpr;
    float color; // for TextMask24 and 32
    float4 colorVec; // for TextMask8 and Styled and Outlined
    float2 shift; // for Styled
    float4 styleColor; // for Styled and Outlined
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 coord : TEXCOORD0;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

PSInput VS_TextMask(VSInput input)
{
    PSInput result;
    result.position = mul(mvp, floor(input.position * dpr + 0.5) / dpr);
    result.coord = input.coord * textureScale;
    return result;
}

float4 PS_TextMask24(PSInput input) : SV_TARGET
{
    float4 glyph = tex.Sample(samp, input.coord);
    return float4(glyph.rgb * color, glyph.a);
}

float4 PS_TextMask32(PSInput input) : SV_TARGET
{
    return tex.Sample(samp, input.coord) * color;
}

float4 PS_TextMask8(PSInput input) : SV_TARGET
{
    return colorVec * tex.Sample(samp, input.coord).a;
}

struct StyledPSInput
{
    float4 position : SV_POSITION;
    float2 coord : TEXCOORD0;
    float2 shiftedCoord : TEXCOORD1;
};

StyledPSInput VS_StyledText(VSInput input)
{
    StyledPSInput result;
    result.position = mul(mvp, floor(input.position * dpr + 0.5) / dpr);
    result.coord = input.coord * textureScale;
    result.shiftedCoord = (input.coord - shift) * textureScale;
    return result;
}

float4 PS_StyledText(StyledPSInput input) : SV_TARGET
{
    float glyph = tex.Sample(samp, input.coord).a;
    float style = clamp(tex.Sample(samp, input.shiftedCoord).a - glyph, 0.0, 1.0);
    return style * styleColor + glyph * colorVec;
}

struct OutlinedPSInput
{
    float4 position : SV_POSITION;
    float2 coord : TEXCOORD0;
    float2 coordUp : TEXCOORD1;
    float2 coordDown : TEXCOORD2;
    float2 coordLeft : TEXCOORD3;
    float2 coordRight : TEXCOORD4;
};

OutlinedPSInput VS_OutlinedText(VSInput input)
{
    OutlinedPSInput result;
    result.position = mul(mvp, floor(input.position * dpr + 0.5) / dpr);
    result.coord = input.coord * textureScale;
    result.coordUp = (input.coord - float2(0.0, -1.0)) * textureScale;
    result.coordDown = (input.coord - float2(0.0, 1.0)) * textureScale;
    result.coordLeft = (input.coord - float2(-1.0, 0.0)) * textureScale;
    result.coordRight = (input.coord - float2(1.0, 0.0)) * textureScale;
    return result;
}

float4 PS_OutlinedText(OutlinedPSInput input) : SV_TARGET
{
    float glyph = tex.Sample(samp, input.coord).a;
    float outline = clamp(clamp(tex.Sample(samp, input.coordUp).a
        + tex.Sample(samp, input.coordDown).a
        + tex.Sample(samp, input.coordLeft).a
        + tex.Sample(samp, input.coordRight).a, 0.0, 1.0) - glyph, 0.0, 1.0);
    return outline * styleColor + glyph * colorVec;
}
