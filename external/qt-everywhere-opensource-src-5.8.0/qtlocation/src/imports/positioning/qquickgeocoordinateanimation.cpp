/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qquickgeocoordinateanimation_p.h"
#include "qquickgeocoordinateanimation_p_p.h"
#include <QtQuick/private/qquickanimation_p_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>
#include <QtPositioning/private/qgeoprojection_p.h>
#include <QtPositioning/private/qgeocoordinate_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype CoordinateAnimation
    \instantiates QQuickGeoCoordinateAnimation
    \inherits PropertyAnimation
    \inqmlmodule QtPositioning
    \since 5.3

    \brief A PropertyAnimation for geo coordinate properties.

    A specialized \l{PropertyAnimation} that defines an animation
    between two \l{coordinate}{coordinates}.

    By default, a \l{latitude} of the \l{coordinate} is animated in the direction of shortest
    (geodesic) distance between those coordinates. Since CoordinateAnimation uses Mercator
    map projection, the \l{latitude} animation is always between -90 and 90 degrees.
    The \l{longitude} animation path is not limited and can go over 180 degrees
    in both west and east directions.

    The \l{direction} property can be set to specify the direction in which the \l{longitude}
    animation should occur.

    \sa {Animation and Transitions in Qt Quick}
*/

QVariant q_coordinateInterpolator(const QGeoCoordinate &from, const QGeoCoordinate &to, qreal progress)
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

QVariant q_coordinateShortestInterpolator(const QGeoCoordinate &from, const QGeoCoordinate &to, qreal progress)
{
    const QGeoMercatorCoordinatePrivate* fromMercator =
            static_cast<const QGeoMercatorCoordinatePrivate*>(QGeoCoordinatePrivate::get(&from));
    const QGeoMercatorCoordinatePrivate* toMercator =
            static_cast<const QGeoMercatorCoordinatePrivate*>(QGeoCoordinatePrivate::get(&to));

    double toX = toMercator->m_mercatorX;
    double toY = toMercator->m_mercatorY;
    double fromX = fromMercator->m_mercatorX;
    double fromY = fromMercator->m_mercatorY;
    double x;
    if (0.5 < qAbs(toX - fromX)) {
        // handle dateline crossing
        double ex = toX;
        double sx = fromX;
        if (ex < sx)
            sx -= 1.0;
        else if (sx < ex)
            ex -= 1.0;

        x = fromX + (toX - fromX) * progress;

        if (x < 0.0)
            x += 1.0;

    } else {
        x = fromX + (toX - fromX) * progress;
    }

    double y = fromY + (toY - fromY) * progress;

    QGeoCoordinate result = QGeoProjection::mercatorToCoord(QDoubleVector2D(x, y));
    result.setAltitude(from.altitude() + (to.altitude() - from.altitude()) * progress);
    return QVariant::fromValue(result);
}

QVariant q_coordinateWestInterpolator(const QGeoCoordinate &from, const QGeoCoordinate &to, qreal progress)
{
    const QGeoMercatorCoordinatePrivate* fromMercator =
            static_cast<const QGeoMercatorCoordinatePrivate*>(QGeoCoordinatePrivate::get(&from));
    const QGeoMercatorCoordinatePrivate* toMercator =
            static_cast<const QGeoMercatorCoordinatePrivate*>(QGeoCoordinatePrivate::get(&to));

    double toX = toMercator->m_mercatorX;
    double toY = toMercator->m_mercatorY;
    double fromX = fromMercator->m_mercatorX;
    double fromY = fromMercator->m_mercatorY;
    double diff = toX - fromX;

    while (diff < 0.0) {
        toX += 1.0;
        diff += 1.0;
    }

    double x = fromX + (toX - fromX) * progress;
    double y = fromY + (toY - fromY) * progress;

    while (x > 1.0)
        x -= 1.0;

    QGeoCoordinate result = QGeoProjection::mercatorToCoord(QDoubleVector2D(x, y));
    result.setAltitude(from.altitude() + (to.altitude() - from.altitude()) * progress);

    return QVariant::fromValue(result);
}

QVariant q_coordinateEastInterpolator(const QGeoCoordinate &from, const QGeoCoordinate &to, qreal progress)
{
    const QGeoMercatorCoordinatePrivate* fromMercator =
            static_cast<const QGeoMercatorCoordinatePrivate*>(QGeoCoordinatePrivate::get(&from));
    const QGeoMercatorCoordinatePrivate* toMercator =
            static_cast<const QGeoMercatorCoordinatePrivate*>(QGeoCoordinatePrivate::get(&to));

    double toX = toMercator->m_mercatorX;
    double toY = toMercator->m_mercatorY;
    double fromX = fromMercator->m_mercatorX;
    double fromY = fromMercator->m_mercatorY;
    double diff = toX - fromX;

    while (diff > 0.0) {
        toX -= 1.0;
        diff -= 1.0;
    }

    double x = fromX + (toX - fromX) * progress;
    double y = fromY + (toY - fromY) * progress;

    while (x < 0.0)
        x += 1.0;

    QGeoCoordinate result = QGeoProjection::mercatorToCoord(QDoubleVector2D(x, y));
    result.setAltitude(from.altitude() + (to.altitude() - from.altitude()) * progress);

    return QVariant::fromValue(result);
}

