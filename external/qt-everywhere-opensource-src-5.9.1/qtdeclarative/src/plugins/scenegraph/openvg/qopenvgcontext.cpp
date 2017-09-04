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

#include <qpa/qplatformnativeinterface.h>
#include <QtGui/QGuiApplication>
#include <QtCore/QVector>
#include <QtCore/QDebug>

#include "qopenvgcontext_p.h"

QT_BEGIN_NAMESPACE

QOpenVGContext::QOpenVGContext(QWindow *window)
    : m_window(window)
{
    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    m_display = reinterpret_cast<EGLDisplay>(nativeInterface->nativeResourceForWindow("EglDisplay", window));
    m_surface = reinterpret_cast<EGLSurface>(nativeInterface->nativeResourceForWindow("EglSurface", window));

    if (m_display == 0)
        qFatal("QOpenVGContext: failed to get EGLDisplay");
    if (m_surface == 0)
        qFatal("QOpenVGContext: failed to get EGLSurface");

    EGLint configID = 0;
    if (eglQuerySurface(m_display, m_surface, EGL_CONFIG_ID, &configID)) {
        EGLint numConfigs;
        const EGLint configAttribs[] = {
            EGL_CONFIG_ID, configID,
            EGL_NONE
        };
        eglChooseConfig(m_display, configAttribs, &m_config, 1, &numConfigs);
    } else {
        qFatal("QOpenVGContext: failed to get surface config");
    }

    // Create an EGL Context
    eglBindAPI(EGL_OPENVG_API);
    m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, 0);
    if (!m_context)
        qFatal("QOpenVGContext: eglCreateContext failed");
}

QOpenVGContext::~QOpenVGContext()
{
    doneCurrent();
    eglDestroyContext(m_display, m_context);
}

void QOpenVGContext::makeCurrent()
{
    makeCurrent(m_surface);
}

void QOpenVGContext::makeCurrent(EGLSurface surface)
{
    eglMakeCurrent(m_display, surface, surface, m_context);
}

