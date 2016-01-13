/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickpointattractor_p.h"
#include <cmath>
#include <QDebug>
QT_BEGIN_NAMESPACE
/*!
    \qmltype Attractor
    \instantiates QQuickAttractorAffector
    \inqmlmodule QtQuick.Particles
    \ingroup qtquick-particles
    \inherits Affector
    \brief For attracting particles towards a specific point

    Note that the size and position of this element affects which particles it affects.
    The size of the point attracted to is always 0x0, and the location of that point
    is specified by the pointX and pointY properties.

    Note that Attractor has the standard Item x,y,width and height properties.
    Like other affectors, these represent the affected area. They
    do not represent the 0x0 point which is the target of the attraction.
*/


/*!
    \qmlproperty real QtQuick.Particles::PointAttractor::pointX

    The x coordinate of the attracting point. This is relative
    to the x coordinate of the Attractor.
*/
/*!
    \qmlproperty real QtQuick.Particles::PointAttractor::pointY

    The y coordinate of the attracting point. This is relative
    to the y coordinate of the Attractor.
*/
/*!
    \qmlproperty real QtQuick.Particles::PointAttractor::strength

    The pull, in units per second, to be exerted on an item one pixel away.

    Depending on how the attraction is proportionalToDistance this may have to
    be very high or very low to have a reasonable effect on particles at a
    distance.
*/
/*!
    \qmlproperty AffectableParameter QtQuick.Particles::Attractor::affectedParameter

    What attribute of particles is directly affected.
    \list
    \li Attractor.Position
    \li Attractor.Velocity
    \li Attractor.Acceleration
    \endlist
*/
/*!
    \qmlproperty Proportion QtQuick.Particles::Attractor::proportionalToDistance

    How the distance from the particle to the point affects the strength of the attraction.

    \list
    \li Attractor.Constant
    \li Attractor.Linear
    \li Attractor.InverseLinear
    \li Attractor.Quadratic
    \li Attractor.InverseQuadratic
    \endlist
*/


QQuickAttractorAffector::QQuickAttractorAffector(QQuickItem *parent) :
    QQuickParticleAffector(parent), m_strength(0.0), m_x(0), m_y(0)
  , m_physics(Velocity), m_proportionalToDistance(Linear)
{
}

bool QQuickAttractorAffector::affectParticle(QQuickParticleData *d, qreal dt)
{
    if (m_strength == 0.0)
        return false;
    qreal dx = m_x+m_offset.x() - d->curX();
    qreal dy = m_y+m_offset.y() - d->curY();
    qreal r = std::sqrt((dx*dx) + (dy*dy));
    qreal theta = std::atan2(dy,dx);
    qreal ds = 0;
    switch (m_proportionalToDistance){
    case InverseQuadratic:
        ds = (m_strength / qMax<qreal>(1.,r*r));
        break;
    case InverseLinear:
        ds = (m_strength / qMax<qreal>(1.,r));
        break;
    case Quadratic:
        ds = (m_strength * qMax<qreal>(1.,r*r));
        break;
    case Linear:
        ds = (m_strength * qMax<qreal>(1.,r));
        break;
    default: //also Constant
        ds = m_strength;
    }
    ds *= dt;
    dx = ds * std::cos(theta);
    dy = ds * std::sin(theta);
    qreal vx,vy;
    switch (m_physics){
    case Position:
        d->x = (d->x + dx);
        d->y = (d->y + dy);
        break;
    case Acceleration:
        d->setInstantaneousAX(d->ax + dx);
        d->setInstantaneousAY(d->ay + dy);
        break;
    case Velocity: //also default
    default:
        vx = d->curVX();
        vy = d->curVY();
        d->setInstantaneousVX(vx + dx);
        d->setInstantaneousVY(vy + dy);
    }

    return true;
}
QT_END_NAMESPACE
