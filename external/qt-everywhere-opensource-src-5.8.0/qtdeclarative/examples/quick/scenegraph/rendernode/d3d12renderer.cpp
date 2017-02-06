/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "d3d12renderer.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QFile>

#if QT_CONFIG(d3d12)

D3D12RenderNode::D3D12RenderNode(QQuickItem *item)
    : m_item(item)
{
}

D3D12RenderNode::~D3D12RenderNode()
{
    releaseResources();
}

void D3D12RenderNode::releaseResources()
{
    if (vbPtr) {
        vertexBuffer->Unmap(0, nullptr);
        vbPtr = nullptr;
    }
    if (cbPtr) {
        constantBuffer->Unmap(0, nullptr);
        cbPtr = nullptr;
    }
    constantBuffer = nullptr;
    vertexBuffer = nullptr;
    rootSignature = nullptr;
    pipelineState = nullptr;
    m_device = nullptr;
}

void D3D12RenderNode::init()
{
    QSGRendererInterface *rif = m_item->window()->rendererInterface();
    m_device = static_cast<ID3D12Device *>(rif->getResource(m_item->window(), QSGRendererInterface::DeviceResource));
    Q_ASSERT(m_device);

    D3D12_ROOT_PARAMETER rootParameter;
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameter.Descriptor.ShaderRegister = 0; // b0
    rootParameter.Descriptor.RegisterSpace = 0;

    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.NumParameters = 1;
    desc.pParameters = &rootParameter;
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = nullptr;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        qWarning("Failed to serialize root signature");
        return;
    }
    if (FAILED(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                             IID_PPV_ARGS(&rootSignature)))) {
        qWarning("Failed to create root signature");
        return;
    }

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    QFile f(QStringLiteral(":/scenegraph/rendernode/shader_vert.cso"));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("Failed to open file with vertex shader bytecode");
        return;
    }
    QByteArray vshader_cso = f.readAll();
    f.close();
    f.setFileName(QStringLiteral(":/scenegraph/rendernode/shader_frag.cso"));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("Failed to open file with fragment shader bytecode");
        return;
    }
    QByteArray fshader_cso = f.readAll();
    D3D12_SHADER_BYTECODE vshader;
    vshader.pShaderBytecode = vshader_cso.constData();
    vshader.BytecodeLength = vshader_cso.size();
    D3D12_SHADER_BYTECODE pshader;
    pshader.pShaderBytecode = fshader_cso.constData();
    pshader.BytecodeLength = fshader_cso.size();

    D3D12_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rastDesc.CullMode = D3D12_CULL_MODE_BACK;
    rastDesc.FrontCounterClockwise = TRUE; // Vertices are given CCW

    // Enable color write and blending (premultiplied alpha). The latter is
    // needed because the example changes the item's opacity and we pass
    // inheritedOpacity() into the pixel shader. If that wasn't the case,
    // blending could have stayed disabled.
    const D3D12_RENDER_TARGET_BLEND_DESC premulBlendDesc = {
        TRUE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };
    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0] = premulBlendDesc;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = vshader;
    psoDesc.PS = pshader;
    psoDesc.RasterizerState = rastDesc;
    psoDesc.BlendState = blendDesc;
    // No depth. The correct stacking of the item is ensured by the projection matrix.
    // Do not bother with stencil since we do not apply clipping in the
    // example. If clipping is desired, render() needs to set a different PSO
    // with stencil enabled whenever the RenderState indicates so.
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // not in use due to !DepthEnable, but this would be the correct format otherwise
    // We are rendering on the default render target so if the QuickWindow/View
    // has requested samples > 0 then we have to follow suit.
    const uint samples = qMax(1, m_item->window()->format().samples());
    psoDesc.SampleDesc.Count = samples;
    if (samples > 1) {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaInfo = {};
        msaaInfo.Format = psoDesc.RTVFormats[0];
        msaaInfo.SampleCount = samples;
        if (SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaInfo, sizeof(msaaInfo)))) {
            if (msaaInfo.NumQualityLevels > 0)
                psoDesc.SampleDesc.Quality = msaaInfo.NumQualityLevels - 1;
        }
    }

    if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))) {
        qWarning("Failed to create graphics pipeline state");
        return;
    }

    const UINT vertexBufferSize = (2 + 3) * 3 * sizeof(float);

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Alignment = 0;
    bufDesc.Width = vertexBufferSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (FAILED(m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(&vertexBuffer)))) {
        qWarning("Failed to create committed resource (vertex buffer)");
        return;
    }

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = vertexBufferSize / 3;
    vertexBufferView.SizeInBytes = vertexBufferSize;

    bufDesc.Width = 256;
    if (FAILED(m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(&constantBuffer)))) {
        qWarning("Failed to create committed resource (constant buffer)");
        return;
    }

    const D3D12_RANGE readRange = { 0, 0 };
    if (FAILED(vertexBuffer->Map(0, &readRange, reinterpret_cast<void **>(&vbPtr)))) {
        qWarning("Map failed");
        return;
    }

    if (FAILED(constantBuffer->Map(0, &readRange, reinterpret_cast<void **>(&cbPtr)))) {
        qWarning("Map failed (constant buffer)");
        return;
    }

    float *vp = reinterpret_cast<float *>(vbPtr);
    vp += 2;
    *vp++ = 1.0f; *vp++ = 0.0f; *vp++ = 0.0f;
    vp += 2;
    *vp++ = 0.0f; *vp++ = 1.0f; *vp++ = 0.0f;
    vp += 2;
    *vp++ = 0.0f; *vp++ = 0.0f; *vp++ = 1.0f;
}

void D3D12RenderNode::render(const RenderState *state)
{
    if (!m_device)
        init();

    QSGRendererInterface *rif = m_item->window()->rendererInterface();
    ID3D12GraphicsCommandList *commandList = static_cast<ID3D12GraphicsCommandList *>(
        rif->getResource(m_item->window(), QSGRendererInterface::CommandListResource));
    Q_ASSERT(commandList);

    const int msize = 16 * sizeof(float);
    memcpy(cbPtr, matrix()->constData(), msize);
    memcpy(cbPtr + msize, state->projectionMatrix()->constData(), msize);
    const float opacity = inheritedOpacity();
    memcpy(cbPtr + 2 * msize, &opacity, sizeof(float));

    const QPointF p0(m_item->width() - 1, m_item->height() - 1);
    const QPointF p1(0, 0);
    const QPointF p2(0, m_item->height() - 1);

    float *vp = reinterpret_cast<float *>(vbPtr);
    *vp++ = p0.x();
    *vp++ = p0.y();
    vp += 3;
    *vp++ = p1.x();
    *vp++ = p1.y();
    vp += 3;
    *vp++ = p2.x();
    *vp++ = p2.y();

    commandList->SetPipelineState(pipelineState.Get());
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

    commandList->DrawInstanced(3, 1, 0, 0);
}

// No need to reimplement changedStates() because no relevant commands are
// added to the command list in render().

QSGRenderNode::RenderingFlags D3D12RenderNode::flags() const
{
    return BoundedRectRendering | DepthAwareRendering;
}

QRectF D3D12RenderNode::rect() const
{
    return QRect(0, 0, m_item->width(), m_item->height());
}

#endif // d3d12
