/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the config.tests of the Qt Toolkit.
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

#include "qwaylandxdgsurface_p.h"

#include "qwaylanddisplay_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandabstractdecoration_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandextendedsurface_p.h"
#include "qwaylandxdgshell_p.h"


QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandXdgSurface::QWaylandXdgSurface(QWaylandXdgShell *shell, QWaylandWindow *window)
    : QWaylandShellSurface(window)
    , QtWayland::xdg_surface(shell->get_xdg_surface(window->object()))
    , m_window(window)
    , m_shell(shell)
    , m_maximized(false)
    , m_minimized(false)
    , m_fullscreen(false)
    , m_active(false)
    , m_extendedWindow(Q_NULLPTR)
{
    if (window->display()->windowExtension())
        m_extendedWindow = new QWaylandExtendedSurface(window);
}

QWaylandXdgSurface::~QWaylandXdgSurface()
{
    if (m_active)
        window()->display()->handleWindowDeactivated(m_window);

    xdg_surface_destroy(object());
    delete m_extendedWindow;
}

void QWaylandXdgSurface::resize(QWaylandInputDevice *inputDevice, enum wl_shell_surface_resize edges)
{
    // May need some conversion if types get incompatibles, ATM they're identical
    enum resize_edge const * const arg = reinterpret_cast<enum resize_edge const * const>(&edges);
    resize(inputDevice, *arg);
}

void QWaylandXdgSurface::resize(QWaylandInputDevice *inputDevice, enum resize_edge edges)
{
    resize(inputDevice->wl_seat(),
           inputDevice->serial(),
           edges);
}

void QWaylandXdgSurface::move(QWaylandInputDevice *inputDevice)
{
    move(inputDevice->wl_seat(),
         inputDevice->serial());
}

void QWaylandXdgSurface::setMaximized()
{
    if (!m_maximized)
        set_maximized();
}

void QWaylandXdgSurface::setFullscreen()
{
    if (!m_fullscreen)
        set_fullscreen(Q_NULLPTR);
}

void QWaylandXdgSurface::setNormal()
{
    if (m_fullscreen || m_maximized  || m_minimized) {
        if (m_maximized) {
            unset_maximized();
        }
        if (m_fullscreen) {
            unset_fullscreen();
        }

        m_fullscreen = m_maximized = m_minimized = false;
    }
}

void QWaylandXdgSurface::setMinimized()
{
    m_minimized = true;
    set_minimized();
}

void QWaylandXdgSurface::updateTransientParent(QWaylandWindow *parent)
{
    if (!parent)
        return;
    auto parentXdgSurface = qobject_cast<QWaylandXdgSurface *>(parent->shellSurface());
    Q_ASSERT(parentXdgSurface);
    set_parent(parentXdgSurface->object());
}

void QWaylandXdgSurface::setTitle(const QString & title)
{
    return QtWayland::xdg_surface::set_title(title);
}

void QWaylandXdgSurface::setAppId(const QString & appId)
{
    return QtWayland::xdg_surface::set_app_id(appId);
}

void QWaylandXdgSurface::raise()
{
    if (m_extendedWindow)
        m_extendedWindow->raise();
}

void QWaylandXdgSurface::lower()
{
    if (m_extendedWindow)
        m_extendedWindow->lower();
}

void QWaylandXdgSurface::setContentOrientationMask(Qt::ScreenOrientations orientation)
{
    if (m_extendedWindow)
        m_extendedWindow->setContentOrientationMask(orientation);
}

void QWaylandXdgSurface::setWindowFlags(Qt::WindowFlags flags)
{
    if (m_extendedWindow)
        m_extendedWindow->setWindowFlags(flags);
}

void QWaylandXdgSurface::sendProperty(const QString &name, const QVariant &value)
{
    if (m_extendedWindow)
        m_extendedWindow->updateGenericProperty(name, value);
}

void QWaylandXdgSurface::setType(Qt::WindowType type, QWaylandWindow *transientParent)
{
    Q_UNUSED(type)
    if (transientParent)
        updateTransientParent(transientParent);
}

void QWaylandXdgSurface::xdg_surface_configure(int32_t width, int32_t height, struct wl_array *states,uint32_t serial)
{
    uint32_t *state = reinterpret_cast<uint32_t*>(states->data);
    size_t numStates = states->size / sizeof(uint32_t);
    bool aboutToMaximize = false;
    bool aboutToFullScreen = false;
    bool aboutToActivate = false;

    for (size_t i = 0; i < numStates; i++) {
        switch (state[i]) {
        case XDG_SURFACE_STATE_MAXIMIZED:
            aboutToMaximize = ((width > 0) && (height > 0));
            break;
        case XDG_SURFACE_STATE_FULLSCREEN:
            aboutToFullScreen = true;
            break;
        case XDG_SURFACE_STATE_RESIZING:
            m_normalSize = QSize(width, height);
            break;
        case XDG_SURFACE_STATE_ACTIVATED:
            aboutToActivate = true;
            break;
        default:
            break;
        }
    }

    if (!m_active && aboutToActivate) {
        m_active = true;
        window()->display()->handleWindowActivated(m_window);
    } else if (m_active && !aboutToActivate) {
        m_active = false;
        window()->display()->handleWindowDeactivated(m_window);
    }

    if (!m_fullscreen && aboutToFullScreen) {
        if (!m_maximized)
            m_normalSize = m_window->window()->frameGeometry().size();
        m_fullscreen = true;
        m_window->window()->showFullScreen();
    } else if (m_fullscreen && !aboutToFullScreen) {
        m_fullscreen = false;
        if ( m_maximized ) {
            m_window->window()->showMaximized();
        } else {
            m_window->window()->showNormal();
        }
    } else if (!m_maximized && aboutToMaximize) {
        if (!m_fullscreen)
            m_normalSize = m_window->window()->frameGeometry().size();
        m_maximized = true;
        m_window->window()->showMaximized();
    } else if (m_maximized && !aboutToMaximize) {
        m_maximized = false;
        m_window->window()->showNormal();
    }

    if (width <= 0 || height <= 0) {
        if (!m_normalSize.isEmpty())
            m_window->configure(0, m_normalSize.width(), m_normalSize.height());
    } else {
        m_window->configure(0, width, height);
    }

    ack_configure(serial);
}

void QWaylandXdgSurface::xdg_surface_close()
{
}

}

QT_END_NAMESPACE
