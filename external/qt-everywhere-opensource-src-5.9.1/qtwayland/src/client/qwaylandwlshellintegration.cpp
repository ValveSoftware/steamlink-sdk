/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qwaylandwlshellintegration_p.h"

#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandwlshellsurface_p.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandWlShellIntegration *QWaylandWlShellIntegration::create(QWaylandDisplay *display)
{
    if (display->hasRegistryGlobal(QLatin1String("wl_shell")))
        return new QWaylandWlShellIntegration(display);
    return nullptr;
}

QWaylandWlShellIntegration::QWaylandWlShellIntegration(QWaylandDisplay *display)
    : m_wlShell(Q_NULLPTR)
{
    Q_FOREACH (QWaylandDisplay::RegistryGlobal global, display->globals()) {
        if (global.interface == QLatin1String("wl_shell")) {
            m_wlShell = new QtWayland::wl_shell(display->wl_registry(), global.id, 1);
            break;
        }
    }
}

bool QWaylandWlShellIntegration::initialize(QWaylandDisplay *display)
{
    QWaylandShellIntegration::initialize(display);
    return m_wlShell != nullptr;
};

QWaylandShellSurface *QWaylandWlShellIntegration::createShellSurface(QWaylandWindow *window)
{
    return new QWaylandWlShellSurface(m_wlShell->get_shell_surface(window->object()), window);
}

}

QT_END_NAMESPACE
