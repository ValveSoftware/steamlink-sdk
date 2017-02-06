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

#ifndef QWAYLANDIVISURFACE_H
#define QWAYLANDIVISURFACE_H

#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include "qwayland-ivi-application.h"
#include "qwayland-ivi-controller.h"

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class QWaylandWindow;
class QWaylandInputDevice;
class QWindow;
class QWaylandExtendedSurface;

class Q_WAYLAND_CLIENT_EXPORT QWaylandIviSurface : public QtWayland::ivi_surface
        , public QWaylandShellSurface, public QtWayland::ivi_controller_surface
{
public:
    QWaylandIviSurface(struct ::ivi_surface *shell_surface, QWaylandWindow *window);
    QWaylandIviSurface(struct ::ivi_surface *shell_surface, QWaylandWindow *window,
                       struct ::ivi_controller_surface *iviControllerSurface);
    virtual ~QWaylandIviSurface();

    void setType(Qt::WindowType type, QWaylandWindow *transientParent) override;

private:
    void createExtendedSurface(QWaylandWindow *window);
    virtual void ivi_surface_configure(int32_t width, int32_t height) Q_DECL_OVERRIDE;

    QWaylandWindow *m_window;
    QWaylandExtendedSurface *m_extendedWindow;
};

}

QT_END_NAMESPACE

#endif // QWAYLANDIVISURFACE_H
