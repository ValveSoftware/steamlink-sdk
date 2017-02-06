/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "waylandeglclientbufferintegration.h"

#include <QtWaylandCompositor/QWaylandCompositor>
#include <qpa/qplatformnativeinterface.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLTexture>
#include <qpa/qplatformscreen.h>
#include <QtGui/QWindow>
#include <QtCore/QPointer>
#include <QDebug>

#include <QMutex>
#include <QMutexLocker>
#include <QtCore/private/qcore_unix_p.h>
#include <QtEglSupport/private/qeglstreamconvenience_p.h>

#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES     0x8D65
#endif

#ifndef EGL_WAYLAND_BUFFER_WL
#define EGL_WAYLAND_BUFFER_WL       0x31D5
#endif

#ifndef EGL_WAYLAND_PLANE_WL
#define EGL_WAYLAND_PLANE_WL        0x31D6
#endif

#ifndef EGL_WAYLAND_Y_INVERTED_WL
#define EGL_WAYLAND_Y_INVERTED_WL   0x31DB
#endif

#ifndef EGL_TEXTURE_RGB
#define EGL_TEXTURE_RGB             0x305D
#endif

#ifndef EGL_TEXTURE_RGBA
#define EGL_TEXTURE_RGBA            0x305E
#endif

#ifndef EGL_TEXTURE_EXTERNAL_WL
#define EGL_TEXTURE_EXTERNAL_WL     0x31DA
#endif

#ifndef EGL_TEXTURE_Y_U_V_WL
#define EGL_TEXTURE_Y_U_V_WL        0x31D7
#endif

#ifndef EGL_TEXTURE_Y_UV_WL
#define EGL_TEXTURE_Y_UV_WL         0x31D8
#endif

#ifndef EGL_TEXTURE_Y_XUXV_WL
#define EGL_TEXTURE_Y_XUXV_WL       0x31D9
#endif

#ifndef EGL_PLATFORM_X11_KHR
#define EGL_PLATFORM_X11_KHR        0x31D5
#endif

/* Needed for compatibility with Mesa older than 10.0. */
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL_compat) (EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);

#ifndef EGL_WL_bind_wayland_display
typedef EGLBoolean (EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLUNBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
#endif

#ifndef EGL_KHR_image
typedef EGLImageKHR (EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay dpy, EGLImageKHR image);
#endif

#ifndef GL_OES_EGL_image
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) (GLenum target, GLeglImageOES image);
#endif

QT_BEGIN_NAMESPACE

static const char *
egl_error_string(EGLint code)
{
#define MYERRCODE(x) case x: return #x;
    switch (code) {
    MYERRCODE(EGL_SUCCESS)
    MYERRCODE(EGL_NOT_INITIALIZED)
    MYERRCODE(EGL_BAD_ACCESS)
    MYERRCODE(EGL_BAD_ALLOC)
    MYERRCODE(EGL_BAD_ATTRIBUTE)
    MYERRCODE(EGL_BAD_CONTEXT)
    MYERRCODE(EGL_BAD_CONFIG)
    MYERRCODE(EGL_BAD_CURRENT_SURFACE)
    MYERRCODE(EGL_BAD_DISPLAY)
    MYERRCODE(EGL_BAD_SURFACE)
    MYERRCODE(EGL_BAD_MATCH)
    MYERRCODE(EGL_BAD_PARAMETER)
    MYERRCODE(EGL_BAD_NATIVE_PIXMAP)
    MYERRCODE(EGL_BAD_NATIVE_WINDOW)
    MYERRCODE(EGL_CONTEXT_LOST)
    default:
        return "unknown";
    }
#undef MYERRCODE
}

struct BufferState
{
    BufferState();

    enum EglMode {
        ModeNone,
        ModeEGLImage,
        ModeEGLStream
    };

    EGLint egl_format;
    QVarLengthArray<EGLImageKHR, 3> egl_images;
    QOpenGLTexture *textures[3];
    EGLStreamKHR egl_stream;

    bool isYInverted;
    QSize size;
    EglMode eglMode;
};

class WaylandEglClientBufferIntegrationPrivate
{
public:
    WaylandEglClientBufferIntegrationPrivate();

    void initBuffer(WaylandEglClientBuffer *buffer);
    void init_egl_texture(WaylandEglClientBuffer *buffer, EGLint format);
    void init_egl_fd_texture(WaylandEglClientBuffer *buffer, EGLNativeFileDescriptorKHR streamFd);
    void register_buffer(struct ::wl_resource *buffer, BufferState state);

    EGLDisplay egl_display;
    bool valid;
    bool display_bound;

    PFNEGLBINDWAYLANDDISPLAYWL egl_bind_wayland_display;
    PFNEGLUNBINDWAYLANDDISPLAYWL egl_unbind_wayland_display;
    PFNEGLQUERYWAYLANDBUFFERWL_compat egl_query_wayland_buffer;

