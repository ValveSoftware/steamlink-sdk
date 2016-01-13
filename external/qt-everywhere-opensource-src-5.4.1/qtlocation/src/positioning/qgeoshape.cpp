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

#include "qgeoshape.h"
#include "qgeoshape_p.h"
#include "qgeorectangle.h"
#include "qgeocircle.h"

#ifndef QT_NO_DEBUG_STREAM
#include <QtCore/QDebug>
#endif

#ifndef QT_NO_DATASTREAM
#include <QtCore/QDataStream>
#endif

QT_BEGIN_NAMESPACE

QGeoShapePrivate::QGeoShapePrivate(QGeoShape::ShapeType type)
:   type(type)
{
}

QGeoShapePrivate::~QGeoShapePrivate()
{
}

bool QGeoShapePrivate::operator==(const QGeoShapePrivate &other) const
{
    return type == other.type;
}

/*!
    \class QGeoShape
    \inmodule QtPositioning
    \ingroup QtPositioning-positioning
    \since 5.2

    \brief The QGeoShape class defines a geographic area.

    This class is the base class for classes which specify a geographic
    area.

    For the sake of consistency, subclasses should describe the specific
    details of the associated areas in terms of QGeoCoordinate instances
    and distances in meters.
*/

/*!
    \enum QGeoShape::ShapeType

    Describes the type of the shape.

    \value UnknownType      A shape of unknown type.
    \value RectangleType    A rectangular shape.
    \value CircleType       A circular shape.
*/

inline QGeoShapePrivate *QGeoShape::d_func()
{
    return static_cast<QGeoShapePrivate *>(d_ptr.data());
}

inline const QGeoShapePrivate *QGeoShape::d_func() const
{
    return static_cast<const QGeoShapePrivate *>(d_ptr.constData());
}

/*!
    Constructs a new invalid geo shape of \l UnknownType.
*/
QGeoShape::QGeoShape()
{
}

/*!
    Constructs a new geo shape which is a copy of \a other.
*/
QGeoShape::QGeoShape(const QGeoShape &other)
:   d_ptr(other.d_ptr)
{
}

/*!
    \internal
*/
QGeoShape::QGeoShape(QGeoShapePrivate *d)
:   d_ptr(d)
{
}

/*!
    Destroys this geo shape.
*/
QGeoShape::~QGeoShape()
{
}

/*!
    Returns the type of this geo shape.
*/
QGeoShape::ShapeType QGeoShape::type() const
{
    Q_D(const QGeoShape);

    if (d)
        return d->type;
    else
        return UnknownType;
}

/*!
    Returns whether this geo shape is valid.

    An geo shape is considered to be invalid if some of the data that is required to
    unambiguously describe the geo shape has not been set or has been set to an
    unsuitable value.
*/
bool QGeoShape::isValid() const
{
    Q_D(const QGeoShape);

    if (d)
        return d->isValid();
    else
        return false;
}

/*!
    Returns whether this geo shape is empty.

    An empty geo shape is a region which has a geometrical area of 0.
*/
bool QGeoShape::isEmpty() const
{
    Q_D(const QGeoShape);

    if (d)
        return d->isEmpty();
    else
        return true;
}

/*!
    Returns whether the coordinate \a coordinate is contained within this geo shape.
*/
bool QGeoShape::contains(const QGeoCoordinate &coordinate) const
{
    Q_D(const QGeoShape);

    if (d)
        return d->contains(coordinate);
    else
        return false;
}

/*!
    Extends the geo shape to also cover the coordinate \a coordinate
*/
void QGeoShape::extendShape(const QGeoCoordinate &coordinate)
{
    Q_D(QGeoShape);

    if (d)
        d->extendShape(coordinate);
}


/*!
    Returns true if the \a other geo shape is equivalent to this geo shape, otherwise returns
    false.
*/
bool QGeoShape::operator==(const QGeoShape &other) const
{
    Q_D(const QGeoShape);

    if (d == other.d_func())
        return true;

    if (!d || !(other.d_func()))
        return false;

    return *d == *other.d_func();
}

/*!
    Returns true if the \a other geo shape is not equivalent to this geo shape, otherwise returns
    false.
*/
bool QGeoShape::operator!=(const QGeoShape &other) const
{
    return !(*this == other);
}

/*!
    Assigns \a other to this geo shape and returns a reference to this geo shape.
*/
QGeoShape &QGeoShape::operator=(const QGeoShape &other)
{
    if (this == &other)
        return *this;

    d_ptr = other.d_ptr;
    return *this;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QGeoShape &shape)
{
    //dbg << *shape.d_func();
    dbg.nospace() << "QGeoShape(";
    switch (shape.type()) {
    case QGeoShape::UnknownType:
        dbg.nospace() << "Unknown";
        break;
    case QGeoShape::RectangleType:
        dbg.nospace() << "Rectangle";
        break;
    case QGeoShape::CircleType:
        dbg.nospace() << "Circle";
    }

    dbg.nospace() << ')';

    return dbg;
}
#endif

#ifndef QT_NO_DATASTREAM
QDataStream &operator<<(QDataStream &stream, const QGeoShape &shape)
{
    stream << quint32(shape.type());
    switch (shape.type()) {
    case QGeoShape::UnknownType:
        break;
    case QGeoShape::RectangleType: {
        QGeoRectangle r = shape;
        stream << r.topLeft() << r.bottomRight();
        break;
    }
    case QGeoShape::CircleType: {
        QGeoCircle c = shape;
        stream << c.center() << c.radius();
        break;
    }
    }

    return stream;
}

QDataStream &operator>>(QDataStream &stream, QGeoShape &shape)
{
    quint32 type;
    stream >> type;

    switch (type) {
    case QGeoShape::UnknownType:
        shape = QGeoShape();
        break;
    case QGeoShape::RectangleType: {
        QGeoCoordinate tl;
        QGeoCoordinate br;
        stream >> tl >> br;
        shape = QGeoRectangle(tl, br);
        break;
    }
    case QGeoShape::CircleType: {
        QGeoCoordinate c;
        qreal r;
        stream >> c >> r;
        shape = QGeoCircle(c, r);
        break;
    }
    }

    return stream;
}
#endif

QT_END_NAMESPACE
