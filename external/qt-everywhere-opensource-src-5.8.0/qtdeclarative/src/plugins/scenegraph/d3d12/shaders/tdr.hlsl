// http://gamedev.stackexchange.com/questions/108141/how-can-i-test-dxgi-error-device-removed-error-handling

RWBuffer<uint> uav;
cbuffer ConstantBuffer { uint zero; }

[numthreads(256, 1, 1)]
void timeout(uint3 id: SV_DispatchThreadID)
{
    while (zero == 0)
        uav[id.x] = zero;
}
