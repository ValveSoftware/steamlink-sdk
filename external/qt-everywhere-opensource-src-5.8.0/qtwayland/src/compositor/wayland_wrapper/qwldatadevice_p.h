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

#ifndef WLDATADEVICE_H
#define WLDATADEVICE_H

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

#include <QtWaylandCompositor/private/qwayland-server-wayland.h>
#include <QtWaylandCompositor/QWaylandSeat>

QT_BEGIN_NAMESPACE

namespace QtWayland {

class Compositor;
class DataSource;
class Seat;
class Surface;

class DataDevice : public QtWaylandServer::wl_data_device
{
public:
    DataDevice(QWaylandSeat *seat);

    void setFocus(QWaylandClient *client);

    void setDragFocus(QWaylandSurface *focus, const QPointF &localPosition);

    QWaylandSurface *dragIcon() const;
    QWaylandSurface *dragOrigin() const;

    void sourceDestroyed(DataSource *source);

    void dragMove(QWaylandSurface *target, const QPointF &pos);
    void drop();
    void cancelDrag();

protected:
    void data_device_start_drag(Resource *resource, struct ::wl_resource *source, struct ::wl_resource *origin, struct ::wl_resource *icon, uint32_t serial) Q_DECL_OVERRIDE;
    void data_device_set_selection(Resource *resource, struct ::wl_resource *source, uint32_t serial) Q_DECL_OVERRIDE;

private:
    void setDragIcon(QWaylandSurface *icon);

    QWaylandCompositor *m_compositor;
    QWaylandSeat *m_seat;

    DataSource *m_selectionSource;

    struct ::wl_client *m_dragClient;
    DataSource *m_dragDataSource;

    QWaylandSurface *m_dragFocus;
    Resource *m_dragFocusResource;

    QWaylandSurface *m_dragIcon;
    QWaylandSurface *m_dragOrigin;
};

}

QT_END_NAMESPACE

#endif // WLDATADEVICE_H
