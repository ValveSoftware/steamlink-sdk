/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd.
** Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
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

#include <QtCore/QtNumeric>
#include "qdeclarativeposition_p.h"
#include <QtQml/qqml.h>
#include <qnmeapositioninfosource.h>
#include <QFile>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Position
    \instantiates QDeclarativePosition
    \inqmlmodule QtPositioning
    \since 5.2

    \brief The Position type holds positional data at a particular point in time,
    such as coordinate (longitude, latitude, altitude) and speed.

    The Position type holds values related to geographic location such as
    a \l coordinate (longitude, latitude, and altitude), the \l timestamp when
    the Position was obtained, the \l speed at that time, and the accuracy of
    the data.

    Primarily, it is used in the \l{PositionSource::position}{position} property
    of a \l{PositionSource}, as the basic unit of data available from the system
    location data source.

    Not all properties of a Position object are necessarily valid or available
    (for example latitude and longitude may be valid, but speed update has not been
    received or set manually). As a result, corresponding "valid" properties
    are available (for example \l{coordinate} and \l{longitudeValid}, \l{latitudeValid}
    etc) to discern whether the data is available and valid in this position
    update.

    Position objects are read-only and can only be produced by a PositionSource.

    \section2 Example Usage

    See the example given for the \l{PositionSource} type, or the
    \l{geoflickr}{GeoFlickr} example application.

    \sa PositionSource, coordinate
*/

namespace
{

bool equalOrNaN(qreal a, qreal b)
{
    return a == b || (qIsNaN(a) && qIsNaN(b));
}

bool exclusiveNaN(qreal a, qreal b)
{
    return qIsNaN(a) != qIsNaN(b);
}

}

QDeclarativePosition::QDeclarativePosition(QObject *parent)
:   QObject(parent)
{
}

QDeclarativePosition::~QDeclarativePosition()
{
}

void QDeclarativePosition::setPosition(const QGeoPositionInfo &info)
{
    // timestamp
    const QDateTime pTimestamp = m_info.timestamp();
    const QDateTime timestamp = info.timestamp();
    bool emitTimestampChanged = pTimestamp != timestamp;

    // coordinate
    const QGeoCoordinate pCoordinate = m_info.coordinate();
    const QGeoCoordinate coordinate = info.coordinate();
    bool emitCoordinateChanged = pCoordinate != coordinate;
    bool emitLatitudeValidChanged = exclusiveNaN(pCoordinate.latitude(), coordinate.latitude());
    bool emitLongitudeValidChanged = exclusiveNaN(pCoordinate.longitude(), coordinate.longitude());
    bool emitAltitudeValidChanged = exclusiveNaN(pCoordinate.altitude(), coordinate.altitude());

    // direction
    const qreal pDirection = m_info.attribute(QGeoPositionInfo::Direction);
    const qreal direction = info.attribute(QGeoPositionInfo::Direction);
    bool emitDirectionChanged = !equalOrNaN(pDirection, direction);
    bool emitDirectionValidChanged = exclusiveNaN(pDirection, direction);

    // ground speed
    const qreal pSpeed = m_info.attribute(QGeoPositionInfo::GroundSpeed);
    const qreal speed = info.attribute(QGeoPositionInfo::GroundSpeed);
    bool emitSpeedChanged = !equalOrNaN(pSpeed, speed);
    bool emitSpeedValidChanged = exclusiveNaN(pSpeed, speed);

    // vertical speed
    const qreal pVerticalSpeed = m_info.attribute(QGeoPositionInfo::VerticalSpeed);
    const qreal verticalSpeed = info.attribute(QGeoPositionInfo::VerticalSpeed);
    bool emitVerticalSpeedChanged = !equalOrNaN(pVerticalSpeed, verticalSpeed);
    bool emitVerticalSpeedValidChanged = exclusiveNaN(pVerticalSpeed, verticalSpeed);

    // magnetic variation
    const qreal pMagneticVariation = m_info.attribute(QGeoPositionInfo::MagneticVariation);
    const qreal magneticVariation = info.attribute(QGeoPositionInfo::MagneticVariation);
    bool emitMagneticVariationChanged = !equalOrNaN(pMagneticVariation, magneticVariation);
    bool emitMagneticVariationValidChanged = exclusiveNaN(pMagneticVariation, magneticVariation);

    // horizontal accuracy
    const qreal pHorizontalAccuracy = m_info.attribute(QGeoPositionInfo::HorizontalAccuracy);
    const qreal horizontalAccuracy = info.attribute(QGeoPositionInfo::HorizontalAccuracy);
    bool emitHorizontalAccuracyChanged = !equalOrNaN(pHorizontalAccuracy, horizontalAccuracy);
    bool emitHorizontalAccuracyValidChanged = exclusiveNaN(pHorizontalAccuracy, horizontalAccuracy);

    // vertical accuracy
    const qreal pVerticalAccuracy = m_info.attribute(QGeoPositionInfo::VerticalAccuracy);
    const qreal verticalAccuracy = info.attribute(QGeoPositionInfo::VerticalAccuracy);
    bool emitVerticalAccuracyChanged = !equalOrNaN(pVerticalAccuracy, verticalAccuracy);
    bool emitVerticalAccuracyValidChanged = exclusiveNaN(pVerticalAccuracy, verticalAccuracy);

    m_info = info;

    if (emitTimestampChanged)
        emit timestampChanged();
    if (emitCoordinateChanged)
        emit coordinateChanged();
    if (emitLatitudeValidChanged)
        emit latitudeValidChanged();
    if (emitLongitudeValidChanged)
        emit longitudeValidChanged();
    if (emitAltitudeValidChanged)
        emit altitudeValidChanged();
    if (emitDirectionChanged)
        emit directionChanged();
    if (emitDirectionValidChanged)
        emit directionValidChanged();
    if (emitSpeedChanged)
        emit speedChanged();
    if (emitSpeedValidChanged)
        emit speedValidChanged();
    if (emitVerticalSpeedChanged)
        emit verticalSpeedChanged();
    if (emitVerticalSpeedValidChanged)
        emit verticalSpeedValidChanged();
    if (emitHorizontalAccuracyChanged)
        emit horizontalAccuracyChanged();
    if (emitHorizontalAccuracyValidChanged)
        emit horizontalAccuracyValidChanged();
    if (emitVerticalAccuracyChanged)
        emit verticalAccuracyChanged();
    if (emitVerticalAccuracyValidChanged)
        emit verticalAccuracyValidChanged();
    if (emitMagneticVariationChanged)
        emit magneticVariationChanged();
    if (emitMagneticVariationValidChanged)
        emit magneticVariationValidChanged();
}

