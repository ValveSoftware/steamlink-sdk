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

#include "qquickellipseextruder_p.h"
#include <qmath.h>
#include <stdlib.h>

QT_BEGIN_NAMESPACE
/*!
    \qmltype EllipseShape
    \instantiates QQuickEllipseExtruder
    \inqmlmodule QtQuick.Particles
    \ingroup qtquick-particles
    \inherits Shape
    \brief Represents an ellipse to other particle system elements

    This shape can be used by Emitter subclasses and Affector subclasses to have
    them act upon an ellipse shaped area.
*/
QQuickEllipseExtruder::QQuickEllipseExtruder(QObject *parent) :
    QQuickParticleExtruder(parent)
  , m_fill(true)
{
}

/*!
    \qmlproperty bool QtQuick.Particles::EllipseShape::fill
    If fill is true the ellipse is filled; otherwise it is just a border.

    Default is true.
*/

QPointF QQuickEllipseExtruder::extrude(const QRectF & r)
{
    qreal theta = ((qreal)rand()/RAND_MAX) * 6.2831853071795862;
    qreal mag = m_fill ? ((qreal)rand()/RAND_MAX) : 1;
    return QPointF(r.x() + r.width()/2 + mag * (r.width()/2) * qCos(theta),
                   r.y() + r.height()/2 + mag * (r.height()/2) * qSin(theta));
}

bool QQuickEllipseExtruder::contains(const QRectF &bounds, const QPointF &point)
{
    if (!bounds.contains(point))
        return false;

    QPointF relPoint(bounds.center() - point);
    qreal xa = relPoint.x()/bounds.width();
    qreal yb = relPoint.y()/bounds.height();
    return  (xa * xa + yb * yb) < 0.25;
}

QT_END_NAMESPACE
