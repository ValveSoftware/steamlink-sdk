/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qdeclarativegeocoordinateanimation_p.h"
#include <QtQuick/private/qquickanimation_p_p.h>
#include <QtPositioning/private/qgeoprojection_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype CoordinateAnimation
    \instantiates QDeclarativeGeoCoordinateAnimation
    \inherits PropertyAnimation
    \inqmlmodule QtPositioning
    \since 5.3

    \brief A PropertyAnimation for geo coordinate properties.

    A specialized \l{PropertyAnimation} that defines an animation
    between two geo coordinates.
*/

QDeclarativeGeoCoordinateAnimation::QDeclarativeGeoCoordinateAnimation(QObject *parent)
    : QQuickPropertyAnimation(parent)
{
    Q_D(QQuickPropertyAnimation);
    d->interpolatorType = qMetaTypeId<QGeoCoordinate>();
    d->defaultToInterpolatorType = true;
    d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
}

QDeclarativeGeoCoordinateAnimation::~QDeclarativeGeoCoordinateAnimation()
{
}

/*!
    \qmlproperty coordinate CoordinateAnimation::from
    This property holds the coordinate where the animation should begin.
*/
QGeoCoordinate QDeclarativeGeoCoordinateAnimation::from() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->from.value<QGeoCoordinate>();
}

void QDeclarativeGeoCoordinateAnimation::setFrom(const QGeoCoordinate &f)
{
    QQuickPropertyAnimation::setFrom(QVariant::fromValue(f));
}

/*!
    \qmlproperty coordinate CoordinateAnimation::to
    This property holds the coordinate where the animation should end.
*/
QGeoCoordinate QDeclarativeGeoCoordinateAnimation::to() const
{
    Q_D(const QQuickPropertyAnimation);
    return d->to.value<QGeoCoordinate>();
}

void QDeclarativeGeoCoordinateAnimation::setTo(const QGeoCoordinate &t)
{
    QQuickPropertyAnimation::setTo(QVariant::fromValue(t));
}

QVariant geoCoordinateInterpolator(const QGeoCoordinate &from, const QGeoCoordinate &to, qreal progress)
{
    if (from == to) {
        if (progress < 0.5) {
            return QVariant::fromValue(from);
        } else {
            return QVariant::fromValue(to);
        }
    }

    QGeoCoordinate result = QGeoProjection::coordinateInterpolation(from, to, progress);

    return QVariant::fromValue(result);
}

QT_END_NAMESPACE
