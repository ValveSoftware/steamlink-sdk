/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Copyright (C) 2017 Eurogiciel, author: <philippe.coval@eurogiciel.fr>
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

#include "qwaylandxdgshellv6_p.h"

#include "qwaylanddisplay_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandabstractdecoration_p.h"

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandXdgSurfaceV6::Toplevel::Toplevel(QWaylandXdgSurfaceV6 *xdgSurface)
    : QtWayland::zxdg_toplevel_v6(xdgSurface->get_toplevel())
    , m_xdgSurface(xdgSurface)
{
}

QWaylandXdgSurfaceV6::Toplevel::~Toplevel()
{
    if (isInitialized())
        destroy();
}

void QWaylandXdgSurfaceV6::Toplevel::applyConfigure()
{
    //TODO: resize, activate etc
    m_xdgSurface->m_window->configure(0, m_configureState.width, m_configureState.height);
}

void QWaylandXdgSurfaceV6::Toplevel::zxdg_toplevel_v6_configure(int32_t width, int32_t height, wl_array *states)
{
    m_configureState.width = width;
    m_configureState.height = height;

    uint32_t *state = reinterpret_cast<uint32_t *>(states->data);
    size_t numStates = states->size / sizeof(uint32_t);
    m_configureState.states.reserve(numStates);
    m_configureState.states.clear();

    for (size_t i = 0; i < numStates; i++)
        m_configureState.states << state[i];
}

void QWaylandXdgSurfaceV6::Toplevel::zxdg_toplevel_v6_close()
{
    m_xdgSurface->m_window->window()->close();
}

QWaylandXdgSurfaceV6::Popup::Popup(QWaylandXdgSurfaceV6 *xdgSurface, QWaylandXdgSurfaceV6 *parent,
                                   QtWayland::zxdg_positioner_v6 *positioner)
    : zxdg_popup_v6(xdgSurface->get_popup(parent->object(), positioner->object()))
    , m_xdgSurface(xdgSurface)
{
}

QWaylandXdgSurfaceV6::Popup::~Popup()
{
    if (isInitialized())
        destroy();
}

void QWaylandXdgSurfaceV6::Popup::applyConfigure()
{

}

void QWaylandXdgSurfaceV6::Popup::zxdg_popup_v6_popup_done()
{
    m_xdgSurface->m_window->window()->close();
}

QWaylandXdgSurfaceV6::QWaylandXdgSurfaceV6(QWaylandXdgShellV6 *shell, ::zxdg_surface_v6 *surface, QWaylandWindow *window)
                    : QWaylandShellSurface(window)
                    , zxdg_surface_v6(surface)
                    , m_shell(shell)
                    , m_window(window)
                    , m_toplevel(nullptr)
                    , m_popup(nullptr)
                    , m_configured(false)
{
}

QWaylandXdgSurfaceV6::~QWaylandXdgSurfaceV6()
{
    if (m_popup)
        zxdg_popup_v6_destroy(m_popup->object());
    destroy();
}

void QWaylandXdgSurfaceV6::resize(QWaylandInputDevice *inputDevice, zxdg_toplevel_v6_resize_edge edges)
{
    Q_ASSERT(m_toplevel && m_toplevel->isInitialized());
    m_toplevel->resize(inputDevice->wl_seat(), inputDevice->serial(), edges);
}

void QWaylandXdgSurfaceV6::resize(QWaylandInputDevice *inputDevice, enum wl_shell_surface_resize edges)
{
    auto xdgEdges = reinterpret_cast<enum zxdg_toplevel_v6_resize_edge const * const>(&edges);
    resize(inputDevice, *xdgEdges);
}


void QWaylandXdgSurfaceV6::move(QWaylandInputDevice *inputDevice)
{
    Q_ASSERT(m_toplevel && m_toplevel->isInitialized());
    m_toplevel->move(inputDevice->wl_seat(), inputDevice->serial());
}

