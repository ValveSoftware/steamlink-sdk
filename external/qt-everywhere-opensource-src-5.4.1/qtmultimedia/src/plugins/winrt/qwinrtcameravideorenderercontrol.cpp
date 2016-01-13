/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwinrtcameravideorenderercontrol.h"

#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QSize>
#include <QtCore/QVector>

#include <d3d11.h>
#include <mfapi.h>
#include <wrl.h>
using namespace Microsoft::WRL;

QT_USE_NAMESPACE

class D3DVideoBlitter
{
public:
    D3DVideoBlitter(ID3D11Device *device, ID3D11Texture2D *target)
        : m_d3dDevice(device), m_target(target)
    {
        HRESULT hr;
        ComPtr<IDXGIResource> targetResource;
        hr = target->QueryInterface(IID_PPV_ARGS(&targetResource));
        Q_ASSERT_SUCCEEDED(hr);
        HANDLE sharedHandle;
        hr = targetResource->GetSharedHandle(&sharedHandle);
        Q_ASSERT_SUCCEEDED(hr);
        hr = m_d3dDevice->OpenSharedResource(sharedHandle, IID_PPV_ARGS(&m_targetTexture));
        Q_ASSERT_SUCCEEDED(hr);
        hr = m_d3dDevice.As(&m_videoDevice);
        Q_ASSERT_SUCCEEDED(hr);
    }

    ID3D11Device *device() const
    {
        return m_d3dDevice.Get();
    }

    ID3D11Texture2D *target() const
    {
        return m_target;
    }

    void blit(ID3D11Texture2D *texture)
    {
        HRESULT hr;
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        if (!m_videoEnumerator) {
            D3D11_VIDEO_PROCESSOR_CONTENT_DESC videoProcessorDesc = {
                D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
                { 0 }, desc.Width, desc.Height,
                { 0 }, desc.Width, desc.Height,
                D3D11_VIDEO_USAGE_PLAYBACK_NORMAL
            };
            hr = m_videoDevice->CreateVideoProcessorEnumerator(&videoProcessorDesc, &m_videoEnumerator);
            RETURN_VOID_IF_FAILED("Failed to create video enumerator");
        }

        if (!m_videoProcessor) {
            hr = m_videoDevice->CreateVideoProcessor(m_videoEnumerator.Get(), 0, &m_videoProcessor);
            RETURN_VOID_IF_FAILED("Failed to create video processor");
        }

        if (!m_outputView) {
            D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc = { D3D11_VPOV_DIMENSION_TEXTURE2D };
            hr = m_videoDevice->CreateVideoProcessorOutputView(
                        m_targetTexture.Get(), m_videoEnumerator.Get(), &outputDesc, &m_outputView);
            RETURN_VOID_IF_FAILED("Failed to create video output view");
        }

        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = {
            0, D3D11_VPIV_DIMENSION_TEXTURE2D, { 0, 0 }
        };
        ComPtr<ID3D11VideoProcessorInputView> inputView;
        hr = m_videoDevice->CreateVideoProcessorInputView(
                    texture, m_videoEnumerator.Get(), &inputViewDesc, &inputView);
        RETURN_VOID_IF_FAILED("Failed to create video input view");

        ComPtr<ID3D11DeviceContext> context;
        ComPtr<ID3D11VideoContext> videoContext;
        m_d3dDevice->GetImmediateContext(&context);
        hr = context.As(&videoContext);
        RETURN_VOID_IF_FAILED("Failed to get video context");

        D3D11_VIDEO_PROCESSOR_STREAM stream = { TRUE };
        stream.pInputSurface = inputView.Get();
        hr = videoContext->VideoProcessorBlt(
                    m_videoProcessor.Get(), m_outputView.Get(), 0, 1, &stream);
        RETURN_VOID_IF_FAILED("Failed to get blit video frame");
    }

private:
    ComPtr<ID3D11Device> m_d3dDevice;
    ComPtr<ID3D11Texture2D> m_targetTexture;
    ID3D11Texture2D *m_target;
    ComPtr<ID3D11VideoDevice> m_videoDevice;
    ComPtr<ID3D11VideoProcessorEnumerator> m_videoEnumerator;
    ComPtr<ID3D11VideoProcessor> m_videoProcessor;
    ComPtr<ID3D11VideoProcessorOutputView> m_outputView;
};

class QWinRTCameraVideoRendererControlPrivate
{
public:
    QScopedPointer<D3DVideoBlitter> blitter;
    QVector<ComPtr<IMF2DBuffer>> buffers;
};

QWinRTCameraVideoRendererControl::QWinRTCameraVideoRendererControl(const QSize &size, QObject *parent)
    : QWinRTAbstractVideoRendererControl(size, parent), d_ptr(new QWinRTCameraVideoRendererControlPrivate)
{
}

QWinRTCameraVideoRendererControl::~QWinRTCameraVideoRendererControl()
{
    shutdown();
}

bool QWinRTCameraVideoRendererControl::render(ID3D11Texture2D *target)
{
    Q_D(QWinRTCameraVideoRendererControl);

    if (d->buffers.isEmpty()) {
        emit bufferRequested();
        return false;
    }

    HRESULT hr;
    ComPtr<IMF2DBuffer> buffer = d->buffers.takeFirst();

    ComPtr<ID3D11Texture2D> sourceTexture;
    ComPtr<IMFDXGIBuffer> dxgiBuffer;
    hr = buffer.As(&dxgiBuffer);
    Q_ASSERT_SUCCEEDED(hr);
    hr = dxgiBuffer->GetResource(IID_PPV_ARGS(&sourceTexture));
    if (FAILED(hr)) {
        qErrnoWarning(hr, "The video frame does not support texture output; aborting rendering.");
        return false;
    }

    ComPtr<ID3D11Device> device;
    sourceTexture->GetDevice(&device);
    if (!d->blitter || d->blitter->device() != device.Get() || d->blitter->target() != target)
        d->blitter.reset(new D3DVideoBlitter(device.Get(), target));

    d->blitter->blit(sourceTexture.Get());

    emit bufferRequested();
    return true;
}

void QWinRTCameraVideoRendererControl::queueBuffer(IMF2DBuffer *buffer)
{
    Q_D(QWinRTCameraVideoRendererControl);
    Q_ASSERT(buffer);
    d->buffers.append(buffer);
}

void QWinRTCameraVideoRendererControl::discardBuffers()
{
    Q_D(QWinRTCameraVideoRendererControl);
    d->buffers.clear();
}
