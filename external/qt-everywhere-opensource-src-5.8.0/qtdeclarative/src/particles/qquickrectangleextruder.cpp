/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickrectangleextruder_p.h"
#include <stdlib.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype RectangleShape
    \instantiates QQuickRectangleExtruder
    \inqmlmodule QtQuick.Particles
    \brief For specifying an area for affectors and emitter
    \ingroup qtquick-particles

    Just a rectangle.
*/

QQuickRectangleExtruder::QQuickRectangleExtruder(QObject *parent) :
    QQuickParticleExtruder(parent), m_fill(true)
{
}

QPointF QQuickRectangleExtruder::extrude(const QRectF &rect)
{
    if (m_fill)
        return QPointF(((qreal)rand() / RAND_MAX) * rect.width() + rect.x(),
                       ((qreal)rand() / RAND_MAX) * rect.height() + rect.y());
    int side = rand() % 4;
    switch (side){//TODO: Doesn't this overlap the corners?
    case 0:
        return QPointF(rect.x(),
                       ((qreal)rand() / RAND_MAX) * rect.height() + rect.y());
    case 1:
        return QPointF(rect.width() + rect.x(),
                       ((qreal)rand() / RAND_MAX) * rect.height() + rect.y());
    case 2:
        return QPointF(((qreal)rand() / RAND_MAX) * rect.width() + rect.x(),
                       rect.y());
    default:
        return QPointF(((qreal)rand() / RAND_MAX) * rect.width() + rect.x(),
                       rect.height() + rect.y());
    }
}

bool QQuickRectangleExtruder::contains(const QRectF &bounds, const QPointF &point)
{
    return bounds.contains(point);
}

QT_END_NAMESPACE
