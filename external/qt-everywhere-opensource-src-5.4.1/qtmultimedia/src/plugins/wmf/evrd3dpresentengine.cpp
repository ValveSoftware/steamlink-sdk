/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "evrd3dpresentengine.h"

#include "mfglobal.h"

#include <qtgui/qguiapplication.h>
#include <qpa/qplatformnativeinterface.h>
#include <qtgui/qopenglcontext.h>
#include <qabstractvideobuffer.h>
#include <QAbstractVideoSurface>
#include <qvideoframe.h>
#include <QDebug>
#include <qopenglcontext.h>
#include <qopenglfunctions.h>
#include <qwindow.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <WinUser.h>
#include <evr.h>

QT_USE_NAMESPACE

static const DWORD PRESENTER_BUFFER_COUNT = 3;

class TextureVideoBuffer : public QAbstractVideoBuffer
{
public:
    TextureVideoBuffer(GLuint textureId)
        : QAbstractVideoBuffer(GLTextureHandle)
        , m_textureId(textureId)
    {}

    ~TextureVideoBuffer() {}

    MapMode mapMode() const { return NotMapped; }
    uchar *map(MapMode, int*, int*) { return 0; }
    void unmap() {}

    QVariant handle() const
    {
        return QVariant::fromValue<unsigned int>(m_textureId);
    }

private:
    GLuint m_textureId;
};

EGLWrapper::EGLWrapper()
{
#ifndef QT_OPENGL_ES_2_ANGLE_STATIC
    // Resolve the EGL functions we use. When configured for dynamic OpenGL, no
    // component in Qt will link to libEGL.lib and libGLESv2.lib. We know
    // however that libEGL is loaded for sure, since this is an ANGLE-only path.

# ifdef QT_DEBUG
    HMODULE eglHandle = GetModuleHandle(L"libEGLd.dll");
# else
    HMODULE eglHandle = GetModuleHandle(L"libEGL.dll");
# endif

    if (!eglHandle)
        qWarning("No EGL library loaded");

    m_eglGetProcAddress = (EglGetProcAddress) GetProcAddress(eglHandle, "eglGetProcAddress");
    m_eglCreatePbufferSurface = (EglCreatePbufferSurface) GetProcAddress(eglHandle, "eglCreatePbufferSurface");
    m_eglDestroySurface = (EglDestroySurface) GetProcAddress(eglHandle, "eglDestroySurface");
    m_eglBindTexImage = (EglBindTexImage) GetProcAddress(eglHandle, "eglBindTexImage");
    m_eglReleaseTexImage = (EglReleaseTexImage) GetProcAddress(eglHandle, "eglReleaseTexImage");
#else
    // Static ANGLE-only build. There is no libEGL.dll in use.

    m_eglGetProcAddress = ::eglGetProcAddress;
    m_eglCreatePbufferSurface = ::eglCreatePbufferSurface;
    m_eglDestroySurface = ::eglDestroySurface;
    m_eglBindTexImage = ::eglBindTexImage;
    m_eglReleaseTexImage = ::eglReleaseTexImage;
#endif
}

__eglMustCastToProperFunctionPointerType EGLWrapper::getProcAddress(const char *procname)
{
    Q_ASSERT(m_eglGetProcAddress);
    return m_eglGetProcAddress(procname);
}

EGLSurface EGLWrapper::createPbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
    Q_ASSERT(m_eglCreatePbufferSurface);
    return m_eglCreatePbufferSurface(dpy, config, attrib_list);
}

EGLBoolean EGLWrapper::destroySurface(EGLDisplay dpy, EGLSurface surface)
{
    Q_ASSERT(m_eglDestroySurface);
    return m_eglDestroySurface(dpy, surface);
}

EGLBoolean EGLWrapper::bindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    Q_ASSERT(m_eglBindTexImage);
    return m_eglBindTexImage(dpy, surface, buffer);
}

EGLBoolean EGLWrapper::releaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    Q_ASSERT(m_eglReleaseTexImage);
    return m_eglReleaseTexImage(dpy, surface, buffer);
}

