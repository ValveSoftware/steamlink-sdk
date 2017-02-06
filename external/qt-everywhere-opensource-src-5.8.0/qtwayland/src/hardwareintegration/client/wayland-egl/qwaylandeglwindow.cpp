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

#include "qwaylandeglwindow.h"

#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include "qwaylandglcontext.h"

#include <QtEglSupport/private/qeglconvenience_p.h>

#include <QDebug>
#include <QtGui/QWindow>
#include <qpa/qwindowsysteminterface.h>
#include <QOpenGLFramebufferObject>
#include <QOpenGLContext>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandEglWindow::QWaylandEglWindow(QWindow *window)
    : QWaylandWindow(window)
    , m_clientBufferIntegration(static_cast<QWaylandEglClientBufferIntegration *>(mDisplay->clientBufferIntegration()))
    , m_waylandEglWindow(0)
    , m_eglSurface(0)
    , m_contentFBO(0)
    , m_resize(false)
{
    QSurfaceFormat fmt = window->requestedFormat();
    if (mDisplay->supportsWindowDecoration())
        fmt.setAlphaBufferSize(8);
    m_eglConfig = q_configFromGLFormat(m_clientBufferIntegration->eglDisplay(), fmt);
    m_format = q_glFormatFromConfig(m_clientBufferIntegration->eglDisplay(), m_eglConfig);

    // Do not create anything from here. This platform window may belong to a
    // RasterGLSurface window which may have pure raster content.  In this case, where the
    // window is never actually made current, creating a wl_egl_window and EGL surface
    // should be avoided.
}

QWaylandEglWindow::~QWaylandEglWindow()
{
    if (m_eglSurface) {
        eglDestroySurface(m_clientBufferIntegration->eglDisplay(), m_eglSurface);
        m_eglSurface = 0;
    }

    if (m_waylandEglWindow)
        wl_egl_window_destroy(m_waylandEglWindow);

    delete m_contentFBO;
}

QWaylandWindow::WindowType QWaylandEglWindow::windowType() const
{
    return QWaylandWindow::Egl;
}

void QWaylandEglWindow::setGeometry(const QRect &rect)
{
    QWaylandWindow::setGeometry(rect);
    // If the surface was invalidated through invalidateSurface() and
    // we're now getting a resize we don't want to create it again.
    // Just resize the wl_egl_window, the EGLSurface will be created
    // the next time makeCurrent is called.
    updateSurface(false);
}

void QWaylandEglWindow::updateSurface(bool create)
{
    QMargins margins = frameMargins();
    QRect rect = geometry();
    QSize sizeWithMargins = (rect.size() + QSize(margins.left() + margins.right(), margins.top() + margins.bottom())) * scale();

    // wl_egl_windows must have both width and height > 0
    // mesa's egl returns NULL if we try to create a, invalid wl_egl_window, however not all EGL
    // implementations may do that, so check the size ourself. Besides, we must deal with resizing
    // a valid window to 0x0, which would make it invalid. Hence, destroy it.
    if (sizeWithMargins.isEmpty()) {
        if (m_eglSurface) {
            eglDestroySurface(m_clientBufferIntegration->eglDisplay(), m_eglSurface);
            m_eglSurface = 0;
        }
        if (m_waylandEglWindow) {
            wl_egl_window_destroy(m_waylandEglWindow);
            m_waylandEglWindow = 0;
        }
        mOffset = QPoint();
    } else {
        if (m_waylandEglWindow) {
            int current_width, current_height;
            wl_egl_window_get_attached_size(m_waylandEglWindow,&current_width,&current_height);
            if (current_width != sizeWithMargins.width() || current_height != sizeWithMargins.height()) {
                wl_egl_window_resize(m_waylandEglWindow, sizeWithMargins.width(), sizeWithMargins.height(), mOffset.x(), mOffset.y());
                mOffset = QPoint();

                m_resize = true;
            }
        } else if (create) {
            m_waylandEglWindow = wl_egl_window_create(object(), sizeWithMargins.width(), sizeWithMargins.height());
        }

        if (!m_eglSurface && create) {
            EGLNativeWindowType eglw = (EGLNativeWindowType) m_waylandEglWindow;
            m_eglSurface = eglCreateWindowSurface(m_clientBufferIntegration->eglDisplay(), m_eglConfig, eglw, 0);
        }
    }
}

QRect QWaylandEglWindow::contentsRect() const
{
    QRect r = geometry();
    QMargins m = frameMargins();
    return QRect(m.left(), m.bottom(), r.width(), r.height());
}

QSurfaceFormat QWaylandEglWindow::format() const
{
    return m_format;
}

void QWaylandEglWindow::setVisible(bool visible)
{
    QWaylandWindow::setVisible(visible);
    if (!visible)
        QMetaObject::invokeMethod(this, "doInvalidateSurface", Qt::QueuedConnection);
}

void QWaylandEglWindow::doInvalidateSurface()
{
    if (!window()->isVisible())
        invalidateSurface();
}

void QWaylandEglWindow::invalidateSurface()
{
    if (m_eglSurface) {
        eglDestroySurface(m_clientBufferIntegration->eglDisplay(), m_eglSurface);
        m_eglSurface = 0;
    }
    if (m_waylandEglWindow) {
        wl_egl_window_destroy(m_waylandEglWindow);
        m_waylandEglWindow = nullptr;
    }
}

EGLSurface QWaylandEglWindow::eglSurface() const
{
    return m_eglSurface;
}

GLuint QWaylandEglWindow::contentFBO() const
{
    if (!decoration())
        return 0;

    if (m_resize || !m_contentFBO) {
        QOpenGLFramebufferObject *old = m_contentFBO;
        QSize fboSize = geometry().size() * scale();
        m_contentFBO = new QOpenGLFramebufferObject(fboSize.width(), fboSize.height(), QOpenGLFramebufferObject::CombinedDepthStencil);

        delete old;
        m_resize = false;
    }

    return m_contentFBO->handle();
}

GLuint QWaylandEglWindow::contentTexture() const
{
    return m_contentFBO->texture();
}

void QWaylandEglWindow::bindContentFBO()
{
    if (decoration()) {
        contentFBO();
        m_contentFBO->bind();
    }
}

}

QT_END_NAMESPACE