/*!
    \qmlproperty coordinate Position::coordinate

    This property holds the latitude, longitude, and altitude value of the Position.

    It is a read-only property.

    \sa longitudeValid, latitudeValid, altitudeValid
*/
QGeoCoordinate QDeclarativePosition::coordinate()
{
    return m_info.coordinate();
}

/*!
    \qmlproperty bool Position::latitudeValid

    This property is true if coordinate's latitude has been set
    (to indicate whether that data has been received or not, as every update
    does not necessarily contain all data).

    \sa coordinate
*/
bool QDeclarativePosition::isLatitudeValid() const
{
    return !qIsNaN(m_info.coordinate().latitude());
}


/*!
    \qmlproperty bool Position::longitudeValid

    This property is true if coordinate's longitude has been set
    (to indicate whether that data has been received or not, as every update
    does not necessarily contain all data).

    \sa coordinate
*/
bool QDeclarativePosition::isLongitudeValid() const
{
    return !qIsNaN(m_info.coordinate().longitude());
}


/*!
    \qmlproperty bool Position::speedValid

    This property is true if \l speed has been set
    (to indicate whether that data has been received or not, as every update
    does not necessarily contain all data).

    \sa speed
*/
bool QDeclarativePosition::isSpeedValid() const
{
    return !qIsNaN(m_info.attribute(QGeoPositionInfo::GroundSpeed));
}

/*!
    \qmlproperty bool Position::altitudeValid

    This property is true if coordinate's altitude has been set
    (to indicate whether that data has been received or not, as every update
    does not necessarily contain all data).

    \sa coordinate
*/
bool QDeclarativePosition::isAltitudeValid() const
{
    return !qIsNaN(m_info.coordinate().altitude());
}

/*!
    \qmlproperty double Position::speed

    This property holds the value of speed (groundspeed, meters / second).

    It is a read-only property.

    \sa speedValid, coordinate
*/
double QDeclarativePosition::speed() const
{
    return m_info.attribute(QGeoPositionInfo::GroundSpeed);
}

/*!
    \qmlproperty real Position::horizontalAccuracy

    This property holds the horizontal accuracy of the coordinate (in meters).

    \sa horizontalAccuracyValid, coordinate
*/
void QDeclarativePosition::setHorizontalAccuracy(qreal horizontalAccuracy)
{
    const qreal pHorizontalAccuracy = m_info.attribute(QGeoPositionInfo::HorizontalAccuracy);

    if (equalOrNaN(pHorizontalAccuracy, horizontalAccuracy))
        return;

    bool validChanged = exclusiveNaN(pHorizontalAccuracy, horizontalAccuracy);

    m_info.setAttribute(QGeoPositionInfo::HorizontalAccuracy, horizontalAccuracy);
    emit horizontalAccuracyChanged();
    if (validChanged)
        emit horizontalAccuracyValidChanged();
}

