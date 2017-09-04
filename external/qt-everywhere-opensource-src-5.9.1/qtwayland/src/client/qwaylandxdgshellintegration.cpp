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

#include "qwaylandxdgshellintegration_p.h"

#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandxdgsurface_p.h>
#include <QtWaylandClient/private/qwaylandxdgpopup_p.h>
#include <QtWaylandClient/private/qwaylandxdgshell_p.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandXdgShellIntegration *QWaylandXdgShellIntegration::create(QWaylandDisplay *display)
{
    if (display->hasRegistryGlobal(QLatin1String("xdg_shell")))
        return new QWaylandXdgShellIntegration(display);
    return nullptr;
}

QWaylandXdgShellIntegration::QWaylandXdgShellIntegration(QWaylandDisplay *display)
    : m_xdgShell(Q_NULLPTR)
{
    Q_FOREACH (QWaylandDisplay::RegistryGlobal global, display->globals()) {
        if (global.interface == QLatin1String("xdg_shell")) {
            m_xdgShell = new QWaylandXdgShell(display->wl_registry(), global.id);
            break;
        }
    }
}

bool QWaylandXdgShellIntegration::initialize(QWaylandDisplay *display)
{
    QWaylandShellIntegration::initialize(display);
    return m_xdgShell != nullptr;
}

QWaylandShellSurface *QWaylandXdgShellIntegration::createShellSurface(QWaylandWindow *window)
{
    if (window->window()->type() == Qt::WindowType::Popup)
        return m_xdgShell->createXdgPopup(window);
    else
        return m_xdgShell->createXdgSurface(window);
}

void QWaylandXdgShellIntegration::handleKeyboardFocusChanged(QWaylandWindow *newFocus, QWaylandWindow *oldFocus) {
    if (newFocus && qobject_cast<QWaylandXdgPopup *>(newFocus->shellSurface()))
        m_display->handleWindowActivated(newFocus);
    if (oldFocus && qobject_cast<QWaylandXdgPopup *>(oldFocus->shellSurface()))
        m_display->handleWindowDeactivated(oldFocus);
}

}

QT_END_NAMESPACE