void QWaylandXdgSurfaceV6::setTitle(const QString &title)
{
    if (m_toplevel)
        m_toplevel->set_title(title);
}

void QWaylandXdgSurfaceV6::setAppId(const QString &appId)
{
    if (m_toplevel)
        m_toplevel->set_app_id(appId);
}

void QWaylandXdgSurfaceV6::setType(Qt::WindowType type, QWaylandWindow *transientParent)
{
    if (type == Qt::Popup && transientParent) {
        setPopup(transientParent, m_window->display()->lastInputDevice(), m_window->display()->lastInputSerial());
    } else {
        setToplevel();
        if (transientParent) {
            auto parentXdgSurface = static_cast<QWaylandXdgSurfaceV6 *>(transientParent->shellSurface());
            m_toplevel->set_parent(parentXdgSurface->m_toplevel->object());
        }
    }
}

bool QWaylandXdgSurfaceV6::handleExpose(const QRegion &region)
{
    if (!m_configured && !region.isEmpty()) {
        m_exposeRegion = region;
        return true;
    }
    return false;
}

void QWaylandXdgSurfaceV6::setToplevel()
{
    Q_ASSERT(!m_toplevel && !m_popup);
    m_toplevel = new Toplevel(this);
}

void QWaylandXdgSurfaceV6::setPopup(QWaylandWindow *parent, QWaylandInputDevice *device, int serial)
{
    Q_ASSERT(!m_toplevel && !m_popup);

    auto parentXdgSurface = static_cast<QWaylandXdgSurfaceV6 *>(parent->shellSurface());
    auto positioner = new QtWayland::zxdg_positioner_v6(m_shell->create_positioner());
    // set_popup expects a position relative to the parent
    QPoint transientPos = m_window->geometry().topLeft(); // this is absolute
    transientPos -= parent->geometry().topLeft();
    if (parent->decoration()) {
        transientPos.setX(transientPos.x() + parent->decoration()->margins().left());
        transientPos.setY(transientPos.y() + parent->decoration()->margins().top());
    }
    positioner->set_anchor_rect(transientPos.x(), transientPos.y(), 1, 1);
    positioner->set_anchor(QtWayland::zxdg_positioner_v6::anchor_top | QtWayland::zxdg_positioner_v6::anchor_left);
    positioner->set_gravity(QtWayland::zxdg_positioner_v6::gravity_bottom | QtWayland::zxdg_positioner_v6::gravity_right);
    positioner->set_size(m_window->geometry().width(), m_window->geometry().height());
    m_popup = new Popup(this, parentXdgSurface, positioner);
    positioner->destroy();
    delete positioner;
    m_popup->grab(device->wl_seat(), serial);
}

void QWaylandXdgSurfaceV6::zxdg_surface_v6_configure(uint32_t serial)
{
    m_configured = true;
    if (m_toplevel)
        m_toplevel->applyConfigure();
    else if (m_popup)
        m_popup->applyConfigure();

    if (!m_exposeRegion.isEmpty()) {
        QWindowSystemInterface::handleExposeEvent(m_window->window(), m_exposeRegion);
        m_exposeRegion = QRegion();
    }
    ack_configure(serial);
}



QWaylandXdgShellV6::QWaylandXdgShellV6(struct ::wl_registry *registry, uint32_t id, uint32_t availableVersion)
                  : QtWayland::zxdg_shell_v6(registry, id, qMin(availableVersion, 1u))
{
}

QWaylandXdgShellV6::~QWaylandXdgShellV6()
{
    destroy();
}

QWaylandXdgSurfaceV6 *QWaylandXdgShellV6::getXdgSurface(QWaylandWindow *window)
{
    return new QWaylandXdgSurfaceV6(this, get_xdg_surface(window->object()), window);
}

void QWaylandXdgShellV6::zxdg_shell_v6_ping(uint32_t serial)
{
    pong(serial);
}

}

QT_END_NAMESPACE
