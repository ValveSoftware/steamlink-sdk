cbuffer ConstantBuffer : register(b0)
{
        float4x4 modelview;
        float4x4 projection;
        float opacity;
};

struct PSInput
{
        float4 position : SV_POSITION;
        float3 color : COLOR;
};

PSInput VS_Simple(float4 position : POSITION, float3 color : COLOR)
{
        PSInput result;

        float4x4 mvp = mul(projection, modelview);
        result.position = mul(mvp, position);
        result.color = color;

        return result;
}

float4 PS_Simple(PSInput input) : SV_TARGET
{
        return float4(input.color, 1.0) * opacity;
}
