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
***************************************************************************/

#include "qdeclarativegeorectangle.h"

#include <QtCore/qnumeric.h>
#include <QtPositioning/QGeoRectangle>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

/*!
    \qmlbasictype georectangle
    \inqmlmodule QtPositioning
    \ingroup qml-QtPositioning5-basictypes
    \since 5.2

    \brief The georectangle type represents a rectangular geographic area.

    The \c georectangle type is a \l {geoshape} that represents a
    rectangular geographic area.  It is defined by a pair of
    \l {coordinate}{coordinates} which represent the top-left and bottom-right corners
    of the \c {georectangle}.  The coordinates are accessible from the \l topLeft and
    \l bottomRight attributes.

    A \c georectangle is considered invalid if the top-left or bottom-right coordinates are invalid
    or if the top-left coordinate is South of the bottom-right coordinate.

    The coordinates of the four corners of the \c georectangle can be accessed with the
    \l {topLeft}, \l {topRight}, \l {bottomLeft} and \l {bottomRight} attributes.  The \l center
    attribute can be used to get the coordinate of the center of the \c georectangle.  The \l width
    and \l height attributes can be used to get the width and height of the \c georectangle in
    degrees.  Setting one of these attributes will cause the other attributes to be adjusted
    accordingly.

    \section1 Limitations

    A \c georectangle can never cross the poles.

    If the height or center of a \c georectangle is adjusted such that it would cross one of the
    poles the height is modified such that the \c georectangle touches but does not cross the pole
    and that the center coordinate is still in the center of the \c georectangle.

    \section1 Example Usage

    Use properties of type \l variant to store a \c {georectangle}.  To create a \c georectangle
    value, use the \l {QtPositioning::rectangle}{QtPositioning.rectangle()} function:

    \qml
    import QtPositioning 5.2

    Item {
        property variant region: QtPositioning.rectangle(QtPositioning.coordinate(-27.5, 153.1), QtPositioning.coordinate(-27.6, 153.2))
    }
    \endqml

    When integrating with C++, note that any QGeoRectangle value passed into QML from C++ is
    automatically converted into a \c georectangle value, and vice-versa.

    \section1 Properties

    \section2 bottomLeft

    \code
    coordinate bottomLeft
    \endcode

    This property holds the bottom left coordinate of this georectangle.

    \section2 bottomRight

    \code
    coordinate bottomRight
    \endcode

    This property holds the bottom right coordinate of this georectangle.

    \section2 center

    \code
    coordinate center
    \endcode

    This property holds the center coordinate of this georectangle. For more details
    see \l {QGeoRectangle::setCenter()}.

    \section2 height

    \code
    double height
    \endcode

    This property holds the height of this georectangle (in degrees). For more details
    see \l {QGeoRectangle::setHeight()}.

    \section2 topLeft

    \code
    coordinate topLeft
    \endcode

    This property holds the top left coordinate of this georectangle.

    \section2 topRight

    \code
    coordinate topRight
    \endcode

    This property holds the top right coordinate of this georectangle.

    \section2 width

    \code
    double width
    \endcode

    This property holds the width of this georectangle (in degrees). For more details
    see \l {QGeoRectangle::setWidth()}.
*/

GeoRectangleValueType::GeoRectangleValueType(QObject *parent)
:   GeoShapeValueType(qMetaTypeId<QGeoRectangle>(), parent)
{
}

GeoRectangleValueType::~GeoRectangleValueType()
{
}

QGeoCoordinate GeoRectangleValueType::bottomLeft()
{
    return QGeoRectangle(v).bottomLeft();
}

/*
    This property holds the bottom left coordinate of this georectangle.
*/
void GeoRectangleValueType::setBottomLeft(const QGeoCoordinate &coordinate)
{
    QGeoRectangle r = v;

    if (r.bottomLeft() == coordinate)
        return;

    r.setBottomLeft(coordinate);
    v = r;
}

QGeoCoordinate GeoRectangleValueType::bottomRight()
{
    return QGeoRectangle(v).bottomRight();
}

/*
    This property holds the bottom right coordinate of this georectangle.
*/
void GeoRectangleValueType::setBottomRight(const QGeoCoordinate &coordinate)
{
    QGeoRectangle r = v;

    if (r.bottomRight() == coordinate)
        return;

    r.setBottomRight(coordinate);
    v = r;
}

