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

#include <QtCore/QDebug>

#include "qwaylandxcompositeglxcontext.h"

#include "qwaylandxcompositeglxwindow.h"

#include <QRegion>

#include <dlfcn.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandXCompositeGLXContext::QWaylandXCompositeGLXContext(const QSurfaceFormat &format, QPlatformOpenGLContext *share, Display *display, int screen)
    : m_display(display),
      m_format(format)
{
    qDebug("creating XComposite-GLX context");

    if (m_format.renderableType() == QSurfaceFormat::DefaultRenderableType)
        m_format.setRenderableType(QSurfaceFormat::OpenGL);

    if (m_format.renderableType() != QSurfaceFormat::OpenGL) {
        qWarning("Unsupported renderable type");
        return;
    }

    GLXContext shareContext = share ? static_cast<QWaylandXCompositeGLXContext *>(share)->m_context : 0;
    GLXFBConfig config = qglx_findConfig(display, screen, m_format, GLX_WINDOW_BIT | GLX_PIXMAP_BIT);
    XVisualInfo *visualInfo = glXGetVisualFromFBConfig(display, config);
    m_context = glXCreateContext(display, visualInfo, shareContext, true);
    qglx_surfaceFormatFromGLXFBConfig(&m_format, display, config);
}

bool QWaylandXCompositeGLXContext::makeCurrent(QPlatformSurface *surface)
{
    Window xWindow = static_cast<QWaylandXCompositeGLXWindow *>(surface)->xWindow();

    return glXMakeCurrent(m_display, xWindow, m_context);
}

void QWaylandXCompositeGLXContext::doneCurrent()
{
    glXMakeCurrent(m_display, 0, 0);
}

void QWaylandXCompositeGLXContext::swapBuffers(QPlatformSurface *surface)
{
    QWaylandXCompositeGLXWindow *w = static_cast<QWaylandXCompositeGLXWindow *>(surface);

    QSize size = w->geometry().size();

    glXSwapBuffers(m_display, w->xWindow());

    w->commit(w->buffer(), QRegion(0, 0, size.width(), size.height()));
    w->waitForFrameSync();
}

QFunctionPointer QWaylandXCompositeGLXContext::getProcAddress(const char *procName)
{
    QFunctionPointer proc = glXGetProcAddress(reinterpret_cast<const GLubyte *>(procName));
    if (!proc)
        proc = (QFunctionPointer) dlsym(RTLD_DEFAULT, procName);
    return proc;
}

QSurfaceFormat QWaylandXCompositeGLXContext::format() const
{
    return m_format;
}

}

QT_END_NAMESPACE
