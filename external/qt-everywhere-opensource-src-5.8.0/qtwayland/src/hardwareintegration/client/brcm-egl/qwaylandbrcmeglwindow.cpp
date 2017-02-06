/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwaylandbrcmeglwindow.h"

#include <QtWaylandClient/private/qwaylandbuffer_p.h>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include "qwaylandbrcmglcontext.h"

#include <QtEglSupport/private/qeglconvenience_p.h>

#include <QtGui/QWindow>
#include <qpa/qwindowsysteminterface.h>

#include <EGL/eglext_brcm.h>

#include <wayland-client.h>
#include "wayland-brcm-client-protocol.h"

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class QWaylandBrcmBuffer : public QWaylandBuffer
{
public:
    QWaylandBrcmBuffer(QWaylandDisplay *display,
                       struct qt_brcm *brcm,
                       const QSize &size,
                       EGLint *data,
                       int count,
                       struct wl_event_queue *eventQueue)
        : m_size(size)
        , m_released(true)
        , m_display(display)
        , m_eventQueue(eventQueue)
    {
        wl_array_init(&m_array);
        m_data = static_cast<EGLint *>(wl_array_add(&m_array, count * sizeof(EGLint)));

        for (int i = 0; i < count; ++i)
            m_data[i] = data[i];

        mBuffer = qt_brcm_create_buffer(brcm, size.width(), size.height(), &m_array);
        wl_proxy_set_queue(reinterpret_cast<struct wl_proxy*>(mBuffer), m_eventQueue);

        static const struct wl_buffer_listener buffer_listener = {
            QWaylandBrcmBuffer::buffer_release
        };

        wl_buffer_add_listener(mBuffer, &buffer_listener, this);
    }

    ~QWaylandBrcmBuffer()
    {
        wl_array_release(&m_array);
        wl_buffer_destroy(mBuffer);
    }

    QSize size() const { return m_size; }

    void bind()
    {
        m_released = false;
    }

    void waitForRelease()
    {
        if (m_released)
            return;
        while (!m_released) {
            wl_display_dispatch_queue(m_display->wl_display(), m_eventQueue);
        }
    }

    static void buffer_release(void *data, wl_buffer *buffer)
    {
        Q_UNUSED(buffer);
        static_cast<QWaylandBrcmBuffer *>(data)->m_released = true;
    }

private:

    QSize m_size;
    bool m_released;
    wl_array m_array;
    EGLint *m_data;
    QWaylandDisplay *m_display;
    struct wl_event_queue *m_eventQueue;
};

QWaylandBrcmEglWindow::QWaylandBrcmEglWindow(QWindow *window)
    : QWaylandWindow(window)
    , m_eglIntegration(static_cast<QWaylandBrcmEglIntegration *>(mDisplay->clientBufferIntegration()))
    , m_eglConfig(0)
    , m_format(window->format())
    , m_eventQueue(wl_display_create_queue(mDisplay->wl_display()))
    , m_current(0)
    , m_count(0)
{
}

QWaylandBrcmEglWindow::~QWaylandBrcmEglWindow()
{
    destroyEglSurfaces();
}

QWaylandWindow::WindowType QWaylandBrcmEglWindow::windowType() const
{
    return QWaylandWindow::Egl;
}

void QWaylandBrcmEglWindow::setGeometry(const QRect &rect)
{
    destroyEglSurfaces();
    QWaylandWindow::setGeometry(rect);
}

QSurfaceFormat QWaylandBrcmEglWindow::format() const
{
    return m_format;
}

void QWaylandBrcmEglWindow::destroyEglSurfaces()
{
    for (int i = 0; i < m_count; ++i) {
        if (m_eglSurfaces[i]) {
            eglDestroySurface(m_eglIntegration->eglDisplay(), m_eglSurfaces[i]);
            m_eglSurfaces[i] = 0;
            // the server does this
            //m_eglIntegration->eglDestroyGlobalImageBRCM(&m_globalImages[5*i]);
            delete m_buffers[i];
        }
    }

    m_count = 0;
    m_current = 0;
}

QSurfaceFormat brcmFixFormat(const QSurfaceFormat &f)
{
    QSurfaceFormat format = f;
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setAlphaBufferSize(8);
    return format;
}

