/****************************************************************************
**
** Copyright (C) 2015 ITAGE Corporation, author: <yusuke.binsaki@itage.co.jp>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWAYLANDIVIINTEGRATION_H
#define QWAYLANDIVIINTEGRATION_H

#include <QtCore/qmutex.h>

#include <QtWaylandClient/private/qwaylandshellintegration_p.h>
#include "qwayland-ivi-application.h"
#include "qwayland-ivi-controller.h"

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class QWaylandWindow;
class QWaylandDisplay;
class QWaylandIviControllerSurface;

class Q_WAYLAND_CLIENT_EXPORT QWaylandIviShellIntegration : public QWaylandShellIntegration
{
public:
    QWaylandIviShellIntegration();
    ~QWaylandIviShellIntegration();
    bool initialize(QWaylandDisplay *display);
    virtual QWaylandShellSurface *createShellSurface(QWaylandWindow *window) Q_DECL_OVERRIDE;

private:
    static void registryIvi(void *data, struct wl_registry *registry,
                            uint32_t id, const QString &interface, uint32_t version);
    uint32_t getNextUniqueSurfaceId();

private:
    QtWayland::ivi_application *m_iviApplication;
    QtWayland::ivi_controller *m_iviController;
    uint32_t m_lastSurfaceId;
    uint32_t m_surfaceNumber;
    bool m_useEnvSurfaceId;
    QMutex m_mutex;
};

}

QT_END_NAMESPACE

#endif //  QWAYLANDIVIINTEGRATION_H