qreal QDeclarativePosition::horizontalAccuracy() const
{
    return m_info.attribute(QGeoPositionInfo::HorizontalAccuracy);
}

/*!
    \qmlproperty bool Position::horizontalAccuracyValid

    This property is true if \l horizontalAccuracy has been set
    (to indicate whether that data has been received or not, as every update
    does not necessarily contain all data).

    \sa horizontalAccuracy
*/
bool QDeclarativePosition::isHorizontalAccuracyValid() const
{
    return !qIsNaN(m_info.attribute(QGeoPositionInfo::HorizontalAccuracy));
}

/*!
    \qmlproperty real Position::verticalAccuracy

    This property holds the vertical accuracy of the coordinate (in meters).

    \sa verticalAccuracyValid, coordinate
*/
void QDeclarativePosition::setVerticalAccuracy(qreal verticalAccuracy)
{
    const qreal pVerticalAccuracy = m_info.attribute(QGeoPositionInfo::VerticalAccuracy);

    if (equalOrNaN(pVerticalAccuracy, verticalAccuracy))
        return;

    bool validChanged = exclusiveNaN(pVerticalAccuracy, verticalAccuracy);

    m_info.setAttribute(QGeoPositionInfo::VerticalAccuracy, verticalAccuracy);
    emit verticalAccuracyChanged();
    if (validChanged)
        emit verticalAccuracyValidChanged();
}

qreal QDeclarativePosition::verticalAccuracy() const
{
    return m_info.attribute(QGeoPositionInfo::VerticalAccuracy);
}

/*!
    \qmlproperty bool Position::verticalAccuracyValid

    This property is true if \l verticalAccuracy has been set
    (to indicate whether that data has been received or not, as every update
    does not necessarily contain all data).

    \sa verticalAccuracy
*/
bool QDeclarativePosition::isVerticalAccuracyValid() const
{
    return !qIsNaN(m_info.attribute(QGeoPositionInfo::VerticalAccuracy));
}

/*!
    \qmlproperty date Position::timestamp

    This property holds the timestamp when this position
    was received. If the property has not been set, it is invalid.

    It is a read-only property.
*/
QDateTime QDeclarativePosition::timestamp() const
{
    return m_info.timestamp();
}

/*!
    \qmlproperty bool Position::directionValid
    \since Qt Positioning 5.3

    This property is true if \l direction has been set (to indicate whether that data has been
    received or not, as every update does not necessarily contain all data).

    \sa direction
*/
bool QDeclarativePosition::isDirectionValid() const
{
    return !qIsNaN(m_info.attribute(QGeoPositionInfo::Direction));
}

/*!
    \qmlproperty double Position::direction
    \since Qt Positioning 5.3

    This property holds the value of the direction of travel in degrees from true north.

    It is a read-only property.

    \sa directionValid
*/
double QDeclarativePosition::direction() const
{
    return m_info.attribute(QGeoPositionInfo::Direction);
}

/*!
    \qmlproperty bool Position::verticalSpeedValid
    \since Qt Positioning 5.3

    This property is true if \l verticalSpeed has been set (to indicate whether that data has been
    received or not, as every update does not necessarily contain all data).

    \sa verticalSpeed
*/
bool QDeclarativePosition::isVerticalSpeedValid() const
{
    return !qIsNaN(m_info.attribute(QGeoPositionInfo::VerticalSpeed));
}

/*!
    \qmlproperty double Position::verticalSpeed
    \since Qt Positioning 5.3

    This property holds the value of the vertical speed in meters per second.

    It is a read-only property.

    \sa verticalSpeedValid
*/
double QDeclarativePosition::verticalSpeed() const
{
    return m_info.attribute(QGeoPositionInfo::VerticalSpeed);
}

/*!
    \qmlproperty bool Position::magneticVariationValid
    \since Qt Positioning 5.4

    This property is true if \l magneticVariation has been set (to indicate whether that data has been
    received or not, as every update does not necessarily contain all data).

    \sa magneticVariation
*/
bool QDeclarativePosition::isMagneticVariationValid() const
{
    return !qIsNaN(m_info.attribute(QGeoPositionInfo::MagneticVariation));
}

/*!
    \qmlproperty double Position::magneticVariation
    \since Qt Positioning 5.4

    This property holds the angle between the horizontal component of the
    magnetic field and true north, in degrees. Also known as magnetic
    declination. A positive value indicates a clockwise direction from
    true north and a negative value indicates a counter-clockwise direction.

    It is a read-only property.

    \sa magneticVariationValid
*/
double QDeclarativePosition::magneticVariation() const
{
    return m_info.attribute(QGeoPositionInfo::MagneticVariation);
}

#include "moc_qdeclarativeposition_p.cpp"

QT_END_NAMESPACE
