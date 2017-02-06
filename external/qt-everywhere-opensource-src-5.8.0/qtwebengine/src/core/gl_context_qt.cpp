/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "gl_context_qt.h"

#include <QGuiApplication>
#include <QOpenGLContext>
#include <QThread>
#include <qpa/qplatformnativeinterface.h>
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_implementation.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#endif

#if defined(OS_WIN)
#include "ui/gl/gl_context_wgl.h"
#endif

QT_BEGIN_NAMESPACE

Q_GUI_EXPORT QOpenGLContext *qt_gl_global_share_context();
GLContextHelper* GLContextHelper::contextHelper = 0;

namespace {

inline void *resourceForContext(const QByteArray &resource)
{
#ifndef QT_NO_OPENGL
    QOpenGLContext *shareContext = qt_gl_global_share_context();
    if (!shareContext) {
        qFatal("QWebEngine: OpenGL resource sharing is not set up in QtQuick. Please make sure to call QtWebEngine::initialize() in your main() function.");
    }
    return qApp->platformNativeInterface()->nativeResourceForContext(resource, shareContext);
#else
    return nullptr;
#endif
}

inline void *resourceForIntegration(const QByteArray &resource)
{
    return qApp->platformNativeInterface()->nativeResourceForIntegration(resource);
}

}

void GLContextHelper::initialize()
{
    if (!contextHelper)
        contextHelper = new GLContextHelper;
}

void GLContextHelper::destroy()
{
    delete contextHelper;
    contextHelper = 0;
}

bool GLContextHelper::initializeContextOnBrowserThread(gl::GLContext* context, gl::GLSurface* surface)
{
    return context->Initialize(surface, gl::PreferDiscreteGpu);
}

bool GLContextHelper::initializeContext(gl::GLContext* context, gl::GLSurface* surface)
{
    bool ret = false;
    Qt::ConnectionType connType = (QThread::currentThread() == qApp->thread()) ? Qt::DirectConnection : Qt::BlockingQueuedConnection;
    QMetaObject::invokeMethod(contextHelper, "initializeContextOnBrowserThread", connType,
            Q_RETURN_ARG(bool, ret),
            Q_ARG(gl::GLContext*, context),
            Q_ARG(gl::GLSurface*, surface));
    return ret;
}

void* GLContextHelper::getEGLConfig()
{
    QByteArray resource = QByteArrayLiteral("eglconfig");
    return resourceForContext(resource);
}

void* GLContextHelper::getXConfig()
{
    return resourceForContext(QByteArrayLiteral("glxconfig"));
}

void* GLContextHelper::getEGLDisplay()
{
    return resourceForIntegration(QByteArrayLiteral("egldisplay"));
}

void* GLContextHelper::getXDisplay()
{
    void *display = qApp->platformNativeInterface()->nativeResourceForScreen(QByteArrayLiteral("display"), qApp->primaryScreen());
#if defined(USE_X11)
    if (!display) {
        // XLib isn't available or has not been initialized, which is a decision we wish to
        // support, for example for the GPU process.
        display = XOpenDisplay(NULL);
    }
#endif
    return display;
}

void* GLContextHelper::getNativeDisplay()
{
    return resourceForIntegration(QByteArrayLiteral("nativedisplay"));
}

QT_END_NAMESPACE

#if defined(USE_OZONE) || defined(OS_WIN)

namespace gl {

namespace init {

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group, GLSurface* compatible_surface, GpuPreference gpu_preference)
{
#if defined(OS_WIN)
    scoped_refptr<GLContext> context;
    if (GetGLImplementation() == kGLImplementationDesktopGL) {
        context = new GLContextWGL(share_group);
        if (!context->Initialize(compatible_surface, gpu_preference))
            return nullptr;
        return context;
    } else {
        context = new GLContextEGL(share_group);
    }
#else
    scoped_refptr<GLContext> context = new GLContextEGL(share_group);
#endif

    if (!GLContextHelper::initializeContext(context.get(), compatible_surface))
        return NULL;

    return context;
}

}  // namespace init

}  // namespace gl

#endif // defined(USE_OZONE) || defined(OS_WIN)