    PFNEGLCREATEIMAGEKHRPROC egl_create_image;
    PFNEGLDESTROYIMAGEKHRPROC egl_destroy_image;

    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC gl_egl_image_target_texture_2d;

    QEGLStreamConvenience *funcs;
    static WaylandEglClientBufferIntegrationPrivate *get(WaylandEglClientBufferIntegration *integration) {
        return integration->d_ptr.data();
    }
};

BufferState::BufferState()
    : egl_format(EGL_TEXTURE_RGBA)
    , egl_stream(EGL_NO_STREAM_KHR)
    , isYInverted(true)
    , eglMode(ModeNone)
{}

WaylandEglClientBufferIntegrationPrivate::WaylandEglClientBufferIntegrationPrivate()
    : egl_display(EGL_NO_DISPLAY)
    , valid(false)
    , display_bound(false)
    , egl_bind_wayland_display(0)
    , egl_unbind_wayland_display(0)
    , egl_query_wayland_buffer(0)
    , egl_create_image(0)
    , egl_destroy_image(0)
    , gl_egl_image_target_texture_2d(0)
    , funcs(Q_NULLPTR)
{
}

void WaylandEglClientBufferIntegrationPrivate::initBuffer(WaylandEglClientBuffer *buffer)
{
    EGLint format;

    if (egl_query_wayland_buffer(egl_display, buffer->waylandBufferHandle(), EGL_TEXTURE_FORMAT, &format))
        init_egl_texture(buffer, format);
}

void WaylandEglClientBufferIntegrationPrivate::init_egl_texture(WaylandEglClientBuffer *buffer, EGLint format)
{
// Non-streaming case

    // Resolving GL functions may need a context current, so do it only here.
    if (!gl_egl_image_target_texture_2d)
        gl_egl_image_target_texture_2d = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    if (!gl_egl_image_target_texture_2d) {
        qWarning("QtCompositor: bindTextureToBuffer() failed. Could not find glEGLImageTargetTexture2DOES.");
        return;
    }

    BufferState &state = *buffer->d;
    state.egl_format = format;
    state.eglMode = BufferState::ModeEGLImage;

#if defined(EGL_WAYLAND_Y_INVERTED_WL)
    EGLint isYInverted;
    EGLBoolean ret = egl_query_wayland_buffer(egl_display, buffer->waylandBufferHandle(), EGL_WAYLAND_Y_INVERTED_WL, &isYInverted);
    // Yes, this looks strange, but the specification says that EGL_FALSE return
    // value (not supported) should be treated the same as EGL_TRUE return value
    // and EGL_TRUE in value.
    state.isYInverted = (ret == EGL_FALSE || isYInverted == EGL_TRUE);
#endif

    int planes = 1;

    switch (format) {
    default:
    case EGL_TEXTURE_RGB:
    case EGL_TEXTURE_RGBA:
    case EGL_TEXTURE_EXTERNAL_WL:
        planes = 1;
        break;
    case EGL_TEXTURE_Y_UV_WL:
        planes = 2;
        break;
    case EGL_TEXTURE_Y_U_V_WL:
        planes = 3;
        break;
    case EGL_TEXTURE_Y_XUXV_WL:
        planes = 2;
        break;
    }

    for (int i = 0; i < planes; i++) {
        const EGLint attribs[] = { EGL_WAYLAND_PLANE_WL, i, EGL_NONE };
        EGLImageKHR image = egl_create_image(egl_display,
                                             EGL_NO_CONTEXT,
                                             EGL_WAYLAND_BUFFER_WL,
                                             buffer->waylandBufferHandle(),
                                             attribs);

        if (image == EGL_NO_IMAGE_KHR)
            qWarning("failed to create EGL image for plane %d", i);

        state.egl_images << image;
        state.textures[i] = nullptr;
    }
}

