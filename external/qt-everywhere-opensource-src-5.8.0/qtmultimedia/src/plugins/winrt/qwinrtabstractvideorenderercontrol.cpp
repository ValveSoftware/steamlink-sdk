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

#include "qwinrtabstractvideorenderercontrol.h"

#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QGlobalStatic>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMetaMethod>
#include <QtCore/QPointer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLTexture>
#include <QtMultimedia/QAbstractVideoBuffer>
#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <d3d11.h>
#include <mfapi.h>
#include <wrl.h>

using namespace Microsoft::WRL;

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcMMVideoRender, "qt.mm.videorender")

#define BREAK_IF_FAILED(msg) RETURN_IF_FAILED(msg, break)
#define CONTINUE_IF_FAILED(msg) RETURN_IF_FAILED(msg, continue)

// Global D3D device to be shared between video surfaces
struct QWinRTVideoRendererControlGlobal
{
    QWinRTVideoRendererControlGlobal()
    {
        qCDebug(lcMMVideoRender) << __FUNCTION__;
        HRESULT hr;

        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };

        UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
                               featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
                               &device, &featureLevel, &context);
#ifdef _DEBUG
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Failed to create D3D device with device debug flag");
            hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                                   featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
                                   &device, &featureLevel, &context);
        }
#endif
        if (FAILED(hr))
            qErrnoWarning(hr, "Failed to create D3D device");

        if (!device || FAILED(hr)) {
            hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags,
                                   featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
                                   &device, &featureLevel, &context);
            if (FAILED(hr)) {
                qErrnoWarning(hr, "Failed to create software D3D device");
                return;
            }
        }

        ComPtr<ID3D10Multithread> multithread;
        hr = device.As(&multithread);
        Q_ASSERT_SUCCEEDED(hr);
        hr = multithread->SetMultithreadProtected(true);
        Q_ASSERT_SUCCEEDED(hr);

        ComPtr<IDXGIDevice> dxgiDevice;
        hr = device.As(&dxgiDevice);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IDXGIAdapter> adapter;
        hr = dxgiDevice->GetAdapter(&adapter);
        Q_ASSERT_SUCCEEDED(hr);
        hr = adapter->EnumOutputs(0, &output);
        Q_ASSERT_SUCCEEDED(hr);
    }

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL featureLevel;
    ComPtr<IDXGIOutput> output;
};
Q_GLOBAL_STATIC(QWinRTVideoRendererControlGlobal, g)

class QWinRTVideoBuffer : public QAbstractVideoBuffer, public QOpenGLTexture
{
public:
    QWinRTVideoBuffer(const QSize &size, TextureFormat format)
        : QAbstractVideoBuffer(QAbstractVideoBuffer::GLTextureHandle)
        , QOpenGLTexture(QOpenGLTexture::Target2D)
    {
        setSize(size.width(), size.height());
        setFormat(format);
        create();
    }

    MapMode mapMode() const Q_DECL_OVERRIDE
    {
        return NotMapped;
    }

    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine) Q_DECL_OVERRIDE
    {
        Q_UNUSED(mode);
        Q_UNUSED(numBytes);
        Q_UNUSED(bytesPerLine);
        return 0;
    }

    void unmap() Q_DECL_OVERRIDE
    {
    }

    QVariant handle() const Q_DECL_OVERRIDE
    {
        return QVariant::fromValue(textureId());
    }
};

enum DirtyState {
    NotDirty,     // All resources have been created
    TextureDirty, // The shared D3D texture needs to be recreated
    SurfaceDirty  // The shared EGL surface needs to be recreated
};

class QWinRTAbstractVideoRendererControlPrivate
{
public:
    QPointer<QAbstractVideoSurface> surface;
    QVideoSurfaceFormat format;

    DirtyState dirtyState;

    HANDLE shareHandle;
    ComPtr<ID3D11Texture2D> texture;

    EGLDisplay eglDisplay;
    EGLConfig eglConfig;
    EGLSurface eglSurface;

    QVideoFrame presentFrame;

    QThread renderThread;
    bool active;
    QWinRTAbstractVideoRendererControl::BlitMode blitMode;
    CRITICAL_SECTION mutex;
};

ID3D11Device *QWinRTAbstractVideoRendererControl::d3dDevice()
{
    return g->device.Get();
}