QGeoCoordinate GeoRectangleValueType::topLeft()
{
    return QGeoRectangle(v).topLeft();
}

/*
    This property holds the top left coordinate of this georectangle.
*/
void GeoRectangleValueType::setTopLeft(const QGeoCoordinate &coordinate)
{
    QGeoRectangle r = v;

    if (r.topLeft() == coordinate)
        return;

    r.setTopLeft(coordinate);
    v = r;
}

QGeoCoordinate GeoRectangleValueType::topRight()
{
    return QGeoRectangle(v).topRight();
}

/*
    This property holds the top right coordinate of this georectangle.
*/
void GeoRectangleValueType::setTopRight(QGeoCoordinate &coordinate)
{
    QGeoRectangle r = v;

    if (r.topRight() == coordinate)
        return;

    r.setTopRight(coordinate);
    v = r;
}

QGeoCoordinate GeoRectangleValueType::center()
{
    return QGeoRectangle(v).center();
}

/*
    This property holds the center coordinate of this georectangle.
*/
void GeoRectangleValueType::setCenter(const QGeoCoordinate &coordinate)
{
    QGeoRectangle r = v;

    if (r.center() == coordinate)
        return;

    r.setCenter(coordinate);
    v = r;
}

double GeoRectangleValueType::height()
{
    return QGeoRectangle(v).height();
}

/*
    This property holds the height of this georectangle (in degrees).
*/
void GeoRectangleValueType::setHeight(double height)
{
    QGeoRectangle r = v;

    if (!r.isValid())
        r.setCenter(QGeoCoordinate(0.0, 0.0));

    r.setHeight(height);
    v = r;
}

double GeoRectangleValueType::width()
{
    return QGeoRectangle(v).width();
}

/*
    This property holds the width of this georectangle (in degrees).
*/
void GeoRectangleValueType::setWidth(double width)
{
    QGeoRectangle r = v;

    if (!r.isValid())
        r.setCenter(QGeoCoordinate(0.0, 0.0));

    r.setWidth(width);
    v = r;
}

QString GeoRectangleValueType::toString() const
{
    if (v.type() != QGeoShape::RectangleType) {
        qWarning("Not a rectangle a %d\n", v.type());
        return QStringLiteral("QGeoRectangle(not a rectangle)");
    }

    QGeoRectangle r = v;
    return QStringLiteral("QGeoRectangle({%1, %2}, {%3, %4})")
        .arg(r.topLeft().latitude())
        .arg(r.topLeft().longitude())
        .arg(r.bottomRight().latitude())
        .arg(r.bottomRight().longitude());
}

void GeoRectangleValueType::setValue(const QVariant &value)
{
    if (value.userType() == qMetaTypeId<QGeoRectangle>())
        v = value.value<QGeoRectangle>();
    else if (value.userType() == qMetaTypeId<QGeoShape>())
        v = value.value<QGeoShape>();
    else
        v = QGeoRectangle();

    onLoad();
}

QVariant GeoRectangleValueType::value()
{
    return QVariant::fromValue(QGeoRectangle(v));
}

void GeoRectangleValueType::write(QObject *obj, int idx, QQmlPropertyPrivate::WriteFlags flags)
{
    QGeoRectangle r = v;
    writeProperty(obj, idx, flags, &r);
}

void GeoRectangleValueType::writeVariantValue(QObject *obj, int idx, QQmlPropertyPrivate::WriteFlags flags, QVariant *from)
{
    if (from->userType() == qMetaTypeId<QGeoRectangle>()) {
        writeProperty(obj, idx, flags, from);
    } else if (from->userType() == qMetaTypeId<QGeoShape>()) {
        QGeoRectangle r = from->value<QGeoShape>();
        QVariant v = QVariant::fromValue(r);
        writeProperty(obj, idx, flags, &v);
    } else {
        QVariant v = QVariant::fromValue(QGeoRectangle());
        writeProperty(obj, idx, flags, &v);
    }
}

#include "moc_qdeclarativegeorectangle.cpp"

QT_END_NAMESPACE
