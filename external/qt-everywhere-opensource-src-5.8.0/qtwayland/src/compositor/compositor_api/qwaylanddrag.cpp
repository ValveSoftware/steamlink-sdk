/****************************************************************************
**
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

#include "qwaylanddrag.h"

#include <private/qobject_p.h>

#include "qwldatadevice_p.h"
#include "qwaylandview.h"
#include <QtWaylandCompositor/private/qwaylandseat_p.h>

QT_BEGIN_NAMESPACE

class QWaylandDragPrivate : public QObjectPrivate
{
public:
    QWaylandDragPrivate(QWaylandSeat *seat)
        : seat(seat)
    {
    }

    QtWayland::DataDevice *dataDevice()
    {
        return QWaylandSeatPrivate::get(seat)->dataDevice();
    }

    const QtWayland::DataDevice *dataDevice() const
    {
        return QWaylandSeatPrivate::get(seat)->dataDevice();
    }

    QWaylandSeat *seat;
};

QWaylandDrag::QWaylandDrag(QWaylandSeat *seat)
    : QObject(* new QWaylandDragPrivate(seat))
{
}

QWaylandSurface *QWaylandDrag::icon() const
{
    Q_D(const QWaylandDrag);

    const QtWayland::DataDevice *dataDevice = d->dataDevice();
    if (!dataDevice)
        return 0;

    return dataDevice->dragIcon();
}

QWaylandSurface *QWaylandDrag::origin() const
{
    Q_D(const QWaylandDrag);
    const QtWayland::DataDevice *dataDevice = d->dataDevice();
    return dataDevice ? dataDevice->dragOrigin() : nullptr;
}

QWaylandSeat *QWaylandDrag::seat() const
{
    Q_D(const QWaylandDrag);
    return d->seat;
}


bool QWaylandDrag::visible() const
{
    Q_D(const QWaylandDrag);

    const QtWayland::DataDevice *dataDevice = d->dataDevice();
    if (!dataDevice)
        return false;

    return dataDevice->dragIcon() != 0;
}

void QWaylandDrag::dragMove(QWaylandSurface *target, const QPointF &pos)
{
    Q_D(QWaylandDrag);
    QtWayland::DataDevice *dataDevice = d->dataDevice();
    if (!dataDevice)
        return;
    dataDevice->dragMove(target, pos);
}
void QWaylandDrag::drop()
{
    Q_D(QWaylandDrag);
    QtWayland::DataDevice *dataDevice = d->dataDevice();
    if (!dataDevice)
        return;
    dataDevice->drop();
}

void QWaylandDrag::cancelDrag()
{
    Q_D(QWaylandDrag);
    QtWayland::DataDevice *dataDevice = d->dataDevice();
    if (!dataDevice)
        return;
    dataDevice->cancelDrag();
}

QT_END_NAMESPACE