D3DPresentEngine::D3DPresentEngine()
    : QObject()
    , m_mutex(QMutex::Recursive)
    , m_deviceResetToken(0)
    , m_D3D9(0)
    , m_device(0)
    , m_deviceManager(0)
    , m_surface(0)
    , m_glContext(0)
    , m_offscreenSurface(0)
    , m_eglDisplay(0)
    , m_eglConfig(0)
    , m_eglSurface(0)
    , m_glTexture(0)
    , m_texture(0)
    , m_egl(0)
{
    ZeroMemory(&m_displayMode, sizeof(m_displayMode));

    HRESULT hr = initializeD3D();

    if (SUCCEEDED(hr)) {
       hr = createD3DDevice();
       if (FAILED(hr))
           qWarning("Failed to create D3D device");
    } else {
        qWarning("Failed to initialize D3D");
    }
}

D3DPresentEngine::~D3DPresentEngine()
{
    qt_wmf_safeRelease(&m_texture);
    qt_wmf_safeRelease(&m_device);
    qt_wmf_safeRelease(&m_deviceManager);
    qt_wmf_safeRelease(&m_D3D9);

    if (m_eglSurface) {
        m_egl->releaseTexImage(m_eglDisplay, m_eglSurface, EGL_BACK_BUFFER);
        m_egl->destroySurface(m_eglDisplay, m_eglSurface);
        m_eglSurface = NULL;
    }
    if (m_glTexture) {
        if (QOpenGLContext *current = QOpenGLContext::currentContext())
            current->functions()->glDeleteTextures(1, &m_glTexture);
        else
            qWarning() << "D3DPresentEngine: Cannot obtain GL context, unable to delete textures";
    }

    delete m_glContext;
    delete m_offscreenSurface;
    delete m_egl;
}

void D3DPresentEngine::start()
{
    QMutexLocker locker(&m_mutex);

    if (!m_surfaceFormat.isValid())
        return;

    if (!m_texture)
        createOffscreenTexture();

    if (m_surface && !m_surface->isActive())
        m_surface->start(m_surfaceFormat);
}

void D3DPresentEngine::stop()
{
    QMutexLocker locker(&m_mutex);
    if (m_surface && m_surface->isActive())
        m_surface->stop();
}

HRESULT D3DPresentEngine::getService(REFGUID, REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (riid == __uuidof(IDirect3DDeviceManager9)) {
        if (m_deviceManager == NULL) {
            hr = MF_E_UNSUPPORTED_SERVICE;
        } else {
            *ppv = m_deviceManager;
            m_deviceManager->AddRef();
        }
    } else {
        hr = MF_E_UNSUPPORTED_SERVICE;
    }

    return hr;
}

HRESULT D3DPresentEngine::checkFormat(D3DFORMAT format)
{
    HRESULT hr = S_OK;

    UINT uAdapter = D3DADAPTER_DEFAULT;
    D3DDEVTYPE type = D3DDEVTYPE_HAL;

    D3DDISPLAYMODE mode;
    D3DDEVICE_CREATION_PARAMETERS params;

    // Our shared D3D/EGL surface only supports RGB32,
    // reject all other formats
    if (format != D3DFMT_X8R8G8B8)
        return MF_E_INVALIDMEDIATYPE;

    if (m_device) {
        hr = m_device->GetCreationParameters(&params);
        if (FAILED(hr))
            return hr;

        uAdapter = params.AdapterOrdinal;
        type = params.DeviceType;
    }

    hr = m_D3D9->GetAdapterDisplayMode(uAdapter, &mode);
    if (FAILED(hr))
        return hr;

    return m_D3D9->CheckDeviceType(uAdapter, type, mode.Format, format, TRUE);
}

