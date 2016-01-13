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

#include "qdeclarativegeocircle.h"

#include <QtCore/qnumeric.h>
#include <QtPositioning/QGeoCircle>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

/*!
    \qmlbasictype geocircle
    \inqmlmodule QtPositioning
    \ingroup qml-QtPositioning5-basictypes
    \since 5.2

    \brief The geocircle type represents a circular geographic area.

    The \c geocircle type is a \l {geoshape} that represents a circular
    geographic area.  It is defined in terms of a \l {coordinate} which
    specifies the \l center of the circle and a qreal which specifies the \l radius of the circle
    in meters.

    The circle is considered invalid if the \l center coordinate is invalid or if the \l radius is less
    than zero.

    \section1 Example Usage

    Use properties of type \l variant to store a \c {geocircle}.  To create a \c geocircle value,
    use the \l {QtPositioning::circle}{QtPositioning.circle()} function:

    \qml
    import QtPositioning 5.2

    Item {
        property variant region: QtPositioning.circle(QtPositioning.coordinate(-27.5, 153.1), 1000)
    }
    \endqml

    When integrating with C++, note that any QGeoCircle value passed into QML from C++ is
    automatically converted into a \c geocircle value, and vise-versa.

    \section1 Properties

    \section2 center

    \code
    coordinate radius
    \endcode

    This property holds the coordinate of the center of the geocircle.

    \section2 radius

    \code
    real radius
    \endcode

    This property holds the radius of the geocircle in meters.

    The default value for the radius is -1 indicating an invalid geocircle area.
*/

GeoCircleValueType::GeoCircleValueType(QObject *parent)
:   GeoShapeValueType(qMetaTypeId<QGeoCircle>(), parent)
{
}

GeoCircleValueType::~GeoCircleValueType()
{
}

/*
    This property holds the coordinate of the center of the geocircle.
*/
QGeoCoordinate GeoCircleValueType::center()
{
    return QGeoCircle(v).center();
}

void GeoCircleValueType::setCenter(const QGeoCoordinate &coordinate)
{
    QGeoCircle c = v;

    if (c.center() == coordinate)
        return;

    c.setCenter(coordinate);
    v = c;
}

/*
    This property holds the radius of the geocircle in meters.

    The default value for the radius is -1 indicating an invalid geocircle area.
*/
qreal GeoCircleValueType::radius() const
{
    return QGeoCircle(v).radius();
}

void GeoCircleValueType::setRadius(qreal radius)
{
    QGeoCircle c = v;

    if (c.radius() == radius)
        return;

    c.setRadius(radius);
    v = c;
}

QString GeoCircleValueType::toString() const
{
    if (v.type() != QGeoShape::CircleType) {
        qWarning("Not a circle");
        return QStringLiteral("QGeoCircle(not a circle)");
    }

    QGeoCircle c = v;
    return QStringLiteral("QGeoCircle({%1, %2}, %3)")
        .arg(c.center().latitude())
        .arg(c.center().longitude())
        .arg(c.radius());
}

void GeoCircleValueType::setValue(const QVariant &value)
{
    if (value.userType() == qMetaTypeId<QGeoCircle>())
        v = value.value<QGeoCircle>();
    else if (value.userType() == qMetaTypeId<QGeoShape>())
        v = value.value<QGeoShape>();
    else
        v = QGeoCircle();

    onLoad();
}

QVariant GeoCircleValueType::value()
{
    return QVariant::fromValue(QGeoCircle(v));
}

void GeoCircleValueType::write(QObject *obj, int idx, QQmlPropertyPrivate::WriteFlags flags)
{
    QGeoCircle c = v;
    writeProperty(obj, idx, flags, &c);
}

void GeoCircleValueType::writeVariantValue(QObject *obj, int idx, QQmlPropertyPrivate::WriteFlags flags, QVariant *from)
{
    if (from->userType() == qMetaTypeId<QGeoCircle>()) {
        writeProperty(obj, idx, flags, from);
    } else if (from->userType() == qMetaTypeId<QGeoShape>()) {
        QGeoCircle c = from->value<QGeoShape>();
        QVariant v = QVariant::fromValue(c);
        writeProperty(obj, idx, flags, &v);
    } else {
        QVariant v = QVariant::fromValue(QGeoCircle());
        writeProperty(obj, idx, flags, &v);
    }
}

#include "moc_qdeclarativegeocircle.cpp"

QT_END_NAMESPACE
