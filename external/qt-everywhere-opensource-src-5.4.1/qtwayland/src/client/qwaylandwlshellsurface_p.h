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

#ifndef QWAYLANDWLSHELLSURFACE_H
#define QWAYLANDWLSHELLSURFACE_H

#include <QtCore/QSize>

#include <wayland-client.h>

#include <QtWaylandClient/private/qwayland-wayland.h>
#include <QtWaylandClient/private/qwaylandclientexport_p.h>
#include "qwaylandshellsurface_p.h"

QT_BEGIN_NAMESPACE

class QWaylandWindow;
class QWaylandInputDevice;
class QWindow;
class QWaylandExtendedSurface;

class Q_WAYLAND_CLIENT_EXPORT QWaylandWlShellSurface : public QtWayland::wl_shell_surface
        , public QWaylandShellSurface
{
public:
    QWaylandWlShellSurface(struct ::wl_shell_surface *shell_surface, QWaylandWindow *window);
    virtual ~QWaylandWlShellSurface();

    using QtWayland::wl_shell_surface::resize;
    void resize(QWaylandInputDevice *inputDevice, enum wl_shell_surface_resize edges) Q_DECL_OVERRIDE;

    using QtWayland::wl_shell_surface::move;
    void move(QWaylandInputDevice *inputDevice) Q_DECL_OVERRIDE;

    void setTitle(const QString & title) Q_DECL_OVERRIDE;
    void setAppId(const QString &appId) Q_DECL_OVERRIDE;

    void raise() Q_DECL_OVERRIDE;
    void lower() Q_DECL_OVERRIDE;
    void setContentOrientationMask(Qt::ScreenOrientations orientation) Q_DECL_OVERRIDE;
    void setWindowFlags(Qt::WindowFlags flags) Q_DECL_OVERRIDE;
    void sendProperty(const QString &name, const QVariant &value) Q_DECL_OVERRIDE;

private:
    void setMaximized() Q_DECL_OVERRIDE;
    void setFullscreen() Q_DECL_OVERRIDE;
    void setNormal() Q_DECL_OVERRIDE;
    void setMinimized() Q_DECL_OVERRIDE;

    void setTopLevel() Q_DECL_OVERRIDE;
    void updateTransientParent(QWindow *parent) Q_DECL_OVERRIDE;
    void setPopup(QWaylandWindow *parent, QWaylandInputDevice *device, int serial);

    QWaylandWindow *m_window;
    bool m_maximized;
    bool m_fullscreen;
    QSize m_size;
    QWaylandExtendedSurface *m_extendedWindow;

    void shell_surface_ping(uint32_t serial) Q_DECL_OVERRIDE;
    void shell_surface_configure(uint32_t edges,
                                 int32_t width,
                                 int32_t height) Q_DECL_OVERRIDE;
    void shell_surface_popup_done() Q_DECL_OVERRIDE;

    friend class QWaylandWindow;
};

QT_END_NAMESPACE

#endif // QWAYLANDSHELLSURFACE_H
