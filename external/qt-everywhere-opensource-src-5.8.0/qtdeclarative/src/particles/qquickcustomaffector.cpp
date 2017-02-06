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

#include "qquickcustomaffector_p.h"
#include <private/qv8engine_p.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlglobal_p.h>
#include <QQmlEngine>
#include <QDebug>
QT_BEGIN_NAMESPACE

//TODO: Move docs (and inheritence) to real base when docs can propagate. Currently this pretends to be the base class!
/*!
    \qmlsignal QtQuick.Particles::Affector::affectParticles(Array particles, real dt)

    This signal is emitted when particles are selected to be affected. particles contains
    an array of particle objects which can be directly manipulated.

    dt is the time since the last time it was affected. Use dt to normalize
    trajectory manipulations to real time.

    Note that JavaScript is slower to execute, so it is not recommended to use this in
    high-volume particle systems.

    The corresponding handler is \c onAffectParticles.
*/

/*!
    \qmlproperty StochasticDirection QtQuick.Particles::Affector::position

    Affected particles will have their position set to this direction,
    relative to the ParticleSystem. When interpreting directions as points,
    imagine it as an arrow with the base at the 0,0 of the ParticleSystem and the
    tip at where the specified position will be.
*/

/*!
    \qmlproperty StochasticDirection QtQuick.Particles::Affector::velocity

    Affected particles will have their velocity set to this direction.
*/


/*!
    \qmlproperty StochasticDirection QtQuick.Particles::Affector::acceleration

    Affected particles will have their acceleration set to this direction.
*/


/*!
    \qmlproperty bool QtQuick.Particles::Affector::relative

    Whether the affected particles have their existing position/velocity/acceleration added
    to the new one.

    Default is true.
*/
QQuickCustomAffector::QQuickCustomAffector(QQuickItem *parent) :
    QQuickParticleAffector(parent)
    , m_position(&m_nullVector)
    , m_velocity(&m_nullVector)
    , m_acceleration(&m_nullVector)
    , m_relative(true)
{
}

bool QQuickCustomAffector::isAffectConnected()
{
    IS_SIGNAL_CONNECTED(this, QQuickCustomAffector, affectParticles, (QQmlV4Handle,qreal));
}

void QQuickCustomAffector::affectSystem(qreal dt)
{
    //Acts a bit differently, just emits affected for everyone it might affect, when the only thing is connecting to affected(x,y)
    bool justAffected = (m_acceleration == &m_nullVector
        && m_velocity == &m_nullVector
        && m_position == &m_nullVector
        && isAffectedConnected());
    if (!isAffectConnected() && !justAffected) {
        QQuickParticleAffector::affectSystem(dt);
        return;
    }
    if (!m_enabled)
        return;
    updateOffsets();

    QList<QQuickParticleData*> toAffect;
    foreach (QQuickParticleGroupData* gd, m_system->groupData) {
        if (activeGroup(gd->index)) {
            foreach (QQuickParticleData* d, gd->data) {
                if (shouldAffect(d)) {
                    toAffect << d;
                }
            }
        }
    }

    if (toAffect.isEmpty())
        return;

    if (justAffected) {
        foreach (QQuickParticleData* d, toAffect) {//Not postAffect to avoid saying the particle changed
            if (m_onceOff)
                m_onceOffed << qMakePair(d->groupId, d->index);
            emit affected(d->curX(m_system), d->curY(m_system));
        }
        return;
    }

    if (m_onceOff)
        dt = 1.0;

    QQmlEngine *qmlEngine = ::qmlEngine(this);
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(qmlEngine->handle());

    QV4::Scope scope(v4);
    QV4::ScopedArrayObject array(scope, v4->newArrayObject(toAffect.size()));
    QV4::ScopedValue v(scope);
    for (int i=0; i<toAffect.size(); i++)
        array->putIndexed(i, (v = toAffect[i]->v4Value(m_system)));

    if (dt >= simulationCutoff || dt <= simulationDelta) {
        affectProperties(toAffect, dt);
        emit affectParticles(QQmlV4Handle(array), dt);
    } else {
        int realTime = m_system->timeInt;
        m_system->timeInt -= dt * 1000.0;
        while (dt > simulationDelta) {
            m_system->timeInt += simulationDelta * 1000.0;
            dt -= simulationDelta;
            affectProperties(toAffect, simulationDelta);
            emit affectParticles(QQmlV4Handle(array), simulationDelta);
        }
        m_system->timeInt = realTime;
        if (dt > 0.0) {
            affectProperties(toAffect, dt);
            emit affectParticles(QQmlV4Handle(array), dt);
        }
    }

    foreach (QQuickParticleData* d, toAffect)
        if (d->update == 1.0)
            postAffect(d);
}

bool QQuickCustomAffector::affectParticle(QQuickParticleData *d, qreal dt)
{
    //This does the property based affecting, called by superclass if signal isn't hooked up.
    bool changed = false;
    QPointF curPos(d->curX(m_system), d->curY(m_system));

    if (m_acceleration != &m_nullVector){
        QPointF pos = m_acceleration->sample(curPos);
        QPointF curAcc = QPointF(d->curAX(), d->curAY());
        if (m_relative) {
            pos *= dt;
            pos += curAcc;
        }
        if (pos != curAcc) {
            d->setInstantaneousAX(pos.x(), m_system);
            d->setInstantaneousAY(pos.y(), m_system);
            changed = true;
        }
    }

    if (m_velocity != &m_nullVector){
        QPointF pos = m_velocity->sample(curPos);
        QPointF curVel = QPointF(d->curVX(m_system), d->curVY(m_system));
        if (m_relative) {
            pos *= dt;
            pos += curVel;
        }
        if (pos != curVel) {
            d->setInstantaneousVX(pos.x(), m_system);
            d->setInstantaneousVY(pos.y(), m_system);
            changed = true;
        }
    }

    if (m_position != &m_nullVector){
        QPointF pos = m_position->sample(curPos);
        if (m_relative) {
            pos *= dt;
            pos += curPos;
        }
        if (pos != curPos) {
            d->setInstantaneousX(pos.x(), m_system);
            d->setInstantaneousY(pos.y(), m_system);
            changed = true;
        }
    }

    return changed;
}

void QQuickCustomAffector::affectProperties(const QList<QQuickParticleData*> particles, qreal dt)
{
    foreach (QQuickParticleData* d, particles)
        if ( affectParticle(d, dt) )
            d->update = 1.0;
}

QT_END_NAMESPACE
