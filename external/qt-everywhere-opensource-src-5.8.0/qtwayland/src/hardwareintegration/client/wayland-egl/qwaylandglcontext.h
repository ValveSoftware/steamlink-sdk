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

#ifndef QWAYLANDGLCONTEXT_H
#define QWAYLANDGLCONTEXT_H

#include <QtWaylandClient/private/qwaylanddisplay_p.h>

#include <qpa/qplatformopenglcontext.h>

#include "qwaylandeglinclude.h"

QT_BEGIN_NAMESPACE

class QOpenGLShaderProgram;
class QOpenGLTextureCache;

namespace QtWaylandClient {

class QWaylandWindow;
class QWaylandGLWindowSurface;
class DecorationsBlitter;

class QWaylandGLContext : public QPlatformOpenGLContext
{
public:
    QWaylandGLContext(EGLDisplay eglDisplay, QWaylandDisplay *display, const QSurfaceFormat &format, QPlatformOpenGLContext *share);
    ~QWaylandGLContext();

    void swapBuffers(QPlatformSurface *surface) Q_DECL_OVERRIDE;

    bool makeCurrent(QPlatformSurface *surface) Q_DECL_OVERRIDE;
    void doneCurrent() Q_DECL_OVERRIDE;

    GLuint defaultFramebufferObject(QPlatformSurface *surface) const Q_DECL_OVERRIDE;

    bool isSharing() const Q_DECL_OVERRIDE;
    bool isValid() const Q_DECL_OVERRIDE;

    QFunctionPointer getProcAddress(const char *procName) Q_DECL_OVERRIDE;

    QSurfaceFormat format() const Q_DECL_OVERRIDE { return m_format; }

    EGLConfig eglConfig() const;
    EGLContext eglContext() const { return m_context; }

private:
    void updateGLFormat();

    EGLDisplay m_eglDisplay;
    QWaylandDisplay *m_display;
    EGLContext m_context;
    EGLContext m_shareEGLContext;
    EGLConfig m_config;
    QSurfaceFormat m_format;
    DecorationsBlitter *m_blitter;
    bool mUseNativeDefaultFbo;
    uint m_api;
    bool mSupportNonBlockingSwap;

    friend class DecorationsBlitter;
};

}

QT_END_NAMESPACE

#endif // QWAYLANDGLCONTEXT_H
