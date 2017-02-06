/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWAYLANDWLSHELL_P_H
#define QWAYLANDWLSHELL_P_H

#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>
#include <QtWaylandCompositor/qwaylandsurface.h>
#include <QtWaylandCompositor/private/qwaylandcompositorextension_p.h>
#include <QtWaylandCompositor/QWaylandWlShellSurface>
#include <QtWaylandCompositor/QWaylandSeat>

#include <wayland-server.h>
#include <QHash>
#include <QPoint>
#include <QSet>

#include <QtWaylandCompositor/private/qwayland-server-wayland.h>

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

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandWlShellPrivate
                                        : public QWaylandCompositorExtensionPrivate
                                        , public QtWaylandServer::wl_shell
{
    Q_DECLARE_PUBLIC(QWaylandWlShell)
public:
    QWaylandWlShellPrivate();

    void unregisterShellSurface(QWaylandWlShellSurface *shellSurface);

    static QWaylandWlShellPrivate *get(QWaylandWlShell *shell) { return shell->d_func(); }

protected:
    void shell_get_shell_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) Q_DECL_OVERRIDE;

    QList<QWaylandWlShellSurface *> m_shellSurfaces;
};

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandWlShellSurfacePrivate
                                        : public QWaylandCompositorExtensionPrivate
                                        , public QtWaylandServer::wl_shell_surface
{
    Q_DECLARE_PUBLIC(QWaylandWlShellSurface)
public:
    QWaylandWlShellSurfacePrivate();
    ~QWaylandWlShellSurfacePrivate();

    static QWaylandWlShellSurfacePrivate *get(QWaylandWlShellSurface *surface) { return surface->d_func(); }

    void ping(uint32_t serial);

    void setWindowType(Qt::WindowType windowType);

private:
    QWaylandWlShell *m_shell;
    QPointer<QWaylandSurface> m_surface;

    QSet<uint32_t> m_pings;

    QString m_title;
    QString m_className;
    Qt::WindowType m_windowType;

    void shell_surface_destroy_resource(Resource *resource) Q_DECL_OVERRIDE;

    void shell_surface_move(Resource *resource,
                            struct wl_resource *input_device_super,
                            uint32_t time) Q_DECL_OVERRIDE;
    void shell_surface_resize(Resource *resource,
                              struct wl_resource *input_device,
                              uint32_t time,
                              uint32_t edges) Q_DECL_OVERRIDE;
    void shell_surface_set_toplevel(Resource *resource) Q_DECL_OVERRIDE;
    void shell_surface_set_transient(Resource *resource,
                                     struct wl_resource *parent_surface_resource,
                                     int x,
                                     int y,
                                     uint32_t flags) Q_DECL_OVERRIDE;
    void shell_surface_set_fullscreen(Resource *resource,
                                      uint32_t method,
                                      uint32_t framerate,
                                      struct wl_resource *output_resource) Q_DECL_OVERRIDE;
    void shell_surface_set_popup(Resource *resource,
                                 struct wl_resource *input_device,
                                 uint32_t time,
                                 struct wl_resource *parent,
                                 int32_t x,
                                 int32_t y,
                                 uint32_t flags) Q_DECL_OVERRIDE;
    void shell_surface_set_maximized(Resource *resource,
                                     struct wl_resource *output_resource) Q_DECL_OVERRIDE;
    void shell_surface_pong(Resource *resource,
                            uint32_t serial) Q_DECL_OVERRIDE;
    void shell_surface_set_title(Resource *resource,
                                 const QString &title) Q_DECL_OVERRIDE;
    void shell_surface_set_class(Resource *resource,
                                 const QString &class_) Q_DECL_OVERRIDE;

    static QWaylandSurfaceRole s_role;
};

QT_END_NAMESPACE

#endif // QWAYLANDWLSHELL_P_H
