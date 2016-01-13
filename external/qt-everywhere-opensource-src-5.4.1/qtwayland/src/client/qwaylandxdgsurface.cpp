/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the config.tests of the Qt Toolkit.
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

#include "qwaylandxdgsurface_p.h"

#include "qwaylanddisplay_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandabstractdecoration_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandextendedsurface_p.h"

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QWaylandXdgSurface::QWaylandXdgSurface(struct ::xdg_surface *xdg_surface, QWaylandWindow *window)
    : QtWayland::xdg_surface(xdg_surface)
    , QWaylandShellSurface(window)
    , m_window(window)
    , m_maximized(false)
    , m_minimized(false)
    , m_fullscreen(false)
    , m_extendedWindow(Q_NULLPTR)
{
    if (window->display()->windowExtension())
        m_extendedWindow = new QWaylandExtendedSurface(window);
}

QWaylandXdgSurface::~QWaylandXdgSurface()
{
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
        request_change_state(XDG_SURFACE_STATE_MAXIMIZED, true, 0);
}

void QWaylandXdgSurface::setFullscreen()
{
    if (!m_fullscreen)
        request_change_state(XDG_SURFACE_STATE_FULLSCREEN, true, 0);
}

void QWaylandXdgSurface::setNormal()
{
    if (m_fullscreen || m_maximized  || m_minimized) {
        if (m_maximized) {
            request_change_state(XDG_SURFACE_STATE_MAXIMIZED, false, 0);
        }
        if (m_fullscreen) {
            request_change_state(XDG_SURFACE_STATE_FULLSCREEN, false, 0);
        }

        m_fullscreen = m_maximized = m_minimized = false;
        setTopLevel();
        QMargins m = m_window->frameMargins();
        m_window->configure(0, m_size.width() + m.left() + m.right(), m_size.height() + m.top() + m.bottom());
    }
}

void QWaylandXdgSurface::setMinimized()
{
    m_minimized = true;
    m_size = m_window->window()->geometry().size();
    set_minimized();
}

void QWaylandXdgSurface::setTopLevel()
{
    // There's no xdg_shell_surface API for this, ignoring
}

void QWaylandXdgSurface::updateTransientParent(QWindow *parent)
{
    QWaylandWindow *parent_wayland_window = static_cast<QWaylandWindow *>(parent->handle());
    if (!parent_wayland_window)
        return;

    // set_transient expects a position relative to the parent
    QPoint transientPos = m_window->geometry().topLeft(); // this is absolute
    QWindow *parentWin = m_window->window()->transientParent();
    transientPos -= parentWin->geometry().topLeft();
    if (parent_wayland_window->decoration()) {
        transientPos.setX(transientPos.x() + parent_wayland_window->decoration()->margins().left());
        transientPos.setY(transientPos.y() + parent_wayland_window->decoration()->margins().top());
    }

    uint32_t flags = 0;
    Qt::WindowFlags wf = m_window->window()->flags();
    if (wf.testFlag(Qt::ToolTip)
            || wf.testFlag(Qt::WindowTransparentForInput))
        flags |= XDG_SURFACE_SET_TRANSIENT_FOR;

    set_transient_for(parent_wayland_window->object());
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

void QWaylandXdgSurface::xdg_surface_configure(int32_t width, int32_t height)
{
    m_window->configure(0 , width, height);
}

void QWaylandXdgSurface::xdg_surface_change_state(uint32_t state,
                                                  uint32_t value,
                                                  uint32_t serial)
{

    if (state == XDG_SURFACE_STATE_MAXIMIZED
            || state == XDG_SURFACE_STATE_FULLSCREEN) {
        if (value) {
            m_size = m_window->window()->geometry().size();
        } else {
            QMargins m = m_window->frameMargins();
            m_window->configure(0, m_size.width() + m.left() + m.right(), m_size.height() + m.top() + m.bottom());
        }
    }

    switch (state) {
    case XDG_SURFACE_STATE_MAXIMIZED:
        m_maximized = value;
        break;
    case XDG_SURFACE_STATE_FULLSCREEN:
        m_fullscreen = value;
        break;
    }

    xdg_surface_ack_change_state(object(), state, value, serial);
}

void QWaylandXdgSurface::xdg_surface_activated()
{
}

void QWaylandXdgSurface::xdg_surface_deactivated()
{
}

void QWaylandXdgSurface::xdg_surface_close()
{
}

QT_END_NAMESPACE
