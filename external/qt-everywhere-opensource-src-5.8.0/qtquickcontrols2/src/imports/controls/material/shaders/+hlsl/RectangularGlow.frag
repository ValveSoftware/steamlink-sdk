cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;
    float relativeSizeX;
    float relativeSizeY;
    float spread;
    float4 color;
}

float linearstep(float e0, float e1, float x) { return clamp((x - e0) / (e1 - e0), 0.0, 1.0); }

float4 main(float4 position : SV_POSITION, float2 coord : TEXCOORD0) : SV_TARGET
{
    float alpha =
        smoothstep(0.0, relativeSizeX, 0.5 - abs(0.5 - coord.x)) *
        smoothstep(0.0, relativeSizeY, 0.5 - abs(0.5 - coord.y));

    float spreadMultiplier = linearstep(spread, 1.0 - spread, alpha);
    return color * qt_Opacity * spreadMultiplier * spreadMultiplier;
}
