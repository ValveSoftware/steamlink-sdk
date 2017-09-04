struct VSInput
{
    float4 position : POSITION;
    float2 coord : TEXCOORD0;
    float2 offset : TEXCOORD1;
    float2 coordOffset : TEXCOORD2;
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvp;
    float opacity;
    float2 pixelSize;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 coord : TEXCOORD0;
    float vertexOpacity : TEXCOORD3;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

PSInput VS_SmoothTexture(VSInput input)
{
    PSInput result;

    float4 pos = mul(mvp, input.position);
    float2 coord = input.coord;

    if (input.offset.x != 0.0) {
        // In HLSL matrix packing is column-major by default (which is good) but the math is row-major (unlike GLSL).
        float4 delta = float4(mvp._11, mvp._21, mvp._31, mvp._41) * input.offset.x;
        float2 dir = delta.xy * pos.w - pos.xy * delta.w;
        float2 ndir = 0.5 * pixelSize * normalize(dir / pixelSize);
        dir -= ndir * delta.w * pos.w;
        float numerator = dot(dir, ndir * pos.w * pos.w);
        float scale = 0.0;
        if (numerator < 0.0)
            scale = 1.0;
        else
            scale = min(1.0, numerator / dot(dir, dir));
        pos += scale * delta;
        coord.x += scale * input.coordOffset.x;
    }

    if (input.offset.y != 0.0) {
        float4 delta = float4(mvp._12, mvp._22, mvp._32, mvp._42) * input.offset.y;
        float2 dir = delta.xy * pos.w - pos.xy * delta.w;
        float2 ndir = 0.5 * pixelSize * normalize(dir / pixelSize);
        dir -= ndir * delta.w * pos.w;
        float numerator = dot(dir, ndir * pos.w * pos.w);
        float scale = 0.0;
        if (numerator < 0.0)
            scale = 1.0;
        else
            scale = min(1.0, numerator / dot(dir, dir));
        pos += scale * delta;
        coord.y += scale * input.coordOffset.y;
    }

    if ((input.offset.x != 0.0 || input.offset.y != 0.0) && (input.coordOffset.x == 0.0 && input.coordOffset.y == 0.0))
        result.vertexOpacity = 0.0;
    else
        result.vertexOpacity = opacity;

    result.position = pos;
    result.coord = coord;
    return result;
}

float4 PS_SmoothTexture(PSInput input) : SV_TARGET
{
    return tex.Sample(samp, input.coord) * input.vertexOpacity;
}