// This is required so that subclasses can stop the render thread before deletion
void QWinRTAbstractVideoRendererControl::shutdown()
{
    qCDebug(lcMMVideoRender) << __FUNCTION__;
    Q_D(QWinRTAbstractVideoRendererControl);
    if (d->renderThread.isRunning()) {
        d->renderThread.requestInterruption();
        d->renderThread.wait();
    }
}

QWinRTAbstractVideoRendererControl::QWinRTAbstractVideoRendererControl(const QSize &size, QObject *parent)
    : QVideoRendererControl(parent), d_ptr(new QWinRTAbstractVideoRendererControlPrivate)
{
    qCDebug(lcMMVideoRender) << __FUNCTION__;
    Q_D(QWinRTAbstractVideoRendererControl);

    d->format = QVideoSurfaceFormat(size, QVideoFrame::Format_BGRA32,
                                    QAbstractVideoBuffer::GLTextureHandle);
    d->dirtyState = TextureDirty;
    d->shareHandle = 0;
    d->eglDisplay = EGL_NO_DISPLAY;
    d->eglConfig = 0;
    d->eglSurface = EGL_NO_SURFACE;
    d->active = false;
    d->blitMode = DirectVideo;
    InitializeCriticalSectionEx(&d->mutex, 0, 0);

    connect(&d->renderThread, &QThread::started,
            this, &QWinRTAbstractVideoRendererControl::syncAndRender,
            Qt::DirectConnection);
}

QWinRTAbstractVideoRendererControl::~QWinRTAbstractVideoRendererControl()
{
    qCDebug(lcMMVideoRender) << __FUNCTION__;
    Q_D(QWinRTAbstractVideoRendererControl);
    CriticalSectionLocker locker(&d->mutex);
    shutdown();
    DeleteCriticalSection(&d->mutex);
    eglDestroySurface(d->eglDisplay, d->eglSurface);
}

QAbstractVideoSurface *QWinRTAbstractVideoRendererControl::surface() const
{
    Q_D(const QWinRTAbstractVideoRendererControl);
    return d->surface;
}

void QWinRTAbstractVideoRendererControl::setSurface(QAbstractVideoSurface *surface)
{
    Q_D(QWinRTAbstractVideoRendererControl);
    d->surface = surface;
}

void QWinRTAbstractVideoRendererControl::syncAndRender()
{
    qCDebug(lcMMVideoRender) << __FUNCTION__;
    Q_D(QWinRTAbstractVideoRendererControl);

    QThread *currentThread = QThread::currentThread();
    const QMetaMethod present = staticMetaObject.method(staticMetaObject.indexOfMethod("present()"));
    forever {
        if (currentThread->isInterruptionRequested())
            break;
        {
            CriticalSectionLocker lock(&d->mutex);
            HRESULT hr;
            if (d->dirtyState == TextureDirty) {
                CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, d->format.frameWidth(), d->format.frameHeight(), 1, 1);
                desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
                desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
                hr = g->device->CreateTexture2D(&desc, NULL, d->texture.ReleaseAndGetAddressOf());
                BREAK_IF_FAILED("Failed to get create video texture");
                ComPtr<IDXGIResource> resource;
                hr = d->texture.As(&resource);
                BREAK_IF_FAILED("Failed to cast texture to resource");
                hr = resource->GetSharedHandle(&d->shareHandle);
                BREAK_IF_FAILED("Failed to get texture share handle");
                d->dirtyState = SurfaceDirty;
            }

            hr = g->output->WaitForVBlank();
            CONTINUE_IF_FAILED("Failed to wait for vertical blank");

            bool success = false;
            switch (d->blitMode) {
            case DirectVideo:
                success = render(d->texture.Get());
                break;
            case MediaFoundation:
                success = dequeueFrame(&d->presentFrame);
                break;
            default:
                success = false;
            }

            if (!success)
                continue;

            // Queue to the control's thread for presentation
            present.invoke(this, Qt::QueuedConnection);
            currentThread->eventDispatcher()->processEvents(QEventLoop::AllEvents);
        }
    }

    // All done, exit render loop
    currentThread->quit();
}

QSize QWinRTAbstractVideoRendererControl::size() const
{
    Q_D(const QWinRTAbstractVideoRendererControl);
    return d->format.frameSize();
}

void QWinRTAbstractVideoRendererControl::setSize(const QSize &size)
{
    Q_D(QWinRTAbstractVideoRendererControl);

    if (d->format.frameSize() == size)
        return;

    d->format.setFrameSize(size);
    d->dirtyState = TextureDirty;
}