HRESULT D3DPresentEngine::createVideoSamples(IMFMediaType *format, QList<IMFSample*> &videoSampleQueue)
{
    if (!format)
        return MF_E_UNEXPECTED;

    HRESULT hr = S_OK;
    D3DPRESENT_PARAMETERS pp;

    IDirect3DSwapChain9 *swapChain = NULL;
    IMFSample *videoSample = NULL;

    QMutexLocker locker(&m_mutex);

    releaseResources();

    // Get the swap chain parameters from the media type.
    hr = getSwapChainPresentParameters(format, &pp);
    if (FAILED(hr))
        goto done;

    // Create the video samples.
    for (int i = 0; i < PRESENTER_BUFFER_COUNT; i++) {
        // Create a new swap chain.
        hr = m_device->CreateAdditionalSwapChain(&pp, &swapChain);
        if (FAILED(hr))
            goto done;

        // Create the video sample from the swap chain.
        hr = createD3DSample(swapChain, &videoSample);
        if (FAILED(hr))
            goto done;

        // Add it to the list.
        videoSample->AddRef();
        videoSampleQueue.append(videoSample);

        // Set the swap chain pointer as a custom attribute on the sample. This keeps
        // a reference count on the swap chain, so that the swap chain is kept alive
        // for the duration of the sample's lifetime.
        hr = videoSample->SetUnknown(MFSamplePresenter_SampleSwapChain, swapChain);
        if (FAILED(hr))
            goto done;

        qt_wmf_safeRelease(&videoSample);
        qt_wmf_safeRelease(&swapChain);
    }

done:
    if (FAILED(hr))
        releaseResources();

    qt_wmf_safeRelease(&swapChain);
    qt_wmf_safeRelease(&videoSample);
    return hr;
}

void D3DPresentEngine::releaseResources()
{
}

void D3DPresentEngine::presentSample(void *opaque, qint64)
{
    HRESULT hr = S_OK;

    IMFSample *sample = reinterpret_cast<IMFSample*>(opaque);
    IMFMediaBuffer* buffer = NULL;
    IDirect3DSurface9* surface = NULL;

    if (m_surface && m_surface->isActive()) {
        if (sample) {
            // Get the buffer from the sample.
            hr = sample->GetBufferByIndex(0, &buffer);
            if (FAILED(hr))
                goto done;

            // Get the surface from the buffer.
            hr = MFGetService(buffer, MR_BUFFER_SERVICE, IID_PPV_ARGS(&surface));
            if (FAILED(hr))
                goto done;
        }

        if (surface && updateTexture(surface)) {
            QVideoFrame frame = QVideoFrame(new TextureVideoBuffer(m_glTexture),
                                            m_surfaceFormat.frameSize(),
                                            m_surfaceFormat.pixelFormat());

            // WMF uses 100-nanosecond units, Qt uses microseconds
            LONGLONG startTime = -1;
            if (SUCCEEDED(sample->GetSampleTime(&startTime))) {
                frame.setStartTime(startTime * 0.1);

                LONGLONG duration = -1;
                if (SUCCEEDED(sample->GetSampleDuration(&duration)))
                    frame.setEndTime((startTime + duration) * 0.1);
            }

            m_surface->present(frame);
        }
    }

done:
    qt_wmf_safeRelease(&surface);
    qt_wmf_safeRelease(&buffer);
    qt_wmf_safeRelease(&sample);
}

void D3DPresentEngine::setSurface(QAbstractVideoSurface *surface)
{
    QMutexLocker locker(&m_mutex);
    m_surface = surface;
}

void D3DPresentEngine::setSurfaceFormat(const QVideoSurfaceFormat &format)
{
    QMutexLocker locker(&m_mutex);
    m_surfaceFormat = format;
}

