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

#include "qdoublevector3d_p.h"
#include <QtCore/qdatastream.h>
#include <QtCore/qmath.h>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

QDoubleVector3D QDoubleVector3D::normalized() const
{
    // Need some extra precision if the length is very small.
    double len = double(xp) * double(xp) +
                 double(yp) * double(yp) +
                 double(zp) * double(zp);
    if (qFuzzyIsNull(len - 1.0))
        return *this;
    else if (!qFuzzyIsNull(len))
        return *this / (double)qSqrt(len);
    else
        return QDoubleVector3D();
}

void QDoubleVector3D::normalize()
{
    // Need some extra precision if the length is very small.
    double len = double(xp) * double(xp) +
                 double(yp) * double(yp) +
                 double(zp) * double(zp);
    if (qFuzzyIsNull(len - 1.0) || qFuzzyIsNull(len))
        return;

    len = qSqrt(len);

    xp /= len;
    yp /= len;
    zp /= len;
}

QDoubleVector3D QDoubleVector3D::normal(const QDoubleVector3D &v1, const QDoubleVector3D &v2)
{
    return crossProduct(v1, v2).normalized();
}

QDoubleVector3D QDoubleVector3D::normal
        (const QDoubleVector3D &v1, const QDoubleVector3D &v2, const QDoubleVector3D &v3)
{
    return crossProduct((v2 - v1), (v3 - v1)).normalized();
}

double QDoubleVector3D::distanceToPlane
    (const QDoubleVector3D &plane1, const QDoubleVector3D &plane2, const QDoubleVector3D &plane3) const
{
    QDoubleVector3D n = normal(plane2 - plane1, plane3 - plane1);
    return dotProduct(*this - plane1, n);
}

double QDoubleVector3D::distanceToLine
        (const QDoubleVector3D &point, const QDoubleVector3D &direction) const
{
    if (direction.isNull())
        return (*this - point).length();
    QDoubleVector3D p = point + dotProduct(*this - point, direction) * direction;
    return (*this - p).length();
}

double QDoubleVector3D::length() const
{
    return qSqrt(xp * xp + yp * yp + zp * zp);
}

#ifndef QT_NO_DEBUG_STREAM

QDebug operator<<(QDebug dbg, const QDoubleVector3D &vector)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QDoubleVector3D("
        << vector.x() << ", " << vector.y() << ", " << vector.z() << ')';
    return dbg;
}

#endif

#ifndef QT_NO_DATASTREAM

QDataStream &operator<<(QDataStream &stream, const QDoubleVector3D &vector)
{
    stream << double(vector.x()) << double(vector.y())
           << double(vector.z());
    return stream;
}

QDataStream &operator>>(QDataStream &stream, QDoubleVector3D &vector)
{
    double x, y, z;
    stream >> x;
    stream >> y;
    stream >> z;
    vector.setX(double(x));
    vector.setY(double(y));
    vector.setZ(double(z));
    return stream;
}

#endif // QT_NO_DATASTREAM

QT_END_NAMESPACE