QQuickGeoCoordinateAnimation::QQuickGeoCoordinateAnimation(QObject *parent)
    : QQuickPropertyAnimation(*(new QQuickGeoCoordinateAnimationPrivate), parent)

{
    Q_D(QQuickGeoCoordinateAnimation);
    d->interpolatorType = qMetaTypeId<QGeoCoordinate>();
    d->defaultToInterpolatorType = true;
    d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
}

QQuickGeoCoordinateAnimation::~QQuickGeoCoordinateAnimation()
{
}

/*!
    \qmlproperty coordinate CoordinateAnimation::from
    This property holds the coordinate where the animation should begin.
*/
QGeoCoordinate QQuickGeoCoordinateAnimation::from() const
{
    Q_D(const QQuickGeoCoordinateAnimation);
    return d->from.value<QGeoCoordinate>();
}

void QQuickGeoCoordinateAnimation::setFrom(const QGeoCoordinate &f)
{
    QGeoMercatorCoordinatePrivate *mercator = new QGeoMercatorCoordinatePrivate();
    QDoubleVector2D fromVector = QGeoProjection::coordToMercator(f);
    mercator->lat = f.latitude();
    mercator->lng = f.longitude();
    mercator->alt = f.altitude();
    mercator->m_mercatorX = fromVector.x();
    mercator->m_mercatorY = fromVector.y();
    QGeoCoordinate from(*mercator);
    QQuickPropertyAnimation::setFrom(QVariant::fromValue(from));
}

/*!
    \qmlproperty coordinate CoordinateAnimation::to
    This property holds the coordinate where the animation should end.
*/
QGeoCoordinate QQuickGeoCoordinateAnimation::to() const
{
    Q_D(const QQuickGeoCoordinateAnimation);
    return d->to.value<QGeoCoordinate>();
}

void QQuickGeoCoordinateAnimation::setTo(const QGeoCoordinate &t)
{
    QGeoMercatorCoordinatePrivate *mercator = new QGeoMercatorCoordinatePrivate();
    QDoubleVector2D toVector = QGeoProjection::coordToMercator(t);
    mercator->lat = t.latitude();
    mercator->lng = t.longitude();
    mercator->alt = t.altitude();
    mercator->m_mercatorX = toVector.x();
    mercator->m_mercatorY = toVector.y();
    QGeoCoordinate to(*mercator);
    QQuickPropertyAnimation::setTo(QVariant::fromValue(to));
}

/*!
    \qmlproperty enumeration CoordinateAnimation::direction
    This property holds the direction of the \l{longitude} animation of the \l{coordinate}.

    Possible values are:

    \list
    \li CoordinateAnimation.Shortest (default) - the longitude animation goes in the direction
        that produces the shortest animation path.
    \li CoordinateAnimation.West - the longitude animation always goes into western direction
        and may cross the date line.
    \li CoordinateAnimation.East - the longitude animation always goes into eastern direction
        and may cross the date line.
    \endlist
    \since 5.5
*/


QQuickGeoCoordinateAnimation::Direction QQuickGeoCoordinateAnimation::direction() const
{
    Q_D(const QQuickGeoCoordinateAnimation);
    return d->m_direction;
}

void QQuickGeoCoordinateAnimation::setDirection(QQuickGeoCoordinateAnimation::Direction direction)
{
    Q_D( QQuickGeoCoordinateAnimation);
    if (d->m_direction == direction)
        return;

    d->m_direction = direction;
    switch (direction) {
    case West:
        d->interpolator = reinterpret_cast<QVariantAnimation::Interpolator>(&q_coordinateWestInterpolator);
        break;
    case East:
        d->interpolator = reinterpret_cast<QVariantAnimation::Interpolator>(&q_coordinateEastInterpolator);
        break;
    case Shortest:
    default:
        d->interpolator = reinterpret_cast<QVariantAnimation::Interpolator>(&q_coordinateShortestInterpolator);
        break;
    }
    emit directionChanged();

}

QQuickGeoCoordinateAnimationPrivate::QQuickGeoCoordinateAnimationPrivate():
    m_direction(QQuickGeoCoordinateAnimation::Shortest)
{
}

QT_END_NAMESPACE
