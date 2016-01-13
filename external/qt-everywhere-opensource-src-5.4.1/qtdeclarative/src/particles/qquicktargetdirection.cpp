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

#include "qquicktargetdirection_p.h"
#include "qquickparticleemitter_p.h"
#include <cmath>
#include <QDebug>

QT_BEGIN_NAMESPACE
/*!
    \qmltype TargetDirection
    \instantiates QQuickTargetDirection
    \inqmlmodule QtQuick.Particles
    \ingroup qtquick-particles
    \inherits Direction
    \brief For specifying a direction towards the target point

*/
/*!
    \qmlproperty real QtQuick.Particles::TargetDirection::targetX
*/
/*!
    \qmlproperty real QtQuick.Particles::TargetDirection::targetY
*/
/*!
    \qmlproperty Item QtQuick.Particles::TargetDirection::targetItem
    If specified, this will take precedence over targetX and targetY.
    The targeted point will be the center of the specified Item
*/
/*!
    \qmlproperty real QtQuick.Particles::TargetDirection::targetVariation
*/
/*!
    \qmlproperty real QtQuick.Particles::TargetDirection::magnitude
*/
/*!
    \qmlproperty real QtQuick.Particles::TargetDirection::magnitudeVariation
*/
/*!
    \qmlproperty bool QtQuick.Particles::TargetDirection::proportionalMagnitude

    If true, then the value of magnitude and magnitudeVariation shall be interpreted as multiples
    of the distance between the source point and the target point, per second.

    If false(default), then the value of magnitude and magnitudeVariation shall be interpreted as
    pixels per second.
*/

QQuickTargetDirection::QQuickTargetDirection(QObject *parent) :
    QQuickDirection(parent)
  , m_targetX(0)
  , m_targetY(0)
  , m_targetVariation(0)
  , m_proportionalMagnitude(false)
  , m_magnitude(0)
  , m_magnitudeVariation(0)
  , m_targetItem(0)
{
}

const QPointF QQuickTargetDirection::sample(const QPointF &from)
{
    //###This approach loses interpolating the last position of the target (like we could with the emitter) is it worthwhile?
    QPointF ret;
    qreal targetX;
    qreal targetY;
    if (m_targetItem){
        QQuickParticleEmitter* parentEmitter = qobject_cast<QQuickParticleEmitter*>(parent());
        targetX = m_targetItem->width()/2;
        targetY = m_targetItem->height()/2;
        if (!parentEmitter){
            qWarning() << "Directed vector is not a child of the emitter. Mapping of target item coordinates may fail.";
            targetX += m_targetItem->x();
            targetY += m_targetItem->y();
        }else{
            ret = parentEmitter->mapFromItem(m_targetItem, QPointF(targetX, targetY));
            targetX = ret.x();
            targetY = ret.y();
        }
    }else{
        targetX = m_targetX;
        targetY = m_targetY;
    }
    targetX += 0 - from.x() - m_targetVariation + rand()/(float)RAND_MAX * m_targetVariation*2;
    targetY += 0 - from.y() - m_targetVariation + rand()/(float)RAND_MAX * m_targetVariation*2;
    qreal theta = std::atan2(targetY, targetX);
    qreal mag = m_magnitude + rand()/(float)RAND_MAX * m_magnitudeVariation * 2 - m_magnitudeVariation;
    if (m_proportionalMagnitude)
        mag *= std::sqrt(targetX * targetX + targetY * targetY);
    ret.setX(mag * std::cos(theta));
    ret.setY(mag * std::sin(theta));
    return ret;
}

QT_END_NAMESPACE
