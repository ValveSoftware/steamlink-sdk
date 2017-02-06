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

#include "qquickage_p.h"
#include "qquickparticleemitter_p.h"
QT_BEGIN_NAMESPACE
/*!
    \qmltype Age
    \instantiates QQuickAgeAffector
    \inqmlmodule QtQuick.Particles
    \inherits Affector
    \brief For altering particle ages
    \ingroup qtquick-particles

    The Age affector allows you to alter where the particle is in its lifecycle. Common uses
    are to expire particles prematurely, possibly giving them time to animate out.

    The Age affector is also sometimes known as a 'Kill' affector, because with the default
    parameters it will immediately expire all particles which it affects.

    The Age affector only applies to particles which are still alive.
*/
/*!
    \qmlproperty int QtQuick.Particles::Age::lifeLeft

    The amount of life to set the particle to have. Affected particles
    will advance to a point in their life where they will have this many
    milliseconds left to live.
*/

/*!
    \qmlproperty bool QtQuick.Particles::Age::advancePosition

    advancePosition determines whether position, veclocity and acceleration are included in
    the simulated aging done by the affector. If advancePosition is false,
    then the position, velocity and acceleration will remain the same and only
    other attributes (such as opacity) will advance in the simulation to where
    it would normally be for that point in the particle's life. With advancePosition set to
    true the position, velocity and acceleration will also advance to where it would
    normally be by that point in the particle's life, making it advance its position
    on screen.

    Default value is true.
*/

QQuickAgeAffector::QQuickAgeAffector(QQuickItem *parent) :
    QQuickParticleAffector(parent), m_lifeLeft(0), m_advancePosition(true)
{
}


bool QQuickAgeAffector::affectParticle(QQuickParticleData *d, qreal dt)
{
    Q_UNUSED(dt);
    if (d->stillAlive(m_system)){
        float curT = m_system->timeInt / 1000.0f;
        float ttl = m_lifeLeft / 1000.0f;
        if (!m_advancePosition && ttl > 0){
            float x = d->curX(m_system);
            float vx = d->curVX(m_system);
            float ax = d->curAX();
            float y = d->curY(m_system);
            float vy = d->curVY(m_system);
            float ay = d->curAY();
            d->t = curT - (d->lifeSpan - ttl);
            d->setInstantaneousX(x, m_system);
            d->setInstantaneousVX(vx, m_system);
            d->setInstantaneousAX(ax, m_system);
            d->setInstantaneousY(y, m_system);
            d->setInstantaneousVY(vy, m_system);
            d->setInstantaneousAY(ay, m_system);
        } else {
            d->t = curT - (d->lifeSpan - ttl);
        }
        return true;
    }
    return false;
}
QT_END_NAMESPACE
