/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QWAYLANDIVISURFACE_H
#define QWAYLANDIVISURFACE_H

#include <QtWaylandCompositor/QWaylandShellSurface>

class QWaylandIviSurfacePrivate;
class QWaylandSurface;
class QWaylandIviApplication;
class QWaylandSurfaceRole;
class QWaylandResource;
class wl_resource;

QT_BEGIN_NAMESPACE

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandIviSurface : public QWaylandShellSurfaceTemplate<QWaylandIviSurface>
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandIviSurface)
    Q_PROPERTY(QWaylandSurface *surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(uint iviId READ iviId NOTIFY iviIdChanged)

public:
    QWaylandIviSurface();
    QWaylandIviSurface(QWaylandIviApplication *application, QWaylandSurface *surface, uint iviId, const QWaylandResource &resource);

    Q_INVOKABLE void initialize(QWaylandIviApplication *iviApplication, QWaylandSurface *surface,
                                uint iviId, const QWaylandResource &resource);

    QWaylandSurface *surface() const;
    uint iviId() const;

    static const struct wl_interface *interface();
    static QByteArray interfaceName();
    static QWaylandSurfaceRole *role();
    static QWaylandIviSurface *fromResource(::wl_resource *resource);

    Q_INVOKABLE void sendConfigure(const QSize &size);

#ifdef QT_WAYLAND_COMPOSITOR_QUICK
    QWaylandQuickShellIntegration *createIntegration(QWaylandQuickShellSurfaceItem *item) Q_DECL_OVERRIDE;
#endif

Q_SIGNALS:
    void surfaceChanged();
    void iviIdChanged();

private:
    void initialize() Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif // QWAYLANDIVISURFACE_H
