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

#ifndef QWAYLANDSHELLSURFACE_H
#define QWAYLANDSHELLSURFACE_H

#include <QtWaylandCompositor/QWaylandCompositorExtension>

QT_BEGIN_NAMESPACE

class QWaylandQuickShellIntegration;
class QWaylandQuickShellSurfaceItem;
class QWaylandShellSurfacePrivate;
class QWaylandShellSurfaceTemplatePrivate;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandShellSurface : public QWaylandCompositorExtension
{
    Q_OBJECT
    Q_PROPERTY(Qt::WindowType windowType READ windowType NOTIFY windowTypeChanged)
public:
#ifdef QT_WAYLAND_COMPOSITOR_QUICK
    virtual QWaylandQuickShellIntegration *createIntegration(QWaylandQuickShellSurfaceItem *item) = 0;
#endif
    QWaylandShellSurface(QWaylandObject *waylandObject) : QWaylandCompositorExtension(waylandObject) {}
    virtual Qt::WindowType windowType() const { return Qt::WindowType::Window; }

protected:
    QWaylandShellSurface(QWaylandCompositorExtensionPrivate &dd) : QWaylandCompositorExtension(dd){}
    QWaylandShellSurface(QWaylandObject *container, QWaylandCompositorExtensionPrivate &dd) : QWaylandCompositorExtension(container, dd) {}

Q_SIGNALS:
    void windowTypeChanged();
};

template <typename T>
class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandShellSurfaceTemplate : public QWaylandShellSurface
{
public:
    QWaylandShellSurfaceTemplate(QWaylandObject *container)
        : QWaylandShellSurface(container)
    { }

    const struct wl_interface *extensionInterface() const override
    {
        return T::interface();
    }

    static T *findIn(QWaylandObject *container)
    {
        if (!container) return nullptr;
        return qobject_cast<T *>(container->extension(T::interfaceName()));
    }

protected:
    QWaylandShellSurfaceTemplate(QWaylandCompositorExtensionPrivate &dd)
        : QWaylandShellSurface(dd)
    { }

    QWaylandShellSurfaceTemplate(QWaylandObject *container, QWaylandCompositorExtensionPrivate &dd)
        : QWaylandShellSurface(container,dd)
    { }
};

QT_END_NAMESPACE

#endif // QWAYLANDSHELLSURFACE_H
