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

#include "qquickwander_p.h"
#include "qquickparticlesystem_p.h"//for ParticlesVertices
QT_BEGIN_NAMESPACE
/*!
    \qmltype Wander
    \instantiates QQuickWanderAffector
    \inqmlmodule QtQuick.Particles
    \ingroup qtquick-particles
    \inherits Affector
    \brief For applying random particle trajectory

*/
/*!
    \qmlproperty real QtQuick.Particles::Wander::pace

    Maximum attribute change per second.
*/
/*!
    \qmlproperty real QtQuick.Particles::Wander::xVariance

    Maximum attribute x value (as a result of Wander).

    If unset, Wander will not affect x values.
*/
/*!
    \qmlproperty real QtQuick.Particles::Wander::yVariance

    Maximum attribute y value (as a result of Wander).

    If unset, Wander will not affect y values.
*/
/*!
    \qmlproperty AffectableParameter QtQuick.Particles::Wander::affectedParameter

    What attribute of particles is directly affected.
    \list
    \li PointAttractor.Position
    \li PointAttractor.Velocity
    \li PointAttractor.Acceleration
    \endlist
*/

QQuickWanderAffector::QQuickWanderAffector(QQuickItem *parent) :
    QQuickParticleAffector(parent), m_xVariance(0), m_yVariance(0), m_pace(0)
    , m_affectedParameter(Velocity)
{
    m_needsReset = true;
}

QQuickWanderAffector::~QQuickWanderAffector()
{
    for (QHash<int, WanderData*>::const_iterator iter=m_wanderData.constBegin();
        iter != m_wanderData.constEnd(); iter++)
        delete (*iter);
}

WanderData* QQuickWanderAffector::getData(int idx)
{
    if (m_wanderData.contains(idx))
        return m_wanderData[idx];
    WanderData* d = new WanderData;
    d->x_vel = 0;
    d->y_vel = 0;
    d->x_peak = m_xVariance;
    d->y_peak = m_yVariance;
    d->x_var = m_pace * qreal(qrand()) / RAND_MAX;
    d->y_var = m_pace * qreal(qrand()) / RAND_MAX;

    m_wanderData.insert(idx, d);
    return d;
}

// TODO: see below
//void QQuickWanderAffector::reset(int systemIdx)
//{
//    if (m_wanderData.contains(systemIdx))
//        delete m_wanderData[systemIdx];
//    m_wanderData.remove(systemIdx);
//}

bool QQuickWanderAffector::affectParticle(QQuickParticleData* data, qreal dt)
{
    /*TODO: Add a mode which does basically this - picking a direction, going in it (random velocity) and then going back
    WanderData* d = getData(data->systemIndex);
    if (m_xVariance != 0.) {
        if ((d->x_vel > d->x_peak && d->x_var > 0.0) || (d->x_vel < -d->x_peak && d->x_var < 0.0)) {
            d->x_var = -d->x_var;
            d->x_peak = m_xVariance + m_xVariance * qreal(qrand()) / RAND_MAX;
        }
        d->x_vel += d->x_var * dt;
    }
    qreal dx = dt * d->x_vel;

    if (m_yVariance != 0.) {
        if ((d->y_vel > d->y_peak && d->y_var > 0.0) || (d->y_vel < -d->y_peak && d->y_var < 0.0)) {
            d->y_var = -d->y_var;
            d->y_peak = m_yVariance + m_yVariance * qreal(qrand()) / RAND_MAX;
        }
        d->y_vel += d->y_var * dt;
    }
    qreal dy = dt * d->x_vel;

    //### Should we be amending vel instead?
    ParticleVertex* p = &(data->pv);
    p->x += dx;

    p->y += dy;
    return true;
    */
    qreal dx = dt * m_pace * (2 * qreal(qrand())/RAND_MAX - 1);
    qreal dy = dt * m_pace * (2 * qreal(qrand())/RAND_MAX - 1);
    qreal newX, newY;
    switch (m_affectedParameter){
    case Position:
        newX = data->curX() + dx;
        if (m_xVariance > qAbs(newX) )
            data->x += dx;
        newY = data->curY() + dy;
        if (m_yVariance > qAbs(newY) )
            data->y += dy;
        break;
    default:
    case Velocity:
        newX = data->curVX() + dx;
        if (m_xVariance > qAbs(newX) )
            data->setInstantaneousVX(newX);
        newY = data->curVY() + dy;
        if (m_yVariance > qAbs(newY) )
            data->setInstantaneousVY(newY);
        break;
    case Acceleration:
        newX = data->ax + dx;
        if (m_xVariance > qAbs(newX) )
            data->setInstantaneousAX(newX);
        newY = data->ay + dy;
        if (m_yVariance > qAbs(newY) )
            data->setInstantaneousAY(newY);
        break;
    }
    return true;
}
QT_END_NAMESPACE
