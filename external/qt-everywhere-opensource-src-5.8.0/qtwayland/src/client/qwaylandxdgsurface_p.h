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

#ifndef QWAYLANDXDGSURFACE_H
#define QWAYLANDXDGSURFACE_H

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

#include <QtCore/QSize>
#include <QtCore/QMargins>

#include <wayland-client.h>

#include <QtWaylandClient/qtwaylandclientglobal.h>
#include <QtWaylandClient/private/qwayland-xdg-shell.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>

QT_BEGIN_NAMESPACE

class QWindow;

namespace QtWaylandClient {

class QWaylandWindow;
class QWaylandInputDevice;
class QWaylandExtendedSurface;
class QWaylandXdgShell;

class Q_WAYLAND_CLIENT_EXPORT QWaylandXdgSurface : public QWaylandShellSurface
        , public QtWayland::xdg_surface
{
    Q_OBJECT
public:
    QWaylandXdgSurface(QWaylandXdgShell *shell, QWaylandWindow *window);
    virtual ~QWaylandXdgSurface();

    using QtWayland::xdg_surface::resize;
    void resize(QWaylandInputDevice *inputDevice, enum resize_edge edges);

    void resize(QWaylandInputDevice *inputDevice, enum wl_shell_surface_resize edges) Q_DECL_OVERRIDE;

    using QtWayland::xdg_surface::move;
    void move(QWaylandInputDevice *inputDevice) Q_DECL_OVERRIDE;

    void setTitle(const QString &title) Q_DECL_OVERRIDE;
    void setAppId(const QString &appId) Q_DECL_OVERRIDE;

    void raise() Q_DECL_OVERRIDE;
    void lower() Q_DECL_OVERRIDE;
    void setContentOrientationMask(Qt::ScreenOrientations orientation) Q_DECL_OVERRIDE;
    void setWindowFlags(Qt::WindowFlags flags) Q_DECL_OVERRIDE;
    void sendProperty(const QString &name, const QVariant &value) Q_DECL_OVERRIDE;

    bool isFullscreen() const { return m_fullscreen; }
    bool isMaximized() const { return m_maximized; }

    void setType(Qt::WindowType type, QWaylandWindow *transientParent) override;

private:
    void setMaximized() Q_DECL_OVERRIDE;
    void setFullscreen() Q_DECL_OVERRIDE;
    void setNormal() Q_DECL_OVERRIDE;
    void setMinimized() Q_DECL_OVERRIDE;

    void updateTransientParent(QWaylandWindow *parent);

private:
    QWaylandWindow *m_window;
    QWaylandXdgShell* m_shell;
    bool m_maximized;
    bool m_minimized;
    bool m_fullscreen;
    bool m_active;
    QSize m_normalSize;
    QMargins m_margins;
    QWaylandExtendedSurface *m_extendedWindow;

    void xdg_surface_configure(int32_t width,
                               int32_t height,
                               struct wl_array *states,
                               uint32_t serial) Q_DECL_OVERRIDE;
    void xdg_surface_close() Q_DECL_OVERRIDE;

    friend class QWaylandWindow;
};

QT_END_NAMESPACE

}

#endif // QWAYLANDXDGSURFACE_H
