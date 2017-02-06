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

#include "qwaylandbrcmeglintegration.h"

#include <QtWaylandClient/private/qwaylandclientbufferintegration_p.h>

#include "qwaylandbrcmeglwindow.h"
#include "qwaylandbrcmglcontext.h"

#include <QtCore/QDebug>

#include "wayland-brcm-client-protocol.h"

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandBrcmEglIntegration::QWaylandBrcmEglIntegration()
    : m_waylandDisplay(0)
{
    qDebug() << "Using Brcm-EGL";
}

void QWaylandBrcmEglIntegration::wlDisplayHandleGlobal(void *data, struct wl_registry *registry, uint32_t id, const QString &interface, uint32_t version)
{
    Q_UNUSED(version);
    if (interface == "qt_brcm") {
        QWaylandBrcmEglIntegration *integration = static_cast<QWaylandBrcmEglIntegration *>(data);
        integration->m_waylandBrcm = static_cast<struct qt_brcm *>(wl_registry_bind(registry, id, &qt_brcm_interface, 1));
    }
}

qt_brcm *QWaylandBrcmEglIntegration::waylandBrcm() const
{
    return m_waylandBrcm;
}

QWaylandBrcmEglIntegration::~QWaylandBrcmEglIntegration()
{
    eglTerminate(m_eglDisplay);
}

void QWaylandBrcmEglIntegration::initialize(QWaylandDisplay *waylandDisplay)
{
    m_waylandDisplay = waylandDisplay->wl_display();
    waylandDisplay->addRegistryListener(wlDisplayHandleGlobal, this);
    EGLint major,minor;
    m_eglDisplay = eglGetDisplay((EGLNativeDisplayType)EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == NULL) {
        qWarning("EGL not available");
    } else {
        if (!eglInitialize(m_eglDisplay, &major, &minor)) {
            qWarning("failed to initialize EGL display");
            return;
        }

        eglFlushBRCM = (PFNEGLFLUSHBRCMPROC)eglGetProcAddress("eglFlushBRCM");
        if (!eglFlushBRCM) {
            qWarning("failed to resolve eglFlushBRCM, performance will suffer");
        }

        eglCreateGlobalImageBRCM = (PFNEGLCREATEGLOBALIMAGEBRCMPROC)eglGetProcAddress("eglCreateGlobalImageBRCM");
        if (!eglCreateGlobalImageBRCM) {
            qWarning("failed to resolve eglCreateGlobalImageBRCM");
            return;
        }

        eglDestroyGlobalImageBRCM = (PFNEGLDESTROYGLOBALIMAGEBRCMPROC)eglGetProcAddress("eglDestroyGlobalImageBRCM");
        if (!eglDestroyGlobalImageBRCM) {
            qWarning("failed to resolve eglDestroyGlobalImageBRCM");
            return;
        }
    }
}

QWaylandWindow *QWaylandBrcmEglIntegration::createEglWindow(QWindow *window)
{
    return new QWaylandBrcmEglWindow(window);
}

QPlatformOpenGLContext *QWaylandBrcmEglIntegration::createPlatformOpenGLContext(const QSurfaceFormat &glFormat, QPlatformOpenGLContext *share) const
{
    return new QWaylandBrcmGLContext(m_eglDisplay, glFormat, share);
}

EGLDisplay QWaylandBrcmEglIntegration::eglDisplay() const
{
    return m_eglDisplay;
}

}

QT_END_NAMESPACE
