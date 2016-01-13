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

#ifndef QWAYLANDSHELLSURFACE_H
#define QWAYLANDSHELLSURFACE_H

#include <QtCore/QSize>

#include <wayland-client.h>

#include <QtWaylandClient/private/qwayland-wayland.h>
#include <QtWaylandClient/private/qwaylandclientexport_p.h>

QT_BEGIN_NAMESPACE

class QVariant;
class QWaylandWindow;
class QWaylandInputDevice;
class QWindow;

class Q_WAYLAND_CLIENT_EXPORT QWaylandShellSurface
{
public:
    explicit QWaylandShellSurface(QWaylandWindow *window);
    virtual ~QWaylandShellSurface() {}
    virtual void resize(QWaylandInputDevice * /*inputDevice*/, enum wl_shell_surface_resize /*edges*/)
    {}

    virtual void move(QWaylandInputDevice * /*inputDevice*/) {}
    virtual void setTitle(const QString & /*title*/) {}
    virtual void setAppId(const QString & /*appId*/) {}

    virtual void setWindowFlags(Qt::WindowFlags flags);

    virtual bool isExposed() const { return true; }

    virtual void raise() {}
    virtual void lower() {}
    virtual void setContentOrientationMask(Qt::ScreenOrientations orientation) { Q_UNUSED(orientation) }

    virtual void sendProperty(const QString &name, const QVariant &value);

    inline QWaylandWindow *window() { return m_window; }

protected:
    virtual void setMaximized() {}
    virtual void setFullscreen() {}
    virtual void setNormal() {}
    virtual void setMinimized() {}

    virtual void setTopLevel() {}
    virtual void updateTransientParent(QWindow * /*parent*/) {}

private:
    QWaylandWindow *m_window;
    friend class QWaylandWindow;
};

QT_END_NAMESPACE

#endif // QWAYLANDSHELLSURFACE_H