void WaylandEglClientBufferIntegrationPrivate::init_egl_fd_texture(WaylandEglClientBuffer *buffer, EGLNativeFileDescriptorKHR streamFd)
{
//EglStreams case
    BufferState &state = *buffer->d;

    state.egl_format = EGL_TEXTURE_EXTERNAL_WL;
    state.isYInverted = false;
    state.egl_stream = funcs->create_stream_from_file_descriptor(egl_display, streamFd);

    close(streamFd);

    if (state.egl_stream == EGL_NO_STREAM_KHR) {
        qWarning("%s:%d: eglCreateStreamFromFileDescriptorKHR failed: 0x%x", Q_FUNC_INFO, __LINE__, eglGetError());
        return;
    }
    state.eglMode = BufferState::ModeEGLStream;

    if (!QOpenGLContext::currentContext())
        qWarning("EglClientBufferIntegration: creating texture with no current context");

    //TODO This texture might end up in a different context than the quick item which wants to use it, this needs to be fixed somehow.

    auto texture = new QOpenGLTexture(static_cast<QOpenGLTexture::Target>(GL_TEXTURE_EXTERNAL_OES));
    texture->create();
    state.textures[0] = texture; // TODO: support multiple planes for the streaming case

    glActiveTexture(GL_TEXTURE0);
    texture->create();
    texture->bind();
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture->textureId());

    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    auto newStream = funcs->stream_consumer_gltexture(egl_display, state.egl_stream);

    if (!newStream) {
        EGLint code = eglGetError();
        qWarning() << "Could not initialize EGLStream:" << egl_error_string(code) << hex << (long)code;
        funcs->destroy_stream(egl_display, state.egl_stream);
        state.egl_stream = EGL_NO_STREAM_KHR;
    }
}

WaylandEglClientBufferIntegration::WaylandEglClientBufferIntegration()
    : QtWayland::ClientBufferIntegration()
    , d_ptr(new WaylandEglClientBufferIntegrationPrivate)
{
}

void WaylandEglClientBufferIntegration::initializeHardware(struct wl_display *display)
{
    Q_D(WaylandEglClientBufferIntegration);

    const bool ignoreBindDisplay = !qgetenv("QT_WAYLAND_IGNORE_BIND_DISPLAY").isEmpty();

    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    if (!nativeInterface) {
        qWarning("QtCompositor: Failed to initialize EGL display. No native platform interface available.");
        return;
    }

    d->egl_display = nativeInterface->nativeResourceForIntegration("EglDisplay");
    if (!d->egl_display) {
        qWarning("QtCompositor: Failed to initialize EGL display. Could not get EglDisplay for window.");
        return;
    }

    const char *extensionString = eglQueryString(d->egl_display, EGL_EXTENSIONS);
    if ((!extensionString || !strstr(extensionString, "EGL_WL_bind_wayland_display")) && !ignoreBindDisplay) {
        qWarning("QtCompositor: Failed to initialize EGL display. There is no EGL_WL_bind_wayland_display extension.");
        return;
    }

    d->egl_bind_wayland_display = reinterpret_cast<PFNEGLBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglBindWaylandDisplayWL"));
    d->egl_unbind_wayland_display = reinterpret_cast<PFNEGLUNBINDWAYLANDDISPLAYWL>(eglGetProcAddress("eglUnbindWaylandDisplayWL"));
    if ((!d->egl_bind_wayland_display || !d->egl_unbind_wayland_display) && !ignoreBindDisplay) {
        qWarning("QtCompositor: Failed to initialize EGL display. Could not find eglBindWaylandDisplayWL and eglUnbindWaylandDisplayWL.");
        return;
    }

    d->egl_query_wayland_buffer = reinterpret_cast<PFNEGLQUERYWAYLANDBUFFERWL_compat>(eglGetProcAddress("eglQueryWaylandBufferWL"));
    if (!d->egl_query_wayland_buffer) {
        qWarning("QtCompositor: Failed to initialize EGL display. Could not find eglQueryWaylandBufferWL.");
        return;
    }

    d->egl_create_image = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    d->egl_destroy_image = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    if (!d->egl_create_image || !d->egl_destroy_image) {
        qWarning("QtCompositor: Failed to initialize EGL display. Could not find eglCreateImageKHR and eglDestroyImageKHR.");
        return;
    }

    if (d->egl_bind_wayland_display && d->egl_unbind_wayland_display) {
        d->display_bound = d->egl_bind_wayland_display(d->egl_display, display);
        if (!d->display_bound) {
            if (!ignoreBindDisplay) {
                qWarning("QtCompositor: Failed to initialize EGL display. Could not bind Wayland display.");
                return;
            } else {
                qWarning("QtCompositor: Could not bind Wayland display. Ignoring.");
            }
        }
    }

    d->funcs = new QEGLStreamConvenience;
    d->funcs->initialize(d->egl_display);

    d->valid = true;
}

QtWayland::ClientBuffer *WaylandEglClientBufferIntegration::createBufferFor(wl_resource *buffer)
{
    if (wl_shm_buffer_get(buffer))
        return nullptr;
    return new WaylandEglClientBuffer(this, buffer);
}

WaylandEglClientBuffer::WaylandEglClientBuffer(WaylandEglClientBufferIntegration *integration, wl_resource *buffer)
    : ClientBuffer(buffer)
    , m_integration(integration)
{
    auto *p = WaylandEglClientBufferIntegrationPrivate::get(m_integration);
    d = new BufferState;
    if (buffer && !wl_shm_buffer_get(buffer)) {
        EGLint width, height;
        p->egl_query_wayland_buffer(p->egl_display, buffer, EGL_WIDTH, &width);
        p->egl_query_wayland_buffer(p->egl_display, buffer, EGL_HEIGHT, &height);
        d->size = QSize(width, height);

        p->initBuffer(this);
    }
}

