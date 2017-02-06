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

#ifndef QWAYLANDBRCMEGLINTEGRATION_H
#define QWAYLANDBRCMEGLINTEGRATION_H

#include <QtWaylandClient/private/qwaylandclientbufferintegration_p.h>
#include <wayland-client.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <EGL/eglext_brcm.h>

#include <QtCore/qglobal.h>

struct qt_brcm;

QT_BEGIN_NAMESPACE

class QWindow;

namespace QtWaylandClient {

class QWaylandWindow;

class QWaylandBrcmEglIntegration : public QWaylandClientBufferIntegration
{
public:
    QWaylandBrcmEglIntegration();
    ~QWaylandBrcmEglIntegration();

    void initialize(QWaylandDisplay *waylandDisplay) Q_DECL_OVERRIDE;

    bool supportsThreadedOpenGL() const Q_DECL_OVERRIDE { return true; }
    bool supportsWindowDecoration() const Q_DECL_OVERRIDE { return false; }

    QWaylandWindow *createEglWindow(QWindow *window);
    QPlatformOpenGLContext *createPlatformOpenGLContext(const QSurfaceFormat &glFormat, QPlatformOpenGLContext *share) const Q_DECL_OVERRIDE;

    EGLDisplay eglDisplay() const;

    struct qt_brcm *waylandBrcm() const;

    PFNEGLFLUSHBRCMPROC eglFlushBRCM;
    PFNEGLCREATEGLOBALIMAGEBRCMPROC eglCreateGlobalImageBRCM;
    PFNEGLDESTROYGLOBALIMAGEBRCMPROC eglDestroyGlobalImageBRCM;

private:
    static void wlDisplayHandleGlobal(void *data, struct wl_registry *registry, uint32_t id, const QString &interface, uint32_t version);

    struct wl_display *m_waylandDisplay;
    struct qt_brcm *m_waylandBrcm;

    EGLDisplay m_eglDisplay;
};

}

QT_END_NAMESPACE

#endif // QWAYLANDBRCMEGLINTEGRATION_H