void D3DPresentEngine::createOffscreenTexture()
{
    // First, check if we have a context on this thread
    QOpenGLContext *currentContext = QOpenGLContext::currentContext();

    if (!currentContext) {
        //Create OpenGL context and set share context from surface
        QOpenGLContext *shareContext = qobject_cast<QOpenGLContext*>(m_surface->property("GLContext").value<QObject*>());
        if (!shareContext)
            return;

        m_offscreenSurface = new QWindow;
        m_offscreenSurface->setSurfaceType(QWindow::OpenGLSurface);
        //Needs geometry to be a valid surface, but size is not important
        m_offscreenSurface->setGeometry(-1, -1, 1, 1);
        m_offscreenSurface->create();

        m_glContext = new QOpenGLContext;
        m_glContext->setFormat(m_offscreenSurface->requestedFormat());
        m_glContext->setShareContext(shareContext);

        if (!m_glContext->create()) {
            delete m_glContext;
            delete m_offscreenSurface;
            m_glContext = 0;
            m_offscreenSurface = 0;
            return;
        }

        currentContext = m_glContext;
    }

    if (m_glContext)
        m_glContext->makeCurrent(m_offscreenSurface);

    if (!m_egl)
        m_egl = new EGLWrapper;

    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    m_eglDisplay = static_cast<EGLDisplay*>(
                nativeInterface->nativeResourceForContext("eglDisplay", currentContext));
    m_eglConfig = static_cast<EGLConfig*>(
                nativeInterface->nativeResourceForContext("eglConfig", currentContext));

    currentContext->functions()->glGenTextures(1, &m_glTexture);

    int w = m_surfaceFormat.frameWidth();
    int h = m_surfaceFormat.frameHeight();
    bool hasAlpha = currentContext->format().hasAlpha();

    EGLint attribs[] = {
        EGL_WIDTH, w,
        EGL_HEIGHT, h,
        EGL_TEXTURE_FORMAT, hasAlpha ? EGL_TEXTURE_RGBA : EGL_TEXTURE_RGB,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };

    EGLSurface pbuffer = m_egl->createPbufferSurface(m_eglDisplay, m_eglConfig, attribs);

    HANDLE share_handle = 0;
    PFNEGLQUERYSURFACEPOINTERANGLEPROC eglQuerySurfacePointerANGLE =
            reinterpret_cast<PFNEGLQUERYSURFACEPOINTERANGLEPROC>(m_egl->getProcAddress("eglQuerySurfacePointerANGLE"));
    Q_ASSERT(eglQuerySurfacePointerANGLE);
    eglQuerySurfacePointerANGLE(
                m_eglDisplay,
                pbuffer,
                EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, &share_handle);


    m_device->CreateTexture(w, h, 1,
                            D3DUSAGE_RENDERTARGET,
                            hasAlpha ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8,
                            D3DPOOL_DEFAULT,
                            &m_texture,
                            &share_handle);

    m_eglSurface = pbuffer;

    if (m_glContext)
        m_glContext->doneCurrent();
}

bool D3DPresentEngine::updateTexture(IDirect3DSurface9 *src)
{
    if (!m_texture)
        return false;

    if (m_glContext)
        m_glContext->makeCurrent(m_offscreenSurface);

    QOpenGLContext::currentContext()->functions()->glBindTexture(GL_TEXTURE_2D, m_glTexture);

    IDirect3DSurface9 *dest = NULL;

    // Copy the sample surface to the shared D3D/EGL surface
    HRESULT hr = m_texture->GetSurfaceLevel(0, &dest);
    if (FAILED(hr))
        goto done;

    hr = m_device->StretchRect(src, NULL, dest, NULL, D3DTEXF_NONE);
    if (FAILED(hr))
        qWarning("Failed to copy D3D surface");

    if (hr == S_OK)
        m_egl->bindTexImage(m_eglDisplay, m_eglSurface, EGL_BACK_BUFFER);

done:
    qt_wmf_safeRelease(&dest);

    if (m_glContext)
        m_glContext->doneCurrent();

    return SUCCEEDED(hr);
}

HRESULT D3DPresentEngine::initializeD3D()
{
    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &m_D3D9);

    if (SUCCEEDED(hr))
        hr = DXVA2CreateDirect3DDeviceManager9(&m_deviceResetToken, &m_deviceManager);

    return hr;
}

