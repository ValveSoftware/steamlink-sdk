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

#include "qdeclarativegeolocation_p.h"

QT_USE_NAMESPACE

/*!
    \qmltype Location
    \instantiates QDeclarativeGeoLocation
    \inqmlmodule QtPositioning
    \since 5.2

    \brief The Location type holds location data.

    Location types represent a geographic "location", in a human sense. This
    consists of a specific \l {coordinate}, an \l {address} and a \l {boundingBox}{bounding box}.
    The \l {boundingBox}{bounding box} represents the recommended region
    to display when viewing this location.

    The Location type is most commonly seen as the contents of a search
    model such as the GeocodeModel. When a GeocodeModel returns the list of
    locations found for a given query, it represents these as Location objects.

    \section2 Example Usage

    The following example shows a simple Location object being declared:

    \code
    Location {
        coordinate {
            latitude: -27.3
            longitude: 153.1
        }
        address: Address {
            ...
        }
    }
    \endcode
*/

QDeclarativeGeoLocation::QDeclarativeGeoLocation(QObject *parent)
:   QObject(parent), m_address(0)

{
    setLocation(QGeoLocation());
}

QDeclarativeGeoLocation::QDeclarativeGeoLocation(const QGeoLocation &src, QObject *parent)
:   QObject(parent), m_address(0)
{
    setLocation(src);
}

QDeclarativeGeoLocation::~QDeclarativeGeoLocation()
{
}

/*!
    \qmlproperty QGeoLocation QtPositioning::Location::location

    For details on how to use this property to interface between C++ and QML see
    "\l {Location - QGeoLocation} {Interfaces between C++ and QML Code}".
*/
void QDeclarativeGeoLocation::setLocation(const QGeoLocation &src)
{
    if (m_address && m_address->parent() == this) {
        m_address->setAddress(src.address());
    } else if (!m_address || m_address->parent() != this) {
        m_address = new QDeclarativeGeoAddress(src.address(), this);
        emit addressChanged();
    }

    setCoordinate(src.coordinate());
    setBoundingBox(src.boundingBox());
}

QGeoLocation QDeclarativeGeoLocation::location() const
{
    QGeoLocation retValue;
    retValue.setAddress(m_address ? m_address->address() : QGeoAddress());
    retValue.setCoordinate(m_coordinate);
    retValue.setBoundingBox(m_boundingBox);
    return retValue;
}

/*!
    \qmlproperty Address QtPositioning::Location::address

    This property holds the address of the location which can be use to retrieve address details of the location.
*/
void QDeclarativeGeoLocation::setAddress(QDeclarativeGeoAddress *address)
{
    if (m_address == address)
        return;

    if (m_address && m_address->parent() == this)
        delete m_address;

    m_address = address;
    emit addressChanged();
}

QDeclarativeGeoAddress *QDeclarativeGeoLocation::address() const
{
    return m_address;
}

/*!
    \qmlproperty coordinate QtPositioning::Location::coordinate

    This property holds the exact geographical coordinate of the location which can be used to retrieve the latitude, longitude and altitude of the location.

    \note this property's changed() signal is currently emitted only if the
    whole object changes, not if only the contents of the object change.
*/
void QDeclarativeGeoLocation::setCoordinate(const QGeoCoordinate coordinate)
{
    if (m_coordinate == coordinate)
        return;

    m_coordinate = coordinate;
    emit coordinateChanged();
}

QGeoCoordinate QDeclarativeGeoLocation::coordinate() const
{
    return m_coordinate;
}

/*!
    \qmlproperty georectangle QtPositioning::Location::boundingBox

    This property holds the recommended region to use when displaying the location.
    For example, a building's location may have a region centered around the building,
    but the region is large enough to show it's immediate surrounding geographical
    context.

    Note: this property's changed() signal is currently emitted only if the
    whole object changes, not if only the contents of the object change.
*/
void QDeclarativeGeoLocation::setBoundingBox(const QGeoRectangle &boundingBox)
{
    if (m_boundingBox == boundingBox)
        return;

    m_boundingBox = boundingBox;
    emit boundingBoxChanged();
}

QGeoRectangle QDeclarativeGeoLocation::boundingBox() const
{
    return m_boundingBox;
}
