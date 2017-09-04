/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include <QDebug>
#include "qopenvgoffscreensurface.h"

#include <QtGui/QImage>


QT_BEGIN_NAMESPACE

QOpenVGOffscreenSurface::QOpenVGOffscreenSurface(const QSize &size)
    : m_size(size)
{
    m_display = eglGetCurrentDisplay();
    m_image = vgCreateImage(VG_sARGB_8888_PRE, m_size.width(), m_size.height(), VG_IMAGE_QUALITY_BETTER);

    const EGLint configAttribs[] = {
        EGL_CONFORMANT, EGL_OPENVG_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_ALPHA_MASK_SIZE, 8,
        EGL_NONE
    };

    EGLConfig pbufferConfig;
    EGLint numConfig;
    eglChooseConfig(m_display, configAttribs, &pbufferConfig, 1, &numConfig);

    m_context = eglCreateContext(m_display, pbufferConfig, eglGetCurrentContext(), 0);
    if (m_context == EGL_NO_CONTEXT)
        qWarning("QOpenVGOffscreenSurface: failed to create EGLContext");

    m_renderTarget = eglCreatePbufferFromClientBuffer(m_display,
                                                      EGL_OPENVG_IMAGE,
                                                      reinterpret_cast<EGLClientBuffer>(uintptr_t(m_image)),
                                                      pbufferConfig,
                                                      0);
    if (m_renderTarget == EGL_NO_SURFACE)
        qWarning("QOpenVGOffscreenSurface: failed to create EGLSurface from VGImage");
}

QOpenVGOffscreenSurface::~QOpenVGOffscreenSurface()
{
    vgDestroyImage(m_image);
    eglDestroySurface(m_display, m_renderTarget);
    eglDestroyContext(m_display, m_context);
}

void QOpenVGOffscreenSurface::makeCurrent()
{
    EGLContext currentContext = eglGetCurrentContext();
    if (m_context != currentContext) {
        m_previousContext = eglGetCurrentContext();
        m_previousReadSurface = eglGetCurrentSurface(EGL_READ);
        m_previousDrawSurface = eglGetCurrentSurface(EGL_DRAW);

        eglMakeCurrent(m_display, m_renderTarget, m_renderTarget, m_context);
    }
}

void QOpenVGOffscreenSurface::doneCurrent()
{
    EGLContext currentContext = eglGetCurrentContext();
    if (m_context == currentContext) {
        eglMakeCurrent(m_display, m_previousDrawSurface, m_previousReadSurface, m_previousContext);
        m_previousContext = EGL_NO_CONTEXT;
        m_previousReadSurface = EGL_NO_SURFACE;
        m_previousDrawSurface = EGL_NO_SURFACE;
    }
}

void QOpenVGOffscreenSurface::swapBuffers()
{
    eglSwapBuffers(m_display, m_renderTarget);
}

QImage QOpenVGOffscreenSurface::readbackQImage()
{
    QImage readbackImage(m_size, QImage::Format_ARGB32_Premultiplied);
    vgGetImageSubData(m_image, readbackImage.bits(), readbackImage.bytesPerLine(), VG_sARGB_8888_PRE, 0, 0, m_size.width(), m_size.height());
    return readbackImage;
}

QT_END_NAMESPACE
