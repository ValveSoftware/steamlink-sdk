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

#include "qwaylandnativeinterface_p.h"
#include "qwaylanddisplay_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandsubsurface_p.h"
#include "qwaylandextendedsurface_p.h"
#include "qwaylandintegration_p.h"
#include "qwaylanddisplay_p.h"
#include "qwaylandwindowmanagerintegration_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandwlshellsurface_p.h"
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/QScreen>
#include <QtWaylandClient/private/qwaylandclientbufferintegration_p.h>

#include <QtPlatformHeaders/qwaylandwindowfunctions.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandNativeInterface::QWaylandNativeInterface(QWaylandIntegration *integration)
    : m_integration(integration)
{
}

void *QWaylandNativeInterface::nativeResourceForIntegration(const QByteArray &resourceString)
{
    QByteArray lowerCaseResource = resourceString.toLower();

    if (lowerCaseResource == "display" || lowerCaseResource == "wl_display" || lowerCaseResource == "nativedisplay")
        return m_integration->display()->wl_display();
    if (lowerCaseResource == "compositor")
        return const_cast<wl_compositor *>(m_integration->display()->wl_compositor());
    if (lowerCaseResource == "server_buffer_integration")
        return m_integration->serverBufferIntegration();

    if (lowerCaseResource == "egldisplay" && m_integration->clientBufferIntegration())
        return m_integration->clientBufferIntegration()->nativeResource(QWaylandClientBufferIntegration::EglDisplay);

    return nullptr;
}

void *QWaylandNativeInterface::nativeResourceForWindow(const QByteArray &resourceString, QWindow *window)
{
    QByteArray lowerCaseResource = resourceString.toLower();

    if (lowerCaseResource == "display")
        return m_integration->display()->wl_display();
    if (lowerCaseResource == "compositor")
        return const_cast<wl_compositor *>(m_integration->display()->wl_compositor());
    if (lowerCaseResource == "surface") {
        QWaylandWindow *w = static_cast<QWaylandWindow*>(window->handle());
        return w ? w->object() : nullptr;
    }
    if (lowerCaseResource == "wl_shell_surface") {
        QWaylandWindow *w = static_cast<QWaylandWindow*>(window->handle());
        if (!w)
            return nullptr;
        QWaylandWlShellSurface *s = qobject_cast<QWaylandWlShellSurface *>(w->shellSurface());
        if (!s)
            return nullptr;
        return s->object();
    }
    if (lowerCaseResource == "egldisplay" && m_integration->clientBufferIntegration())
        return m_integration->clientBufferIntegration()->nativeResource(QWaylandClientBufferIntegration::EglDisplay);

    return nullptr;
}

void *QWaylandNativeInterface::nativeResourceForScreen(const QByteArray &resourceString, QScreen *screen)
{
    QByteArray lowerCaseResource = resourceString.toLower();

    if (lowerCaseResource == "output")
        return ((QWaylandScreen *) screen->handle())->output();

    return nullptr;
}

#if QT_CONFIG(opengl)
void *QWaylandNativeInterface::nativeResourceForContext(const QByteArray &resource, QOpenGLContext *context)
{
#if QT_CONFIG(opengl)
    QByteArray lowerCaseResource = resource.toLower();

    if (lowerCaseResource == "eglconfig" && m_integration->clientBufferIntegration())
        return m_integration->clientBufferIntegration()->nativeResourceForContext(QWaylandClientBufferIntegration::EglConfig, context->handle());

    if (lowerCaseResource == "eglcontext" && m_integration->clientBufferIntegration())
        return m_integration->clientBufferIntegration()->nativeResourceForContext(QWaylandClientBufferIntegration::EglContext, context->handle());

    if (lowerCaseResource == "egldisplay" && m_integration->clientBufferIntegration())
        return m_integration->clientBufferIntegration()->nativeResourceForContext(QWaylandClientBufferIntegration::EglDisplay, context->handle());
#endif

    return nullptr;
}
#endif  // opengl

QVariantMap QWaylandNativeInterface::windowProperties(QPlatformWindow *window) const
{
    QWaylandWindow *waylandWindow = static_cast<QWaylandWindow *>(window);
    return waylandWindow->properties();
}

QVariant QWaylandNativeInterface::windowProperty(QPlatformWindow *window, const QString &name) const
{
    QWaylandWindow *waylandWindow = static_cast<QWaylandWindow *>(window);
    return waylandWindow->property(name);
}

QVariant QWaylandNativeInterface::windowProperty(QPlatformWindow *window, const QString &name, const QVariant &defaultValue) const
{
    QWaylandWindow *waylandWindow = static_cast<QWaylandWindow *>(window);
    return waylandWindow->property(name, defaultValue);
}

void QWaylandNativeInterface::setWindowProperty(QPlatformWindow *window, const QString &name, const QVariant &value)
{
    QWaylandWindow *wlWindow = static_cast<QWaylandWindow*>(window);
    wlWindow->sendProperty(name, value);
}

void QWaylandNativeInterface::emitWindowPropertyChanged(QPlatformWindow *window, const QString &name)
{
    emit windowPropertyChanged(window,name);
}

QFunctionPointer QWaylandNativeInterface::platformFunction(const QByteArray &resource) const
{
    if (resource == QWaylandWindowFunctions::setSyncIdentifier()) {
        return QFunctionPointer(setSync);
    } else if (resource == QWaylandWindowFunctions::setDeSyncIdentifier()) {
        return QFunctionPointer(setDeSync);
    } else if (resource == QWaylandWindowFunctions::isSyncIdentifier()) {
        return QFunctionPointer(isSync);
    }
    return nullptr;
}


void QWaylandNativeInterface::setSync(QWindow *window)
{
    QWaylandWindow *ww = static_cast<QWaylandWindow*>(window->handle());
    if (ww->subSurfaceWindow()) {
        ww->subSurfaceWindow()->setSync();
    }
}

void QWaylandNativeInterface::setDeSync(QWindow *window)
{
    QWaylandWindow *ww = static_cast<QWaylandWindow*>(window->handle());
    if (ww->subSurfaceWindow()) {
        ww->subSurfaceWindow()->setDeSync();
    }
}

bool QWaylandNativeInterface::isSync(QWindow *window)
{
    QWaylandWindow *ww = static_cast<QWaylandWindow*>(window->handle());
    if (ww->subSurfaceWindow()) {
        return ww->subSurfaceWindow()->isSync();
    }
    return false;
}

}

QT_END_NAMESPACE
