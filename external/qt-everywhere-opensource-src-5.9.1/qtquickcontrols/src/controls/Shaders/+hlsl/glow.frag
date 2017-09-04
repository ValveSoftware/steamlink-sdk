cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;
    float weight1;
    float weight2;
    float weight3;
    float weight4;
    float weight5;
    float spread;
    float4 color;
};

Texture2D source1 : register(t0);
SamplerState sourceSampler1 : register(s0);
Texture2D source2 : register(t1);
SamplerState sourceSampler2 : register(s1);
Texture2D source3 : register(t2);
SamplerState sourceSampler3 : register(s2);
Texture2D source4 : register(t3);
SamplerState sourceSampler4 : register(s3);
Texture2D source5 : register(t4);
SamplerState sourceSampler5 : register(s4);

float linearstep(float e0, float e1, float x)
{
    return clamp((x - e0) / (e1 - e0), 0.0, 1.0);
}

float4 main(float4 position : SV_POSITION, float2 coord : TEXCOORD0) : SV_TARGET
{
    float4 sourceColor = source1.Sample(sourceSampler1, coord) * weight1;
    sourceColor += source2.Sample(sourceSampler2, coord) * weight2;
    sourceColor += source3.Sample(sourceSampler3, coord) * weight3;
    sourceColor += source4.Sample(sourceSampler4, coord) * weight4;
    sourceColor += source5.Sample(sourceSampler5, coord) * weight5;
    sourceColor = lerp(float4(0.0, 0.0, 0.0, 0.0), color, linearstep(0.0, spread, sourceColor.a));
    return sourceColor * qt_Opacity;
}
