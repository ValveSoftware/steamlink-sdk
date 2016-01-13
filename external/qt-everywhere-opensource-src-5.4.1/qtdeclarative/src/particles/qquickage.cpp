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
    if (d->stillAlive()){
        qreal curT = (qreal)m_system->timeInt/1000.0;
        qreal ttl = (qreal)m_lifeLeft/1000.0;
        if (!m_advancePosition && ttl > 0){
            qreal x = d->curX();
            qreal vx = d->curVX();
            qreal ax = d->curAX();
            qreal y = d->curY();
            qreal vy = d->curVY();
            qreal ay = d->curAY();
            d->t = curT - (d->lifeSpan - ttl);
            d->setInstantaneousX(x);
            d->setInstantaneousVX(vx);
            d->setInstantaneousAX(ax);
            d->setInstantaneousY(y);
            d->setInstantaneousVY(vy);
            d->setInstantaneousAY(ay);
        } else {
            d->t = curT - (d->lifeSpan - ttl);
        }
        return true;
    }
    return false;
}
QT_END_NAMESPACE
