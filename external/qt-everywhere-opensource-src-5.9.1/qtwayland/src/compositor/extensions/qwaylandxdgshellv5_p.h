/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#ifndef QWAYLANDXDGSHELLV5_P_H
#define QWAYLANDXDGSHELLV5_P_H

#include <QtWaylandCompositor/private/qwaylandcompositorextension_p.h>
#include <QtWaylandCompositor/private/qwayland-server-xdg-shell.h>

#include <QtWaylandCompositor/QWaylandXdgShellV5>

#include <QtCore/QSet>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandXdgShellV5Private
        : public QWaylandCompositorExtensionPrivate
        , public QtWaylandServer::xdg_shell
{
    Q_DECLARE_PUBLIC(QWaylandXdgShellV5)
public:
    QWaylandXdgShellV5Private();
    void ping(Resource *resource, uint32_t serial);
    void registerSurface(QWaylandXdgSurfaceV5 *xdgSurface);
    void unregisterXdgSurface(QWaylandXdgSurfaceV5 *xdgSurface);
    void registerXdgPopup(QWaylandXdgPopupV5 *xdgPopup);
    void unregisterXdgPopup(QWaylandXdgPopupV5 *xdgPopup);
    static QWaylandXdgShellV5Private *get(QWaylandXdgShellV5 *xdgShell) { return xdgShell->d_func(); }
    bool isValidPopupParent(QWaylandSurface *parentSurface) const;
    QWaylandXdgPopupV5 *topmostPopupForClient(struct wl_client* client) const;

    QSet<uint32_t> m_pings;
    QMultiMap<struct wl_client *, QWaylandXdgSurfaceV5 *> m_xdgSurfaces;
    QMultiMap<struct wl_client *, QWaylandXdgPopupV5 *> m_xdgPopups;

    QWaylandXdgSurfaceV5 *xdgSurfaceFromSurface(QWaylandSurface *surface);

protected:
    void xdg_shell_destroy(Resource *resource) override;
    void xdg_shell_get_xdg_surface(Resource *resource, uint32_t id,
                                   struct ::wl_resource *surface) override;
    void xdg_shell_use_unstable_version(Resource *resource, int32_t version) override;
    void xdg_shell_get_xdg_popup(Resource *resource, uint32_t id, struct ::wl_resource *surface,
                                 struct ::wl_resource *parent, struct ::wl_resource *seatResource,
                                 uint32_t serial, int32_t x, int32_t y) override;
    void xdg_shell_pong(Resource *resource, uint32_t serial) override;
};

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandXdgSurfaceV5Private
        : public QWaylandCompositorExtensionPrivate
        , public QtWaylandServer::xdg_surface
{
    Q_DECLARE_PUBLIC(QWaylandXdgSurfaceV5)
public:
    QWaylandXdgSurfaceV5Private();
    static QWaylandXdgSurfaceV5Private *get(QWaylandXdgSurfaceV5 *xdgSurface) { return xdgSurface->d_func(); }

    struct ConfigureEvent {
        QVector<uint> states;
        QSize size;
        uint serial;
    };

    void handleFocusLost();
    void handleFocusReceived();
    QRect calculateFallbackWindowGeometry() const;
    void updateFallbackWindowGeometry();

    void setWindowType(Qt::WindowType windowType);

private:
    QWaylandXdgShellV5 *m_xdgShell;
    QWaylandSurface *m_surface;
    QWaylandXdgSurfaceV5 *m_parentSurface;

    Qt::WindowType m_windowType;

    QString m_title;
    QString m_appId;
    QRect m_windowGeometry;
    bool m_unsetWindowGeometry;

    QList<ConfigureEvent> m_pendingConfigures;
    ConfigureEvent m_lastAckedConfigure;
    ConfigureEvent lastSentConfigure() const { return m_pendingConfigures.empty() ? m_lastAckedConfigure : m_pendingConfigures.first(); }

    void xdg_surface_destroy_resource(Resource *resource) override;

    void xdg_surface_destroy(Resource *resource) override;
    void xdg_surface_move(Resource *resource, struct ::wl_resource *seat,
                          uint32_t serial) override;
    void xdg_surface_resize(Resource *resource, struct ::wl_resource *seat, uint32_t serial,
                            uint32_t edges) override;
    void xdg_surface_set_maximized(Resource *resource) override;
    void xdg_surface_unset_maximized(Resource *resource) override;
    void xdg_surface_set_fullscreen(Resource *resource,
                                    struct ::wl_resource *output) override;
    void xdg_surface_unset_fullscreen(Resource *resource) override;
    void xdg_surface_set_minimized(Resource *resource) override;
    void xdg_surface_set_parent(Resource *resource, struct ::wl_resource *parent) override;
    void xdg_surface_set_app_id(Resource *resource, const QString &app_id) override;
    void xdg_surface_show_window_menu(Resource *resource, struct ::wl_resource *seatResource,
                                      uint32_t serial, int32_t x, int32_t y) override;
    void xdg_surface_ack_configure(Resource *resource, uint32_t serial) override;
    void xdg_surface_set_title(Resource *resource, const QString &title) override;
    void xdg_surface_set_window_geometry(Resource *resource, int32_t x, int32_t y,
                                         int32_t width, int32_t height) override;

    static QWaylandSurfaceRole s_role;
};

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandXdgPopupV5Private
        : public QWaylandCompositorExtensionPrivate
        , public QtWaylandServer::xdg_popup
{
    Q_DECLARE_PUBLIC(QWaylandXdgPopupV5)

public:
    QWaylandXdgPopupV5Private();
    static QWaylandXdgPopupV5Private *get(QWaylandXdgPopupV5 *xdgPopup) { return xdgPopup->d_func(); }

    QWaylandSurface *m_surface;
    QWaylandSurface *m_parentSurface;
    QWaylandXdgShellV5 *m_xdgShell;
    QPoint m_position;

    void xdg_popup_destroy_resource(Resource *resource) override;
    void xdg_popup_destroy(xdg_popup::Resource *resource) override;

    static QWaylandSurfaceRole s_role;
};

QT_END_NAMESPACE

#endif // QWAYLANDXDGSHELL_P_H
