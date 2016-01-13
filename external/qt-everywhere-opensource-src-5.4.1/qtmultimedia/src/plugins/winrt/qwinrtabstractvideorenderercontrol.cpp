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

#include "qwinrtabstractvideorenderercontrol.h"

#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QGlobalStatic>
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

QT_USE_NAMESPACE

#define BREAK_IF_FAILED(msg) RETURN_IF_FAILED(msg, break)
#define CONTINUE_IF_FAILED(msg) RETURN_IF_FAILED(msg, continue)

// Global D3D device to be shared between video surfaces
struct QWinRTVideoRendererControlGlobal
{
    QWinRTVideoRendererControlGlobal()
    {
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
    QWinRTVideoBuffer()
        : QAbstractVideoBuffer(QAbstractVideoBuffer::GLTextureHandle)
        , QOpenGLTexture(QOpenGLTexture::Target2D)
    {
    }

    void addRef()
    {
        refCount.ref();
    }

    void release() Q_DECL_OVERRIDE
    {
        if (!refCount.deref())
            delete this;
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

private:
    QAtomicInt refCount;
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

    QWinRTVideoBuffer *videoBuffer;

    QThread renderThread;
    bool active;
};

ID3D11Device *QWinRTAbstractVideoRendererControl::d3dDevice()
{
    return g->device.Get();
}

// This is required so that subclasses can stop the render thread before deletion
void QWinRTAbstractVideoRendererControl::shutdown()
{
    Q_D(QWinRTAbstractVideoRendererControl);
    if (d->renderThread.isRunning()) {
        d->renderThread.requestInterruption();
        d->renderThread.wait();
    }
}

QWinRTAbstractVideoRendererControl::QWinRTAbstractVideoRendererControl(const QSize &size, QObject *parent)
    : QVideoRendererControl(parent), d_ptr(new QWinRTAbstractVideoRendererControlPrivate)
{
    Q_D(QWinRTAbstractVideoRendererControl);

    d->format = QVideoSurfaceFormat(size, QVideoFrame::Format_BGRA32,
                                    QAbstractVideoBuffer::GLTextureHandle);
    d->dirtyState = TextureDirty;
    d->shareHandle = 0;
    d->eglDisplay = EGL_NO_DISPLAY;
    d->eglConfig = 0;
    d->eglSurface = EGL_NO_SURFACE;
    d->active = false;

    d->videoBuffer = new QWinRTVideoBuffer;

    connect(&d->renderThread, &QThread::started,
            this, &QWinRTAbstractVideoRendererControl::syncAndRender,
            Qt::DirectConnection);
}

QWinRTAbstractVideoRendererControl::~QWinRTAbstractVideoRendererControl()
{
    shutdown();
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
    Q_D(QWinRTAbstractVideoRendererControl);

    QThread *currentThread = QThread::currentThread();
    const QMetaMethod present = staticMetaObject.method(staticMetaObject.indexOfMethod("present()"));
    forever {
        if (currentThread->isInterruptionRequested())
            break;

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

        if (!render(d->texture.Get()))
            continue;

        // Queue to the control's thread for presentation
        present.invoke(this, Qt::QueuedConnection);
        currentThread->eventDispatcher()->processEvents(QEventLoop::AllEvents);
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

    d->renderThread.requestInterruption();
    if (d->surface && d->surface->isActive())
        d->surface->stop();
}

void QWinRTAbstractVideoRendererControl::present()
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

        d->videoBuffer->setFormat(QOpenGLTexture::RGBAFormat);
        d->videoBuffer->setSize(d->format.frameWidth(), d->format.frameHeight());
        if (!d->videoBuffer->isCreated())
            d->videoBuffer->create();

        // bind the pbuffer surface to the texture
        d->videoBuffer->bind();
        eglBindTexImage(d->eglDisplay, d->eglSurface, EGL_BACK_BUFFER);
        static_cast<QOpenGLTexture *>(d->videoBuffer)->release();

        d->dirtyState = NotDirty;
    }

    // Present the frame
    d->videoBuffer->addRef();
    QVideoFrame frame(d->videoBuffer, d->format.frameSize(), d->format.pixelFormat());
    d->surface->present(frame);
}