HRESULT D3DPresentEngine::createD3DDevice()
{
    HRESULT hr = S_OK;
    HWND hwnd = NULL;
    UINT uAdapterID = D3DADAPTER_DEFAULT;
    DWORD vp = 0;

    D3DCAPS9 ddCaps;
    ZeroMemory(&ddCaps, sizeof(ddCaps));

    IDirect3DDevice9Ex* device = NULL;

    // Hold the lock because we might be discarding an existing device.
    QMutexLocker locker(&m_mutex);

    if (!m_D3D9 || !m_deviceManager)
        return MF_E_NOT_INITIALIZED;

    hwnd = ::GetShellWindow();

    // Note: The presenter creates additional swap chains to present the
    // video frames. Therefore, it does not use the device's implicit
    // swap chain, so the size of the back buffer here is 1 x 1.

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));

    pp.BackBufferWidth = 1;
    pp.BackBufferHeight = 1;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferCount = 1;
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.hDeviceWindow = hwnd;
    pp.Flags = D3DPRESENTFLAG_VIDEO;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    hr = m_D3D9->GetDeviceCaps(uAdapterID, D3DDEVTYPE_HAL, &ddCaps);
    if (FAILED(hr))
        goto done;

    if (ddCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
        vp = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    else
        vp = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    hr = m_D3D9->CreateDeviceEx(
                uAdapterID,
                D3DDEVTYPE_HAL,
                pp.hDeviceWindow,
                vp | D3DCREATE_NOWINDOWCHANGES | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
                &pp,
                NULL,
                &device
                );
    if (FAILED(hr))
        goto done;

    hr = m_D3D9->GetAdapterDisplayMode(uAdapterID, &m_displayMode);
    if (FAILED(hr))
        goto done;

    hr = m_deviceManager->ResetDevice(device, m_deviceResetToken);
    if (FAILED(hr))
        goto done;

    qt_wmf_safeRelease(&m_device);

    m_device = device;
    m_device->AddRef();

done:
    qt_wmf_safeRelease(&device);
    return hr;
}

HRESULT D3DPresentEngine::createD3DSample(IDirect3DSwapChain9 *swapChain, IMFSample **videoSample)
{
    D3DCOLOR clrBlack = D3DCOLOR_ARGB(0xFF, 0x00, 0x00, 0x00);

    IDirect3DSurface9* surface = NULL;
    IMFSample* sample = NULL;

    // Get the back buffer surface.
    HRESULT hr = swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &surface);
    if (FAILED(hr))
        goto done;

    // Fill it with black.
    hr = m_device->ColorFill(surface, NULL, clrBlack);
    if (FAILED(hr))
        goto done;

    hr = MFCreateVideoSampleFromSurface(surface, &sample);
    if (FAILED(hr))
        goto done;

    *videoSample = sample;
    (*videoSample)->AddRef();

done:
    qt_wmf_safeRelease(&surface);
    qt_wmf_safeRelease(&sample);
    return hr;
}

HRESULT D3DPresentEngine::getSwapChainPresentParameters(IMFMediaType *type, D3DPRESENT_PARAMETERS* pp)
{
    ZeroMemory(pp, sizeof(D3DPRESENT_PARAMETERS));

    // Get some information about the video format.

    UINT32 width = 0, height = 0;

    HRESULT hr = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr))
        return hr;

    DWORD d3dFormat = 0;

    hr = qt_wmf_getFourCC(type, &d3dFormat);
    if (FAILED(hr))
        return hr;

    ZeroMemory(pp, sizeof(D3DPRESENT_PARAMETERS));
    pp->BackBufferWidth = width;
    pp->BackBufferHeight = height;
    pp->Windowed = TRUE;
    pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp->BackBufferFormat = (D3DFORMAT)d3dFormat;
    pp->hDeviceWindow = ::GetShellWindow();
    pp->Flags = D3DPRESENTFLAG_VIDEO;
    pp->PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    D3DDEVICE_CREATION_PARAMETERS params;
    hr = m_device->GetCreationParameters(&params);
    if (FAILED(hr))
        return hr;

    if (params.DeviceType != D3DDEVTYPE_HAL)
        pp->Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

    return S_OK;
}
