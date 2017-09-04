cbuffer ConstantBuffer : register(b0)
{
    float4x4 qt_Matrix;
    float qt_Opacity;
    float bend;
    float minimize;
    float side;
    float width;
    float height;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 coord : TEXCOORD0;
};

PSInput main(float4 position : POSITION, float2 coord : TEXCOORD0)
{
    PSInput result;
    result.coord = coord;

    float4 pos = position;
    pos.y = lerp(position.y, height, minimize);
    float t = pos.y / height;
    t = (3.0 - 2.0 * t) * t * t;
    pos.x = lerp(position.x, side * width, t * bend);
    result.position = mul(qt_Matrix, pos);

    return result;
}
