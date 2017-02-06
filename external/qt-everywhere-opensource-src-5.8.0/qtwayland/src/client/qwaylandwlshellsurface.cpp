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

#include "qwaylandwlshellsurface_p.h"

#include "qwaylanddisplay_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandabstractdecoration_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandextendedsurface_p.h"

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandWlShellSurface::QWaylandWlShellSurface(struct ::wl_shell_surface *shell_surface, QWaylandWindow *window)
    : QWaylandShellSurface(window)
    , QtWayland::wl_shell_surface(shell_surface)
    , m_window(window)
    , m_maximized(false)
    , m_fullscreen(false)
    , m_extendedWindow(Q_NULLPTR)
{
    if (window->display()->windowExtension())
        m_extendedWindow = new QWaylandExtendedSurface(window);
}

QWaylandWlShellSurface::~QWaylandWlShellSurface()
{
    wl_shell_surface_destroy(object());
    delete m_extendedWindow;
}

void QWaylandWlShellSurface::resize(QWaylandInputDevice *inputDevice, enum wl_shell_surface_resize edges)
{
    resize(inputDevice->wl_seat(),
           inputDevice->serial(),
           edges);
}

void QWaylandWlShellSurface::move(QWaylandInputDevice *inputDevice)
{
    move(inputDevice->wl_seat(),
         inputDevice->serial());
}

void QWaylandWlShellSurface::setTitle(const QString & title)
{
    return QtWayland::wl_shell_surface::set_title(title);
}

void QWaylandWlShellSurface::setAppId(const QString & appId)
{
    return QtWayland::wl_shell_surface::set_class(appId);
}

void QWaylandWlShellSurface::raise()
{
    if (m_extendedWindow)
        m_extendedWindow->raise();
}

void QWaylandWlShellSurface::lower()
{
    if (m_extendedWindow)
        m_extendedWindow->lower();
}

void QWaylandWlShellSurface::setContentOrientationMask(Qt::ScreenOrientations orientation)
{
    if (m_extendedWindow)
        m_extendedWindow->setContentOrientationMask(orientation);
}

void QWaylandWlShellSurface::setWindowFlags(Qt::WindowFlags flags)
{
    if (m_extendedWindow)
        m_extendedWindow->setWindowFlags(flags);
}

void QWaylandWlShellSurface::sendProperty(const QString &name, const QVariant &value)
{
    if (m_extendedWindow)
        m_extendedWindow->updateGenericProperty(name, value);
}

void QWaylandWlShellSurface::setMaximized()
{
    m_maximized = true;
    m_size = m_window->window()->geometry().size();
    set_maximized(0);
}

void QWaylandWlShellSurface::setFullscreen()
{
    m_fullscreen = true;
    m_size = m_window->window()->geometry().size();
    set_fullscreen(WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, 0);
}

void QWaylandWlShellSurface::setNormal()
{
    if (m_fullscreen || m_maximized) {
        m_fullscreen = m_maximized = false;
        setTopLevel();
        QMargins m = m_window->frameMargins();
        m_window->configure(0, m_size.width() + m.left() + m.right(), m_size.height() + m.top() + m.bottom());
    }
}

void QWaylandWlShellSurface::setMinimized()
{
    // TODO: There's no wl_shell_surface API for this
}

void QWaylandWlShellSurface::setTopLevel()
{
    set_toplevel();
}

static inline bool testShowWithoutActivating(const QWindow *window)
{
    // QWidget-attribute Qt::WA_ShowWithoutActivating.
    const QVariant showWithoutActivating = window->property("_q_showWithoutActivating");
    return showWithoutActivating.isValid() && showWithoutActivating.toBool();
}

void QWaylandWlShellSurface::updateTransientParent(QWindow *parent)
{
    QWaylandWindow *parent_wayland_window = static_cast<QWaylandWindow *>(parent->handle());
    if (!parent_wayland_window)
        return;

    // set_transient expects a position relative to the parent
    QPoint transientPos = m_window->geometry().topLeft(); // this is absolute
    transientPos -= parent->geometry().topLeft();
    if (parent_wayland_window->decoration()) {
        transientPos.setX(transientPos.x() + parent_wayland_window->decoration()->margins().left());
        transientPos.setY(transientPos.y() + parent_wayland_window->decoration()->margins().top());
    }

    uint32_t flags = 0;
    Qt::WindowFlags wf = m_window->window()->flags();
    if (wf.testFlag(Qt::ToolTip)
            || wf.testFlag(Qt::WindowTransparentForInput)
            || testShowWithoutActivating(m_window->window()))
        flags |= WL_SHELL_SURFACE_TRANSIENT_INACTIVE;

    set_transient(parent_wayland_window->object(),
                  transientPos.x(),
                  transientPos.y(),
                  flags);
}

void QWaylandWlShellSurface::setPopup(QWaylandWindow *parent, QWaylandInputDevice *device, int serial)
{
    QWaylandWindow *parent_wayland_window = parent;
    if (!parent_wayland_window) {
        qWarning("setPopup called without parent window");
        return;
    }
    if (!device) {
        qWarning("setPopup called without input device");
        return;
    }

    // set_popup expects a position relative to the parent
    QPoint transientPos = m_window->geometry().topLeft(); // this is absolute
    transientPos -= parent_wayland_window->geometry().topLeft();
    if (parent_wayland_window->decoration()) {
        transientPos.setX(transientPos.x() + parent_wayland_window->decoration()->margins().left());
        transientPos.setY(transientPos.y() + parent_wayland_window->decoration()->margins().top());
    }

    set_popup(device->wl_seat(), serial, parent_wayland_window->object(),
              transientPos.x(), transientPos.y(), 0);
}

void QWaylandWlShellSurface::setType(Qt::WindowType type, QWaylandWindow *transientParent)
{
    if (type == Qt::Popup && transientParent)
        setPopup(transientParent, m_window->display()->lastInputDevice(), m_window->display()->lastInputSerial());
    else if (transientParent)
        updateTransientParent(transientParent->window());
    else
        setTopLevel();
}

void QWaylandWlShellSurface::shell_surface_ping(uint32_t serial)
{
    pong(serial);
}

void QWaylandWlShellSurface::shell_surface_configure(uint32_t edges,
                                                     int32_t width,
                                                     int32_t height)
{
    m_window->configure(edges, width, height);
}

void QWaylandWlShellSurface::shell_surface_popup_done()
{
    QCoreApplication::postEvent(m_window->window(), new QCloseEvent());
}

}

QT_END_NAMESPACE
