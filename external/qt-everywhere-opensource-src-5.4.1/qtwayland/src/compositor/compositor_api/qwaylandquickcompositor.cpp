/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#include <QQuickWindow>

#include <QtCompositor/private/qwlcompositor_p.h>

#include "qwaylandquickcompositor.h"
#include "qwaylandquicksurface.h"
#include "qwaylandsurfaceitem.h"

QT_BEGIN_NAMESPACE

class QWaylandQuickCompositorPrivate : public QtWayland::Compositor
{
public:
    QWaylandQuickCompositorPrivate(QWaylandQuickCompositor *compositor, QWaylandCompositor::ExtensionFlags extensions)
        : QtWayland::Compositor(compositor, extensions)
        , updateScheduled(false)
    {
    }

    void compositor_create_surface(Resource *resource, uint32_t id) Q_DECL_OVERRIDE
    {
        QWaylandQuickSurface *surface = new QWaylandQuickSurface(resource->client(), id, wl_resource_get_version(resource->handle), static_cast<QWaylandQuickCompositor *>(m_qt_compositor));
        m_surfaces << surface->handle();
        //BUG: This may not be an on-screen window surface though
        m_qt_compositor->surfaceCreated(surface);
    }

    void updateStarted()
    {
        updateScheduled = false;
        m_qt_compositor->frameStarted();
        m_qt_compositor->cleanupGraphicsResources();
    }

    bool updateScheduled;
};


QWaylandQuickCompositor::QWaylandQuickCompositor(QQuickWindow *window, const char *socketName, ExtensionFlags extensions)
                       : QWaylandCompositor(window, socketName, new QWaylandQuickCompositorPrivate(this, extensions))
{
    window->connect(window, &QQuickWindow::beforeSynchronizing, d_ptr(), &QWaylandQuickCompositorPrivate::updateStarted, Qt::DirectConnection);

    qmlRegisterUncreatableType<QWaylandSurfaceItem>("QtCompositor", 1, 0, "WaylandSurfaceItem", QObject::tr("Cannot create instance of WaylandSurfaceItem"));
    qmlRegisterUncreatableType<QWaylandQuickSurface>("QtCompositor", 1, 0, "WaylandQuickSurface", QObject::tr("Cannot create instance of WaylandQuickSurface"));
}

QWaylandQuickCompositorPrivate *QWaylandQuickCompositor::d_ptr()
{
    return static_cast<QWaylandQuickCompositorPrivate *>(m_compositor);
}

void QWaylandQuickCompositor::update()
{
    if (!d_ptr()->updateScheduled) {
        static_cast<QQuickWindow *>(window())->update();
        d_ptr()->updateScheduled = true;
    }
}

QWaylandSurfaceView *QWaylandQuickCompositor::createView(QWaylandSurface *surf)
{
    return new QWaylandSurfaceItem(static_cast<QWaylandQuickSurface *>(surf));
}

QT_END_NAMESPACE
