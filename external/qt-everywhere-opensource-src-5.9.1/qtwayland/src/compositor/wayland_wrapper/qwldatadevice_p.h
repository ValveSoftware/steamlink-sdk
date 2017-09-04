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
#include <QtWaylandCompositor/private/qtwaylandcompositorglobal_p.h>
#include <QtWaylandCompositor/QWaylandSeat>

QT_REQUIRE_CONFIG(wayland_datadevice);

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
    void sourceDestroyed(DataSource *source);

#if QT_CONFIG(draganddrop)
    void setDragFocus(QWaylandSurface *focus, const QPointF &localPosition);

    QWaylandSurface *dragIcon() const;
    QWaylandSurface *dragOrigin() const;

    void dragMove(QWaylandSurface *target, const QPointF &pos);
    void drop();
    void cancelDrag();
#endif

protected:
#if QT_CONFIG(draganddrop)
    void data_device_start_drag(Resource *resource, struct ::wl_resource *source, struct ::wl_resource *origin, struct ::wl_resource *icon, uint32_t serial) override;
#endif
    void data_device_set_selection(Resource *resource, struct ::wl_resource *source, uint32_t serial) override;

private:
#if QT_CONFIG(draganddrop)
    void setDragIcon(QWaylandSurface *icon);
#endif

    QWaylandCompositor *m_compositor;
    QWaylandSeat *m_seat;

    DataSource *m_selectionSource;

#if QT_CONFIG(draganddrop)
    struct ::wl_client *m_dragClient;
    DataSource *m_dragDataSource;

    QWaylandSurface *m_dragFocus;
    Resource *m_dragFocusResource;

    QWaylandSurface *m_dragIcon;
    QWaylandSurface *m_dragOrigin;
#endif
};

}

QT_END_NAMESPACE

#endif // WLDATADEVICE_H
