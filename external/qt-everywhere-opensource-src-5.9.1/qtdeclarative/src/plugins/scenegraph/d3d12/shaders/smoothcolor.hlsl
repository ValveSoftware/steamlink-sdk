struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
    float2 offset : TEXCOORD0;
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
    float4 color : COLOR;
};

PSInput VS_SmoothColor(VSInput input)
{
    PSInput result;

    float4 pos = mul(mvp, input.position);

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
    }

    result.position = pos;
    result.color = input.color * opacity;
    return result;
}

float4 PS_SmoothColor(PSInput input) : SV_TARGET
{
    return input.color;
}
