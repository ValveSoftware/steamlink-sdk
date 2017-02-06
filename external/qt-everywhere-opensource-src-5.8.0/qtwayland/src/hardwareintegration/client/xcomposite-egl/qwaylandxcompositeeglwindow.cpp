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

#include "qwaylandxcompositeeglwindow.h"
#include "qwaylandxcompositebuffer.h"

#include <QtEglSupport/private/qeglconvenience_p.h>
#include <QtEglSupport/private/qxlibeglintegration_p.h>

#include "wayland-xcomposite-client-protocol.h"

#include <X11/extensions/Xcomposite.h>
#include "qwaylandxcompositeeglclientbufferintegration.h"

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandXCompositeEGLWindow::QWaylandXCompositeEGLWindow(QWindow *window, QWaylandXCompositeEGLClientBufferIntegration *glxIntegration)
    : QWaylandWindow(window)
    , m_glxIntegration(glxIntegration)
    , m_context(0)
    , m_buffer(0)
    , m_xWindow(0)
    , m_config(q_configFromGLFormat(glxIntegration->eglDisplay(), window->format(), true, EGL_WINDOW_BIT | EGL_PIXMAP_BIT))
    , m_surface(0)
    , mBuffer(0)
{
}

QWaylandWindow::WindowType QWaylandXCompositeEGLWindow::windowType() const
{
    //yeah. this type needs a new name
    return QWaylandWindow::Egl;
}

void QWaylandXCompositeEGLWindow::setGeometry(const QRect &rect)
{
    QWaylandWindow::setGeometry(rect);

    if (m_surface) {
        eglDestroySurface(m_glxIntegration->eglDisplay(), m_surface);
        m_surface = 0;
    }
}

EGLSurface QWaylandXCompositeEGLWindow::eglSurface() const
{
    if (!m_surface)
        const_cast<QWaylandXCompositeEGLWindow *>(this)->createEglSurface();
    return m_surface;
}

void QWaylandXCompositeEGLWindow::createEglSurface()
{
    QSize size(geometry().size());
    if (size.isEmpty()) {
        // QGLWidget wants a context for a window without geometry
        size = QSize(1,1);
    }

    delete m_buffer;
    //XFreePixmap deletes the glxPixmap as well
    if (m_xWindow) {
        XDestroyWindow(m_glxIntegration->xDisplay(), m_xWindow);
    }

    VisualID visualId = QXlibEglIntegration::getCompatibleVisualId(m_glxIntegration->xDisplay(), m_glxIntegration->eglDisplay(), m_config);

    XVisualInfo visualInfoTemplate;
    memset(&visualInfoTemplate, 0, sizeof(XVisualInfo));
    visualInfoTemplate.visualid = visualId;

    int matchingCount = 0;
    XVisualInfo *visualInfo = XGetVisualInfo(m_glxIntegration->xDisplay(), VisualIDMask, &visualInfoTemplate, &matchingCount);

    Colormap cmap = XCreateColormap(m_glxIntegration->xDisplay(),m_glxIntegration->rootWindow(),visualInfo->visual,AllocNone);

    XSetWindowAttributes a;
    a.colormap = cmap;
    m_xWindow = XCreateWindow(m_glxIntegration->xDisplay(), m_glxIntegration->rootWindow(),0, 0, size.width(), size.height(),
                             0, visualInfo->depth, InputOutput, visualInfo->visual,
                             CWColormap, &a);

    XCompositeRedirectWindow(m_glxIntegration->xDisplay(), m_xWindow, CompositeRedirectManual);
    XMapWindow(m_glxIntegration->xDisplay(), m_xWindow);

    m_surface = eglCreateWindowSurface(m_glxIntegration->eglDisplay(), m_config, m_xWindow,0);
    if (m_surface == EGL_NO_SURFACE) {
        qFatal("Could not make eglsurface");
    }

    XSync(m_glxIntegration->xDisplay(),False);
    m_buffer = new QWaylandXCompositeBuffer(m_glxIntegration->waylandXComposite(),
                                           (uint32_t)m_xWindow,
                                           size);
}

}

QT_END_NAMESPACE
