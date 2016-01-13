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

#include "qquickgravity_p.h"
#include <cmath>
QT_BEGIN_NAMESPACE
const qreal CONV = 0.017453292520444443;
/*!
    \qmltype Gravity
    \instantiates QQuickGravityAffector
    \inqmlmodule QtQuick.Particles
    \ingroup qtquick-particles
    \inherits Affector
    \brief For applying acceleration in an angle

    This element will accelerate all affected particles to a vector of
    the specified magnitude in the specified angle. If the angle and acceleration do
    not vary, it is more efficient to set the specified acceleration on the Emitter.

    This element models the gravity of a massive object whose center of
    gravity is far away (and thus the gravitational pull is effectively constant
    across the scene). To model the gravity of an object near or inside the scene,
    use PointAttractor.
*/

/*!
    \qmlproperty real QtQuick.Particles::Gravity::magnitude

    Pixels per second that objects will be accelerated by.
*/
/*!
    \qmlproperty real QtQuick.Particles::Gravity::acceleration

    Name changed to magnitude, will be removed soon.
*/
/*!
    \qmlproperty real QtQuick.Particles::Gravity::angle

    Angle of acceleration.
*/

QQuickGravityAffector::QQuickGravityAffector(QQuickItem *parent) :
    QQuickParticleAffector(parent), m_magnitude(-10), m_angle(90), m_needRecalc(true)
{
}

bool QQuickGravityAffector::affectParticle(QQuickParticleData *d, qreal dt)
{
    if (!m_magnitude)
        return false;
    if (m_needRecalc) {
        m_needRecalc = false;
        m_dx = m_magnitude * std::cos(m_angle * CONV);
        m_dy = m_magnitude * std::sin(m_angle * CONV);
    }

    d->setInstantaneousVX(d->curVX() + m_dx*dt);
    d->setInstantaneousVY(d->curVY() + m_dy*dt);
    return true;
}
QT_END_NAMESPACE