void QOpenVGContext::doneCurrent()
{
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void QOpenVGContext::swapBuffers()
{
    swapBuffers(m_surface);
}

void QOpenVGContext::swapBuffers(EGLSurface surface)
{
    eglSwapBuffers(m_display, surface);
}

QWindow *QOpenVGContext::window() const
{
    return m_window;
}

QImage QOpenVGContext::readFramebuffer(const QSize &size)
{
    QImage framebufferImage(size, QImage::Format_RGB32);
    vgReadPixels(framebufferImage.bits(), framebufferImage.bytesPerLine(), VG_sXRGB_8888, 0, 0, size.width(), size.height());
    return framebufferImage.mirrored(false, true);
}

void QOpenVGContext::getConfigs()
{
    EGLint configsAvailable = 0;
    eglGetConfigs(m_display, 0, 0, &configsAvailable);

    QVector<EGLConfig> configs(configsAvailable);
    eglGetConfigs(m_display, configs.data(), configs.size(), &configsAvailable);

    for (EGLConfig config : configs) {
        EGLint value;
        eglGetConfigAttrib(m_display, config, EGL_CONFIG_ID, &value);
        qDebug() << "#################\n" << "EGL_CONFIG_ID:" << value;
        eglGetConfigAttrib(m_display, config, EGL_BUFFER_SIZE, &value);
        qDebug() << "EGL_BUFFER_SIZE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_ALPHA_SIZE, &value);
        qDebug() << "EGL_ALPHA_SIZE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_RED_SIZE, &value);
        qDebug() << "EGL_RED_SIZE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_GREEN_SIZE, &value);
        qDebug() << "EGL_GREEN_SIZE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_BLUE_SIZE, &value);
        qDebug() << "EGL_BLUE_SIZE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_DEPTH_SIZE, &value);
        qDebug() << "EGL_DEPTH_SIZE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_STENCIL_SIZE, &value);
        qDebug() << "EGL_STENCIL_SIZE:" << value;

        eglGetConfigAttrib(m_display, config, EGL_ALPHA_MASK_SIZE, &value);
        qDebug() << "EGL_ALPHA_MASK_SIZE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_BIND_TO_TEXTURE_RGB, &value);
        qDebug() << "EGL_BIND_TO_TEXTURE_RGB:" << value;
        eglGetConfigAttrib(m_display, config, EGL_BIND_TO_TEXTURE_RGBA, &value);
        qDebug() << "EGL_BIND_TO_TEXTURE_RGBA:" << value;


        eglGetConfigAttrib(m_display, config, EGL_COLOR_BUFFER_TYPE, &value);
        qDebug() << "EGL_COLOR_BUFFER_TYPE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_CONFIG_CAVEAT, &value);
        qDebug() << "EGL_CONFIG_CAVEAT:" << value;
        eglGetConfigAttrib(m_display, config, EGL_CONFORMANT, &value);
        qDebug() << "EGL_CONFORMANT:" << value;


        eglGetConfigAttrib(m_display, config, EGL_LEVEL, &value);
        qDebug() << "EGL_LEVEL:" << value;
        eglGetConfigAttrib(m_display, config, EGL_LUMINANCE_SIZE, &value);
        qDebug() << "EGL_LUMINANCE_SIZE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_MAX_PBUFFER_WIDTH, &value);
        qDebug() << "EGL_MAX_PBUFFER_WIDTH:" << value;
        eglGetConfigAttrib(m_display, config, EGL_MAX_PBUFFER_HEIGHT, &value);
        qDebug() << "EGL_MAX_PBUFFER_HEIGHT:" << value;
        eglGetConfigAttrib(m_display, config, EGL_MAX_PBUFFER_PIXELS, &value);
        qDebug() << "EGL_MAX_PBUFFER_PIXELS:" << value;
        eglGetConfigAttrib(m_display, config, EGL_MAX_SWAP_INTERVAL, &value);
        qDebug() << "EGL_MAX_SWAP_INTERVAL:" << value;
        eglGetConfigAttrib(m_display, config, EGL_MIN_SWAP_INTERVAL, &value);
        qDebug() << "EGL_MIN_SWAP_INTERVAL:" << value;
        eglGetConfigAttrib(m_display, config, EGL_NATIVE_RENDERABLE, &value);
        qDebug() << "EGL_NATIVE_RENDERABLE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_NATIVE_VISUAL_ID, &value);
        qDebug() << "EGL_NATIVE_VISUAL_ID:" << value;
        eglGetConfigAttrib(m_display, config, EGL_NATIVE_VISUAL_TYPE, &value);
        qDebug() << "EGL_NATIVE_VISUAL_TYPE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_RENDERABLE_TYPE, &value);
        qDebug() << "EGL_RENDERABLE_TYPE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_SAMPLE_BUFFERS, &value);
        qDebug() << "EGL_SAMPLE_BUFFERS:" << value;
        eglGetConfigAttrib(m_display, config, EGL_SAMPLES, &value);
        qDebug() << "EGL_SAMPLES:" << value;

        eglGetConfigAttrib(m_display, config, EGL_SURFACE_TYPE, &value);
        qDebug() << "EGL_SURFACE_TYPE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_TRANSPARENT_TYPE, &value);
        qDebug() << "EGL_TRANSPARENT_TYPE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_TRANSPARENT_RED_VALUE, &value);
        qDebug() << "EGL_TRANSPARENT_RED_VALUE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_TRANSPARENT_GREEN_VALUE, &value);
        qDebug() << "EGL_TRANSPARENT_GREEN_VALUE:" << value;
        eglGetConfigAttrib(m_display, config, EGL_TRANSPARENT_BLUE_VALUE, &value);
        qDebug() << "EGL_TRANSPARENT_BLUE_VALUE:" << value;
    }
}

void QOpenVGContext::checkErrors()
{
    VGErrorCode error;
    EGLint eglError;
    do {
        error = vgGetError();
        eglError = eglGetError();
        qDebug() << "error: " << QString::number(error, 16);
        qDebug() << "eglError: " << QString::number(eglError, 16);
    } while (error != VG_NO_ERROR && eglError != EGL_SUCCESS);
}

QT_END_NAMESPACE