static QWaylandBufferRef::BufferFormatEgl formatFromEglFormat(EGLint format) {
    switch (format) {
    case EGL_TEXTURE_RGB:
        return QWaylandBufferRef::BufferFormatEgl_RGB;
    case EGL_TEXTURE_RGBA:
        return QWaylandBufferRef::BufferFormatEgl_RGBA;
    case EGL_TEXTURE_EXTERNAL_WL:
        return QWaylandBufferRef::BufferFormatEgl_EXTERNAL_OES;
    case EGL_TEXTURE_Y_UV_WL:
        return QWaylandBufferRef::BufferFormatEgl_Y_UV;
    case EGL_TEXTURE_Y_U_V_WL:
        return QWaylandBufferRef::BufferFormatEgl_Y_U_V;
    case EGL_TEXTURE_Y_XUXV_WL:
        return QWaylandBufferRef::BufferFormatEgl_Y_XUXV;
    }

    return QWaylandBufferRef::BufferFormatEgl_RGBA;
}

QWaylandBufferRef::BufferFormatEgl WaylandEglClientBuffer::bufferFormatEgl() const
{
    return formatFromEglFormat(d->egl_format);
}

QOpenGLTexture *WaylandEglClientBuffer::toOpenGlTexture(int plane)
{
    if (!m_buffer)
        return nullptr;

    auto *p = WaylandEglClientBufferIntegrationPrivate::get(m_integration);
    auto texture = d->textures[plane];
    const auto target = static_cast<QOpenGLTexture::Target>((d->eglMode == BufferState::ModeEGLStream || d->egl_format == EGL_TEXTURE_EXTERNAL_WL) ? GL_TEXTURE_EXTERNAL_OES
                                                                        : GL_TEXTURE_2D);
    if (!texture) {
        texture = new QOpenGLTexture(target);
        texture->create();
        d->textures[plane] = texture;
    }


    if (d->eglMode == BufferState::ModeEGLStream) {
        // EGLStream requires calling acquire on every frame.
        if (d->egl_stream != EGL_NO_STREAM_KHR) {

            texture->bind();

            EGLint stream_state;
            p->funcs->query_stream(p->egl_display, d->egl_stream, EGL_STREAM_STATE_KHR, &stream_state);

            if (stream_state == EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR) {
                if (p->funcs->stream_consumer_acquire(p->egl_display, d->egl_stream) != EGL_TRUE)
                    qWarning("%s:%d: eglStreamConsumerAcquireKHR failed: 0x%x", Q_FUNC_INFO, __LINE__, eglGetError());
            }
        }
    } else if (m_textureDirty) {
        texture->bind();
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        p->gl_egl_image_target_texture_2d(target, d->egl_images[plane]);
    }
    return texture;
}

void WaylandEglClientBuffer::setCommitted(QRegion &damage)
{
    ClientBuffer::setCommitted(damage);
    if (d->eglMode == BufferState::ModeNone) {
        EGLNativeFileDescriptorKHR streamFd = EGL_NO_FILE_DESCRIPTOR_KHR;
        auto *p = WaylandEglClientBufferIntegrationPrivate::get(m_integration);
        if (p->egl_query_wayland_buffer(p->egl_display, waylandBufferHandle(), EGL_WAYLAND_BUFFER_WL, &streamFd))
                p->init_egl_fd_texture(this, streamFd);
    }
}

QWaylandSurface::Origin WaylandEglClientBuffer::origin() const
{
    return d->isYInverted ? QWaylandSurface::OriginTopLeft : QWaylandSurface::OriginBottomLeft;
}

quintptr WaylandEglClientBuffer::lockNativeBuffer()
{
    auto *p = WaylandEglClientBufferIntegrationPrivate::get(m_integration);

    if (d->egl_stream != EGL_NO_STREAM_KHR)
        return 0;

    EGLImageKHR image = p->egl_create_image(p->egl_display, EGL_NO_CONTEXT,
                                          EGL_WAYLAND_BUFFER_WL,
                                          m_buffer, NULL);
    return reinterpret_cast<quintptr>(image);
}

void WaylandEglClientBuffer::unlockNativeBuffer(quintptr native_buffer) const
{
    if (!native_buffer)
        return;

    auto *p = WaylandEglClientBufferIntegrationPrivate::get(m_integration);

    EGLImageKHR image = reinterpret_cast<EGLImageKHR>(native_buffer);
    p->egl_destroy_image(p->egl_display, image);
}

QSize WaylandEglClientBuffer::size() const
{
    return d->size;
}

QT_END_NAMESPACE
