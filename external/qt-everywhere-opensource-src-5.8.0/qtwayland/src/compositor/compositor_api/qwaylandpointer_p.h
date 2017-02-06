/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2013 Klarälvdalens Datakonsult AB (KDAB).
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

#ifndef QWAYLANDPOINTER_P_H
#define QWAYLANDPOINTER_P_H

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

#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>
#include <QtWaylandCompositor/QWaylandDestroyListener>
#include <QtWaylandCompositor/QWaylandPointer>

#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QObject>
#include <QtCore/private/qobject_p.h>

#include <QtWaylandCompositor/private/qwayland-server-wayland.h>
#include <QtWaylandCompositor/QWaylandView>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandSeat>

#include <stdint.h>

QT_BEGIN_NAMESPACE

class QWaylandView;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandPointerPrivate : public QObjectPrivate
                                                 , public QtWaylandServer::wl_pointer
{
    Q_DECLARE_PUBLIC(QWaylandPointer)
public:
    QWaylandPointerPrivate(QWaylandPointer *pointer, QWaylandSeat *seat);

    QWaylandCompositor *compositor() const { return seat->compositor(); }

protected:
    void pointer_set_cursor(Resource *resource, uint32_t serial, wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y) Q_DECL_OVERRIDE;
    void pointer_release(Resource *resource) Q_DECL_OVERRIDE;
    void pointer_destroy_resource(Resource *resource) Q_DECL_OVERRIDE;

private:
    void focusDestroyed(void *data);

    QWaylandSeat *seat;
    QWaylandOutput *output;

    QPointF localPosition;
    QPointF spacePosition;

    struct ::wl_resource *focusResource;
    bool hasSentEnter;

    int buttonCount;

    QWaylandDestroyListener focusDestroyListener;

    static QWaylandSurfaceRole s_role;
};

QT_END_NAMESPACE

#endif // QWAYLANDPOINTER_P_H
