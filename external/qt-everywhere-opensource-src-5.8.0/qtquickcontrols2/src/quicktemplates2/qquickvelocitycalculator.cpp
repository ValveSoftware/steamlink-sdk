/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickvelocitycalculator_p_p.h"

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

/*
    Usage:

    QQuickVelocityCalculator velocityCalculator;

    // ...

    velocityCalcular.startMeasuring(event->pos(), event->timestamp());
    velocityCalcular.stopMeasuring(event->pos(), event->timestamp());

    // ...

    if (velocityCalculator.velocity().x() > someAmount)
        doSomething();
    else if (velocityCalculator.velocity().x() < -someAmount)
        doSomethingElse();
*/

QQuickVelocityCalculator::QQuickVelocityCalculator() :
    m_point1Timestamp(0),
    m_point2Timestamp(0)
{
}

void QQuickVelocityCalculator::startMeasuring(const QPointF &point1, qint64 timestamp)
{
    m_point1 = point1;

    if (timestamp != 0)
        m_point1Timestamp = timestamp;
    else
        m_timer.start();
}

void QQuickVelocityCalculator::stopMeasuring(const QPointF &point2, qint64 timestamp)
{
    if (timestamp == 0 && !m_timer.isValid()) {
        qWarning() << "QQuickVelocityCalculator: a call to stopMeasuring() must be preceded by a call to startMeasuring()";
        return;
    }

    m_point2 = point2;
    m_point2Timestamp = timestamp != 0 ? timestamp : m_timer.elapsed();
    m_timer.invalidate();
}

void QQuickVelocityCalculator::reset()
{
    m_point1 = QPointF();
    m_point2 = QPointF();
    m_point1Timestamp = 0;
    m_point2Timestamp = 0;
    m_timer.invalidate();
}

QPointF QQuickVelocityCalculator::velocity() const
{
    if ((m_point2Timestamp == 0 || m_point1Timestamp == m_point2Timestamp) && !m_timer.isValid())
        return QPointF();

    const qreal secondsElapsed = (m_point2Timestamp != 0 ? m_point2Timestamp - m_point1Timestamp : m_timer.elapsed()) / 1000.0;
    const QPointF distance = m_point2 - m_point1;
    return distance / secondsElapsed;
}

QT_END_NAMESPACE