void QWinRTAbstractVideoRendererControl::setScanLineDirection(QVideoSurfaceFormat::Direction scanLineDirection)
{
    Q_D(QWinRTAbstractVideoRendererControl);

    if (d->format.scanLineDirection() == scanLineDirection)
        return;

    d->format.setScanLineDirection(scanLineDirection);
}

void QWinRTAbstractVideoRendererControl::setActive(bool active)
{
    qCDebug(lcMMVideoRender) << __FUNCTION__ << active;
    Q_D(QWinRTAbstractVideoRendererControl);

    if (d->active == active)
        return;

    d->active = active;
    if (d->active) {
        if (!d->surface)
            return;

        if (!d->surface->isActive())
            d->surface->start(d->format);

        d->renderThread.start();
        return;
    }

    shutdown();
    if (d->surface && d->surface->isActive())
        d->surface->stop();
}

QWinRTAbstractVideoRendererControl::BlitMode QWinRTAbstractVideoRendererControl::blitMode() const
{
    Q_D(const QWinRTAbstractVideoRendererControl);
    return d->blitMode;
}

void QWinRTAbstractVideoRendererControl::setBlitMode(QWinRTAbstractVideoRendererControl::BlitMode mode)
{
    Q_D(QWinRTAbstractVideoRendererControl);
    CriticalSectionLocker lock(&d->mutex);

    if (d->blitMode == mode)
        return;

    d->blitMode = mode;
    d->dirtyState = d->blitMode == MediaFoundation ? NotDirty : TextureDirty;

    if (d->blitMode == DirectVideo)
        return;

    if (d->texture) {
        d->texture.Reset();
        d->shareHandle = 0;
    }

    if (d->eglSurface) {
        eglDestroySurface(d->eglDisplay, d->eglSurface);
        d->eglSurface = EGL_NO_SURFACE;
    }
}

bool QWinRTAbstractVideoRendererControl::dequeueFrame(QVideoFrame *frame)
{
    Q_UNUSED(frame)
    return false;
}

void QWinRTAbstractVideoRendererControl::textureToFrame()
{
    Q_D(QWinRTAbstractVideoRendererControl);

    if (d->dirtyState == SurfaceDirty) {
        if (!QOpenGLContext::currentContext()) {
            qWarning("A valid OpenGL context is required for binding the video texture.");
            return;
        }

        if (d->eglDisplay == EGL_NO_DISPLAY)
            d->eglDisplay = eglGetCurrentDisplay();

        if (d->eglDisplay == EGL_NO_DISPLAY) {
            qWarning("Failed to get the current EGL display for video presentation: 0x%x", eglGetError());
            return;
        }

        EGLint configAttributes[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_NONE
        };
        EGLint configCount;
        if (!eglChooseConfig(d->eglDisplay, configAttributes, &d->eglConfig, 1, &configCount)) {
            qWarning("Failed to create the texture EGL configuration for video presentation: 0x%x", eglGetError());
            return;
        }

        if (d->eglSurface != EGL_NO_SURFACE)
            eglDestroySurface(d->eglDisplay, d->eglSurface);

        EGLint bufferAttributes[] = {
            EGL_WIDTH, d->format.frameWidth(),
            EGL_HEIGHT, d->format.frameHeight(),
            EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
            EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
            EGL_NONE
        };
        d->eglSurface = eglCreatePbufferFromClientBuffer(d->eglDisplay,
                                                         EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                                         d->shareHandle, d->eglConfig, bufferAttributes);
        if (d->eglSurface == EGL_NO_SURFACE) {
            qWarning("Failed to create the EGL configuration for video presentation: 0x%x", eglGetError());
            return;
        }

        QWinRTVideoBuffer *videoBuffer = new QWinRTVideoBuffer(d->format.frameSize(), QOpenGLTexture::RGBAFormat);
        d->presentFrame = QVideoFrame(videoBuffer, d->format.frameSize(), d->format.pixelFormat());

        // bind the pbuffer surface to the texture
        videoBuffer->bind();
        eglBindTexImage(d->eglDisplay, d->eglSurface, EGL_BACK_BUFFER);
        static_cast<QOpenGLTexture *>(videoBuffer)->release();

        d->dirtyState = NotDirty;
    }
}

void QWinRTAbstractVideoRendererControl::present()
{
    Q_D(QWinRTAbstractVideoRendererControl);
    if (d->blitMode == DirectVideo)
        textureToFrame();

    // Present the frame
    d->surface->present(d->presentFrame);
}

QT_END_NAMESPACE
