static const uint GROUP_DIM = 8; // 2 ^ (out_mip_count - 1)

Texture2D tex : register(t0);
SamplerState samp : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    uint2 mip1Size;
    uint sampleLevel;
    uint totalMips;
}

RWTexture2D<float4> mip1 : register(u0);
RWTexture2D<float4> mip2 : register(u1);
RWTexture2D<float4> mip3 : register(u2);
RWTexture2D<float4> mip4 : register(u3);

groupshared float4 groupColor[GROUP_DIM][GROUP_DIM];

[numthreads(GROUP_DIM, GROUP_DIM, 1)]
void CS_Generate4MipMaps(uint3 localId: SV_GroupThreadId, uint3 globalId: SV_DispatchThreadID)
{
    const float2 coord = float2(1.0f / float(mip1Size.x), 1.0f / float(mip1Size.y)) * (globalId.xy + 0.5);
    float4 c = tex.SampleLevel(samp, coord, sampleLevel);

    mip1[globalId.xy] = c;
    groupColor[localId.y][localId.x] = c;

    if (sampleLevel + 1 >= totalMips)
        return;

    GroupMemoryBarrierWithGroupSync();

    if ((localId.x & 1) == 0 && (localId.y & 1) == 0) {
        c = (c + groupColor[localId.y][localId.x + 1] + groupColor[localId.y + 1][localId.x] + groupColor[localId.y + 1][localId.x + 1]) / 4.0;
        mip2[globalId.xy / 2] = c;
        groupColor[localId.y][localId.x] = c;
    }

    if (sampleLevel + 2 >= totalMips)
        return;

    GroupMemoryBarrierWithGroupSync();

    if ((localId.x & 3) == 0 && (localId.y & 3) == 0) {
        c = (c + groupColor[localId.y][localId.x + 2] + groupColor[localId.y + 2][localId.x] + groupColor[localId.y + 2][localId.x + 2]) / 4.0;
        mip3[globalId.xy / 4] = c;
        groupColor[localId.y][localId.x] = c;
    }

    if (sampleLevel + 3 >= totalMips)
        return;

    GroupMemoryBarrierWithGroupSync();

    if ((localId.x & 7) == 0 && (localId.y & 7) == 0) {
        c = (c + groupColor[localId.y][localId.x + 3] + groupColor[localId.y + 3][localId.x] + groupColor[localId.y + 3][localId.x + 3]) / 4.0;
        mip4[globalId.xy / 8] = c;
    }
}
