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

#include "qquickfriction_p.h"
#include <math.h>

QT_BEGIN_NAMESPACE
/*!
    \qmltype Friction
    \instantiates QQuickFrictionAffector
    \inqmlmodule QtQuick.Particles
    \ingroup qtquick-particles
    \inherits Affector
    \brief For applying friction proportional to the particle's current velocity

*/

/*!
    \qmlproperty real QtQuick.Particles::Friction::factor

    A drag will be applied to moving objects which is this factor of their current velocity.
*/
/*!
    \qmlproperty real QtQuick.Particles::Friction::threshold

    The drag will only be applied to objects with a velocity above the threshold velocity. The
    drag applied will bring objects down to the threshold velocity, but no further.

    The default threshold is 0
*/
static qreal sign(qreal a)
{
    return a >= 0 ? 1 : -1;
}

static const qreal epsilon = 0.00001;

QQuickFrictionAffector::QQuickFrictionAffector(QQuickItem *parent) :
    QQuickParticleAffector(parent), m_factor(0.0), m_threshold(0.0)
{
}

bool QQuickFrictionAffector::affectParticle(QQuickParticleData *d, qreal dt)
{
    if (!m_factor)
        return false;
    qreal curVX = d->curVX();
    qreal curVY = d->curVY();
    if (!curVX && !curVY)
        return false;
    qreal newVX = curVX + (curVX * m_factor * -1 * dt);
    qreal newVY = curVY + (curVY * m_factor * -1 * dt);

    if (!m_threshold) {
        if (sign(curVX) != sign(newVX))
            newVX = 0;
        if (sign(curVY) != sign(newVY))
            newVY = 0;
    } else {
        qreal curMag = sqrt(curVX*curVX + curVY*curVY);
        if (curMag <= m_threshold + epsilon)
            return false;
        qreal newMag = sqrt(newVX*newVX + newVY*newVY);
        if (newMag <= m_threshold + epsilon || //went past the threshold, stop there instead
            sign(curVX) != sign(newVX) || //went so far past maybe it came out the other side!
            sign(curVY) != sign(newVY)) {
            qreal theta = atan2(curVY, curVX);
            newVX = m_threshold * cos(theta);
            newVY = m_threshold * sin(theta);
        }
    }

    d->setInstantaneousVX(newVX);
    d->setInstantaneousVY(newVY);
    return true;
}
QT_END_NAMESPACE
