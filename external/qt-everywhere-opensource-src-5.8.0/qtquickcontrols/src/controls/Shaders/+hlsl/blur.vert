cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;
    float xStep;
    float yStep;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 coord0 : TEXCOORD0;
    float2 coord1 : TEXCOORD1;
    float2 coord2 : TEXCOORD2;
    float2 coord3 : TEXCOORD3;
};

PSInput main(float4 position : POSITION, float2 coord : TEXCOORD0)
{
    PSInput result;

    result.position = mul(qt_Matrix, position);

    result.coord0 = float2(coord.x + xStep, coord.y + yStep * 0.36);
    result.coord1 = float2(coord.x + xStep * 0.36, coord.y - yStep);
    result.coord2 = float2(coord.x - xStep * 0.36, coord.y + yStep);
    result.coord3 = float2(coord.x - xStep, coord.y - yStep * 0.36);

    return result;
}
