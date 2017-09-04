/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
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

#include "qquickmathutils_p.h"

#include <qmath.h>

QQuickMathUtils::QQuickMathUtils(QObject *parent) :
    QObject(parent)
{
}

qreal QQuickMathUtils::pi2() const
{
    return M_PI * 2;
}

/*!
    Converts the angle \a degrees to radians.
*/
qreal QQuickMathUtils::degToRad(qreal degrees) const {
    return degrees * (M_PI / 180);
}

/*!
    Converts the angle \a degrees to radians.

    This function assumes that the angle origin (0) is north, as this
    is the origin used by all of the Extras. The angle
    returned will have its angle origin (0) pointing east, in order to be
    consistent with standard angles used by \l {QtQuick::Canvas}{Canvas},
    for example.
*/
qreal QQuickMathUtils::degToRadOffset(qreal degrees) const {
    return (degrees - 90) * (M_PI / 180);
}

/*!
    Converts the angle \a radians to degrees.
*/
qreal QQuickMathUtils::radToDeg(qreal radians) const {
    return radians * (180 / M_PI);
}

/*!
    Converts the angle \a radians to degrees.

    This function assumes that the angle origin (0) is east; as is standard for
    mathematical operations using radians (this origin is used by
    \l {QtQuick::Canvas}{Canvas}, for example). The angle returned in degrees
    will have its angle origin (0) pointing north, which is what the extras
    expect.
*/
qreal QQuickMathUtils::radToDegOffset(qreal radians) const {
    return radians * (180 / M_PI) + 90;
}

/*!
    Returns the top left position of the item if it were centered along
    a circle according to \a angleOnCircle and \a distanceAlongRadius.

    \a angleOnCircle is from 0.0 - M_PI2.
    \a distanceAlongRadius is the distance along the radius in M_PIxels.
*/
QPointF QQuickMathUtils::centerAlongCircle(qreal xCenter, qreal yCenter,
    qreal width, qreal height, qreal angleOnCircle, qreal distanceAlongRadius) const {
    return QPointF(
        (xCenter - width / 2) + (distanceAlongRadius * qCos(angleOnCircle)),
        (yCenter - height / 2) + (distanceAlongRadius * qSin(angleOnCircle)));
}

/*!
    Returns \a number rounded to the nearest even integer.
*/
qreal QQuickMathUtils::roundEven(qreal number) const {
    int rounded = qRound(number);
    return rounded % 2 == 0 ? rounded : rounded + 1;
}
