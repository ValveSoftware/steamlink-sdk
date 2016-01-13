/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
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

        eglCreateGlobalImageBRCM = ::eglCreateGlobalImageBRCM;
        if (!eglCreateGlobalImageBRCM) {
            qWarning("failed to resolve eglCreateGlobalImageBRCM");
            return;
        }

        eglDestroyGlobalImageBRCM = ::eglDestroyGlobalImageBRCM;
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

QT_END_NAMESPACE
