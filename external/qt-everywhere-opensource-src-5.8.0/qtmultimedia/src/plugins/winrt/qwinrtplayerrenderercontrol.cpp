/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwinrtplayerrenderercontrol.h"

#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QGlobalStatic>
#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>
#include <QtGui/QGuiApplication>
#include <QtGui/QOpenGLTexture>
#include <QtGui/QOpenGLContext>

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <d3d11.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <wrl.h>
using namespace Microsoft::WRL;

QT_BEGIN_NAMESPACE

template <typename T>
class D3DDeviceLocker
{
public:
    D3DDeviceLocker(IMFDXGIDeviceManager *manager, HANDLE deviceHandle, T **device)
        : m_manager(manager), m_deviceHandle(deviceHandle)
    {
        HRESULT hr = m_manager->LockDevice(m_deviceHandle, IID_PPV_ARGS(device), TRUE);
        if (FAILED(hr))
            qErrnoWarning(hr, "Failed to lock the D3D device");
    }

    ~D3DDeviceLocker()
    {
        HRESULT hr = m_manager->UnlockDevice(m_deviceHandle, FALSE);
        if (FAILED(hr))
            qErrnoWarning(hr, "Failed to unlock the D3D device");
    }

private:
    IMFDXGIDeviceManager *m_manager;
    HANDLE m_deviceHandle;
};

class QWinRTPlayerRendererControlPrivate
{
public:
    ComPtr<IMFMediaEngineEx> engine;
    ComPtr<IMFDXGIDeviceManager> manager;
    quint32 resetToken;
    HANDLE deviceHandle;
};

QWinRTPlayerRendererControl::QWinRTPlayerRendererControl(IMFMediaEngineEx *engine, IMFDXGIDeviceManager *manager, quint32 resetToken, QObject *parent)
    : QWinRTAbstractVideoRendererControl(QSize(), parent), d_ptr(new QWinRTPlayerRendererControlPrivate)
{
    Q_D(QWinRTPlayerRendererControl);

    d->engine = engine;
    d->manager = manager;
    d->resetToken = resetToken;
    d->deviceHandle = 0;
}

QWinRTPlayerRendererControl::~QWinRTPlayerRendererControl()
{
    shutdown();
}

bool QWinRTPlayerRendererControl::ensureReady()
{
    Q_D(QWinRTPlayerRendererControl);

    HRESULT hr;
    hr = d->manager->TestDevice(d->deviceHandle);
    if (hr == MF_E_DXGI_NEW_VIDEO_DEVICE || FAILED(hr)) {
        d->manager->CloseDeviceHandle(d->deviceHandle);

        hr = d->manager->ResetDevice(d3dDevice(), d->resetToken);
        RETURN_FALSE_IF_FAILED("Failed to reset the DXGI manager device");

        hr = d->manager->OpenDeviceHandle(&d->deviceHandle);
        RETURN_FALSE_IF_FAILED("Failed to open device handle");
    }

    return SUCCEEDED(hr);
}

bool QWinRTPlayerRendererControl::render(ID3D11Texture2D *texture)
{
    Q_D(QWinRTPlayerRendererControl);

    if (!ensureReady())
        return false;

    HRESULT hr;
    LONGLONG ts;
    hr = d->engine->OnVideoStreamTick(&ts);
    RETURN_FALSE_IF_FAILED("Failed to obtain video stream tick");
    if (hr == S_FALSE) // Frame not available, nothing to do
        return false;

    ComPtr<ID3D11Device> device;
    D3DDeviceLocker<ID3D11Device> locker(d->manager.Get(), d->deviceHandle, device.GetAddressOf());

    MFVideoNormalizedRect sourceRect = { 0, 0, 1, 1 };
    const QSize frameSize = size();
    RECT destRect = { 0, 0, frameSize.width(), frameSize.height() };
    MFARGB borderColor = { 255, 255, 255, 255 };
    hr = d->engine->TransferVideoFrame(texture, &sourceRect, &destRect, &borderColor);
    RETURN_FALSE_IF_FAILED("Failed to transfer video frame to DXGI surface");
    return true;
}

QT_END_NAMESPACE
