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

#include "qdeclarativegeoshape.h"

#include <QtPositioning/QGeoRectangle>
#include <QtPositioning/QGeoCircle>

QT_BEGIN_NAMESPACE

/*!
    \qmlbasictype geoshape
    \inqmlmodule QtPositioning
    \ingroup qml-QtPositioning5-basictypes
    \since 5.2

    \brief A geoshape type represents an abstract geographic area.

    The \c geoshape type represents an abstract geographic area.  It includes attributes and
    methods common to all geographic areas.  To create objects that represent a valid geographic
    area use \l {georectangle} or \l {geocircle}.

    The \l isValid attribute can be used to test if the geoshape represents a valid geographic
    area.

    The \l isEmpty attribute can be used to test if the geoshape represents a region with a
    geomatrical area of 0.

    The \l {contains}{contains()} method can be used to test if a \l {coordinate} is
    within the geoshape.

    \section1 Example Usage

    Use properties of type \l variant to store a \c {geoshape}.  To create a \c geoshape use one
    of the methods described below.

    To create a \c geoshape value, specify it as a "shape()" string:

    \qml
    import QtPositioning

    Item {
        property variant region: "shape()"
    }
    \endqml

    or with the \l {QtPositioning::shape}{QtPositioning.shape()} function:

    \qml
    import QtPositioning 5.2

    Item {
        property variant region: QtPositioning.shape()
    }
    \endqml

    When integrating with C++, note that any QGeoShape value passed into QML from C++ is
    automatically converted into a \c geoshape value, and vice-versa.

    \section1 Properties

    \section2 isEmpty

    \code
    bool isEmpty
    \endcode

    Returns whether this geo shape is empty. An empty geo shape is a region which has
    a geometrical area of 0.

    \section2 isValid

    \code
    bool isValid
    \endcode

    Returns whether this geo shape is valid.

    A geo shape is considered to be invalid if some of the data that is required to
    unambiguously describe the geo shape has not been set or has been set to an
    unsuitable value.


    \section1 Methods

    \section2 contains()

    \code
    bool contains(coordinate coord)
    \endcode

    Returns true if the \l {QtPositioning::coordinate}{coordinate} specified by \a coord is within
    this geoshape; Otherwise returns false.
*/

GeoShapeValueType::GeoShapeValueType(QObject *parent)
:   QQmlValueTypeBase<QGeoShape>(qMetaTypeId<QGeoShape>(), parent)
{
}

GeoShapeValueType::~GeoShapeValueType()
{
}

GeoShapeValueType::ShapeType GeoShapeValueType::type() const
{
    return static_cast<GeoShapeValueType::ShapeType>(v.type());
}

bool GeoShapeValueType::isValid() const
{
    return v.isValid();
}

bool GeoShapeValueType::isEmpty() const
{
    return v.isEmpty();
}

bool GeoShapeValueType::contains(const QGeoCoordinate &coordinate) const
{
    return v.contains(coordinate);
}

QString GeoShapeValueType::toString() const
{
    switch (v.type()) {
    case QGeoShape::UnknownType:
        return QStringLiteral("QGeoShape()");
    case QGeoShape::RectangleType: {
        QGeoRectangle r = v;
        return QStringLiteral("QGeoRectangle({%1, %2}, {%3, %4})")
            .arg(r.topLeft().latitude())
            .arg(r.topLeft().longitude())
            .arg(r.bottomRight().latitude())
            .arg(r.bottomRight().longitude());
    }
    case QGeoShape::CircleType: {
        QGeoCircle c = v;
        return QStringLiteral("QGeoCircle({%1, %2}, %3)")
            .arg(c.center().latitude())
            .arg(c.center().longitude())
            .arg(c.radius());
    }
    }

    return QStringLiteral("QGeoShape(%1)").arg(v.type());
}

void GeoShapeValueType::setValue(const QVariant &value)
{
    if (value.userType() == qMetaTypeId<QGeoShape>())
        v = value.value<QGeoShape>();
    else if (value.userType() == qMetaTypeId<QGeoRectangle>())
        v = value.value<QGeoRectangle>();
    else if (value.userType() == qMetaTypeId<QGeoCircle>())
        v = value.value<QGeoCircle>();
    else
        v = QGeoShape();

    onLoad();
}

bool GeoShapeValueType::isEqual(const QVariant &other) const
{
    if (other.userType() == qMetaTypeId<QGeoShape>())
        return v == other.value<QGeoShape>();
    else if (other.userType() == qMetaTypeId<QGeoRectangle>())
        return v == other.value<QGeoRectangle>();
    else if (other.userType() == qMetaTypeId<QGeoCircle>())
        return v == other.value<QGeoCircle>();
    else
        return false;
}

GeoShapeValueType::GeoShapeValueType(int userType, QObject *parent)
:   QQmlValueTypeBase<QGeoShape>(userType, parent)
{
    QMetaType::construct(userType, &v, 0);
}

QT_END_NAMESPACE
