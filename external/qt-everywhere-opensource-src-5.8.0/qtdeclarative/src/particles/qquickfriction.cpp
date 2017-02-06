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

#include "qquickfriction_p.h"
#include <qmath.h>

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
    qreal curVX = d->curVX(m_system);
    qreal curVY = d->curVY(m_system);
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
        qreal curMag = qSqrt(curVX*curVX + curVY*curVY);
        if (curMag <= m_threshold + epsilon)
            return false;
        qreal newMag = qSqrt(newVX*newVX + newVY*newVY);
        if (newMag <= m_threshold + epsilon || //went past the threshold, stop there instead
            sign(curVX) != sign(newVX) || //went so far past maybe it came out the other side!
            sign(curVY) != sign(newVY)) {
            qreal theta = qAtan2(curVY, curVX);
            newVX = m_threshold * qCos(theta);
            newVY = m_threshold * qSin(theta);
        }
    }

    d->setInstantaneousVX(newVX, m_system);
    d->setInstantaneousVY(newVY, m_system);
    return true;
}
QT_END_NAMESPACE