void QWaylandBrcmEglWindow::createEglSurfaces()
{
    QSize size(geometry().size());

    m_count = window()->format().swapBehavior() == QSurfaceFormat::TripleBuffer ? 3 : 2;

    m_eglConfig = q_configFromGLFormat(m_eglIntegration->eglDisplay(), brcmFixFormat(window()->format()), true, EGL_PIXMAP_BIT);

    m_format = q_glFormatFromConfig(m_eglIntegration->eglDisplay(), m_eglConfig);

    EGLint pixel_format = EGL_PIXEL_FORMAT_ARGB_8888_BRCM;

    EGLint rt;
    eglGetConfigAttrib(m_eglIntegration->eglDisplay(), m_eglConfig, EGL_RENDERABLE_TYPE, &rt);

    if (rt & EGL_OPENGL_ES_BIT) {
        pixel_format |= EGL_PIXEL_FORMAT_RENDER_GLES_BRCM;
        pixel_format |= EGL_PIXEL_FORMAT_GLES_TEXTURE_BRCM;
    }

    if (rt & EGL_OPENGL_ES2_BIT) {
        pixel_format |= EGL_PIXEL_FORMAT_RENDER_GLES2_BRCM;
        pixel_format |= EGL_PIXEL_FORMAT_GLES2_TEXTURE_BRCM;
    }

    if (rt & EGL_OPENVG_BIT) {
        pixel_format |= EGL_PIXEL_FORMAT_RENDER_VG_BRCM;
        pixel_format |= EGL_PIXEL_FORMAT_VG_IMAGE_BRCM;
    }

    if (rt & EGL_OPENGL_BIT) {
        pixel_format |= EGL_PIXEL_FORMAT_RENDER_GL_BRCM;
    }

    memset(m_globalImages, 0, 5 * m_count * sizeof(EGLint));
    for (int i = 0; i < m_count; ++i) {
        m_eglIntegration->eglCreateGlobalImageBRCM(size.width(), size.height(), pixel_format,
                0, size.width() * 4, &m_globalImages[5*i]);

        m_globalImages[5*i+2] = size.width();
        m_globalImages[5*i+3] = size.height();
        m_globalImages[5*i+4] = pixel_format;

        EGLint attrs[] = {
            EGL_VG_COLORSPACE, EGL_VG_COLORSPACE_sRGB,
            EGL_VG_ALPHA_FORMAT, pixel_format & EGL_PIXEL_FORMAT_ARGB_8888_PRE_BRCM ? EGL_VG_ALPHA_FORMAT_PRE : EGL_VG_ALPHA_FORMAT_NONPRE,
            EGL_NONE
        };

        m_eglSurfaces[i] = eglCreatePixmapSurface(m_eglIntegration->eglDisplay(), m_eglConfig, (EGLNativePixmapType)&m_globalImages[5*i], attrs);
        if (m_eglSurfaces[i] == EGL_NO_SURFACE)
            qFatal("eglCreatePixmapSurface failed: %x, global image id: %d %d\n", eglGetError(), m_globalImages[5*i], m_globalImages[5*i+1]);
        m_buffers[i] = new QWaylandBrcmBuffer(mDisplay, m_eglIntegration->waylandBrcm(), size, &m_globalImages[5*i], 5, m_eventQueue);
    }
}

void QWaylandBrcmEglWindow::swapBuffers()
{
    if (m_eglIntegration->eglFlushBRCM) {
        m_eglIntegration->eglFlushBRCM();
    } else {
        glFlush();
        glFinish();
    }

    if (!m_count)
        return;

    m_buffers[m_current]->bind();
    attach(m_buffers[m_current], 0, 0);
    damage(QRect(QPoint(), geometry().size()));
    wl_surface::commit();

    m_current = (m_current + 1) % m_count;
    m_buffers[m_current]->waitForRelease();
}

bool QWaylandBrcmEglWindow::makeCurrent(EGLContext context)
{
    if (!m_count)
        const_cast<QWaylandBrcmEglWindow *>(this)->createEglSurfaces();
    return eglMakeCurrent(m_eglIntegration->eglDisplay(), m_eglSurfaces[m_current], m_eglSurfaces[m_current], context);
}

}

QT_END_NAMESPACE
