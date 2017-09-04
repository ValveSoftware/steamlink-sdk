/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qt3dquickvaluetypes_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DCore {
namespace Quick {

namespace Quick3DValueTypes {
    void registerValueTypes()
    {
        QQmlValueTypeFactory::registerValueTypes("Qt3D.Core", 2, 0);
    }
}

QString Quick3DColorValueType::toString() const
{
    // to maintain behaviour with QtQuick 1.0, we just output normal toString() value.
    return QVariant(v).toString();
}

qreal Quick3DColorValueType::r() const
{
    return v.redF();
}

qreal Quick3DColorValueType::g() const
{
    return v.greenF();
}

qreal Quick3DColorValueType::b() const
{
    return v.blueF();
}

qreal Quick3DColorValueType::a() const
{
    return v.alphaF();
}

void Quick3DColorValueType::setR(qreal r)
{
    v.setRedF(r);
}

void Quick3DColorValueType::setG(qreal g)
{
    v.setGreenF(g);
}

void Quick3DColorValueType::setB(qreal b)
{
    v.setBlueF(b);
}

void Quick3DColorValueType::setA(qreal a)
{
    v.setAlphaF(a);
}


QString Quick3DVector2DValueType::toString() const
{
    return QString(QLatin1String("QVector2D(%1, %2)")).arg(v.x()).arg(v.y());
}

qreal Quick3DVector2DValueType::x() const
{
    return v.x();
}

qreal Quick3DVector2DValueType::y() const
{
    return v.y();
}

void Quick3DVector2DValueType::setX(qreal x)
{
    v.setX(x);
}

void Quick3DVector2DValueType::setY(qreal y)
{
    v.setY(y);
}

qreal Quick3DVector2DValueType::dotProduct(const QVector2D &vec) const
{
    return QVector2D::dotProduct(v, vec);
}

QVector2D Quick3DVector2DValueType::times(const QVector2D &vec) const
{
    return v * vec;
}

QVector2D Quick3DVector2DValueType::times(qreal scalar) const
{
    return v * scalar;
}

QVector2D Quick3DVector2DValueType::plus(const QVector2D &vec) const
{
    return v + vec;
}

QVector2D Quick3DVector2DValueType::minus(const QVector2D &vec) const
{
    return v - vec;
}

QVector2D Quick3DVector2DValueType::normalized() const
{
    return v.normalized();
}

qreal Quick3DVector2DValueType::length() const
{
    return v.length();
}

QVector3D Quick3DVector2DValueType::toVector3d() const
{
    return v.toVector3D();
}

QVector4D Quick3DVector2DValueType::toVector4d() const
{
    return v.toVector4D();
}

bool Quick3DVector2DValueType::fuzzyEquals(const QVector2D &vec, qreal epsilon) const
{
    qreal absEps = qAbs(epsilon);
    if (qAbs(v.x() - vec.x()) > absEps)
        return false;
    if (qAbs(v.y() - vec.y()) > absEps)
        return false;
    return true;
}

bool Quick3DVector2DValueType::fuzzyEquals(const QVector2D &vec) const
{
    return qFuzzyCompare(v, vec);
}


QString Quick3DVector3DValueType::toString() const
{
    return QString(QLatin1String("QVector3D(%1, %2, %3)")).arg(v.x()).arg(v.y()).arg(v.z());
}

qreal Quick3DVector3DValueType::x() const
{
    return v.x();
}

qreal Quick3DVector3DValueType::y() const
{
    return v.y();
}

qreal Quick3DVector3DValueType::z() const
{
    return v.z();
}

void Quick3DVector3DValueType::setX(qreal x)
{
    v.setX(x);
}

void Quick3DVector3DValueType::setY(qreal y)
{
    v.setY(y);
}

void Quick3DVector3DValueType::setZ(qreal z)
{
    v.setZ(z);
}

QVector3D Quick3DVector3DValueType::crossProduct(const QVector3D &vec) const
{
    return QVector3D::crossProduct(v, vec);
}

qreal Quick3DVector3DValueType::dotProduct(const QVector3D &vec) const
{
    return QVector3D::dotProduct(v, vec);
}

QVector3D Quick3DVector3DValueType::times(const QMatrix4x4 &m) const
{
    return v * m;
}

QVector3D Quick3DVector3DValueType::times(const QVector3D &vec) const
{
    return v * vec;
}

QVector3D Quick3DVector3DValueType::times(qreal scalar) const
{
    return v * scalar;
}

QVector3D Quick3DVector3DValueType::plus(const QVector3D &vec) const
{
    return v + vec;
}

QVector3D Quick3DVector3DValueType::minus(const QVector3D &vec) const
{
    return v - vec;
}

QVector3D Quick3DVector3DValueType::normalized() const
{
    return v.normalized();
}

qreal Quick3DVector3DValueType::length() const
{
    return v.length();
}

QVector2D Quick3DVector3DValueType::toVector2d() const
{
    return v.toVector2D();
}

QVector4D Quick3DVector3DValueType::toVector4d() const
{
    return v.toVector4D();
}

bool Quick3DVector3DValueType::fuzzyEquals(const QVector3D &vec, qreal epsilon) const
{
    qreal absEps = qAbs(epsilon);
    if (qAbs(v.x() - vec.x()) > absEps)
        return false;
    if (qAbs(v.y() - vec.y()) > absEps)
        return false;
    if (qAbs(v.z() - vec.z()) > absEps)
        return false;
    return true;
}

bool Quick3DVector3DValueType::fuzzyEquals(const QVector3D &vec) const
{
    return qFuzzyCompare(v, vec);
}


QString Quick3DVector4DValueType::toString() const
{
    return QString(QLatin1String("QVector4D(%1, %2, %3, %4)")).arg(v.x()).arg(v.y()).arg(v.z()).arg(v.w());
}

qreal Quick3DVector4DValueType::x() const
{
    return v.x();
}

qreal Quick3DVector4DValueType::y() const
{
    return v.y();
}

qreal Quick3DVector4DValueType::z() const
{
    return v.z();
}

qreal Quick3DVector4DValueType::w() const
{
    return v.w();
}

void Quick3DVector4DValueType::setX(qreal x)
{
    v.setX(x);
}

void Quick3DVector4DValueType::setY(qreal y)
{
    v.setY(y);
}

void Quick3DVector4DValueType::setZ(qreal z)
{
    v.setZ(z);
}

void Quick3DVector4DValueType::setW(qreal w)
{
    v.setW(w);
}

qreal Quick3DVector4DValueType::dotProduct(const QVector4D &vec) const
{
    return QVector4D::dotProduct(v, vec);
}

QVector4D Quick3DVector4DValueType::times(const QVector4D &vec) const
{
    return v * vec;
}

QVector4D Quick3DVector4DValueType::times(const QMatrix4x4 &m) const
{
    return v * m;
}

QVector4D Quick3DVector4DValueType::times(qreal scalar) const
{
    return v * scalar;
}

QVector4D Quick3DVector4DValueType::plus(const QVector4D &vec) const
{
    return v + vec;
}

QVector4D Quick3DVector4DValueType::minus(const QVector4D &vec) const
{
    return v - vec;
}

QVector4D Quick3DVector4DValueType::normalized() const
{
    return v.normalized();
}

qreal Quick3DVector4DValueType::length() const
{
    return v.length();
}

QVector2D Quick3DVector4DValueType::toVector2d() const
{
    return v.toVector2D();
}

QVector3D Quick3DVector4DValueType::toVector3d() const
{
    return v.toVector3D();
}

bool Quick3DVector4DValueType::fuzzyEquals(const QVector4D &vec, qreal epsilon) const
{
    qreal absEps = qAbs(epsilon);
    if (qAbs(v.x() - vec.x()) > absEps)
        return false;
    if (qAbs(v.y() - vec.y()) > absEps)
        return false;
    if (qAbs(v.z() - vec.z()) > absEps)
        return false;
    if (qAbs(v.w() - vec.w()) > absEps)
        return false;
    return true;
}

bool Quick3DVector4DValueType::fuzzyEquals(const QVector4D &vec) const
{
    return qFuzzyCompare(v, vec);
}


QString Quick3DQuaternionValueType::toString() const
{
    return QString(QLatin1String("QQuaternion(%1, %2, %3, %4)")).arg(v.scalar()).arg(v.x()).arg(v.y()).arg(v.z());
}

qreal Quick3DQuaternionValueType::scalar() const
{
    return v.scalar();
}

qreal Quick3DQuaternionValueType::x() const
{
    return v.x();
}

qreal Quick3DQuaternionValueType::y() const
{
    return v.y();
}

qreal Quick3DQuaternionValueType::z() const
{
    return v.z();
}

void Quick3DQuaternionValueType::setScalar(qreal scalar)
{
    v.setScalar(scalar);
}

void Quick3DQuaternionValueType::setX(qreal x)
{
    v.setX(x);
}

void Quick3DQuaternionValueType::setY(qreal y)
{
    v.setY(y);
}

void Quick3DQuaternionValueType::setZ(qreal z)
{
    v.setZ(z);
}


QMatrix4x4 Quick3DMatrix4x4ValueType::times(const QMatrix4x4 &m) const
{
    return v * m;
}

QVector4D Quick3DMatrix4x4ValueType::times(const QVector4D &vec) const
{
    return v * vec;
}

QVector3D Quick3DMatrix4x4ValueType::times(const QVector3D &vec) const
{
    return v * vec;
}

QMatrix4x4 Quick3DMatrix4x4ValueType::times(qreal factor) const
{
    return v * factor;
}

QMatrix4x4 Quick3DMatrix4x4ValueType::plus(const QMatrix4x4 &m) const
{
    return v + m;
}

QMatrix4x4 Quick3DMatrix4x4ValueType::minus(const QMatrix4x4 &m) const
{
    return v - m;
}

QVector4D Quick3DMatrix4x4ValueType::row(int n) const
{
    return v.row(n);
}

QVector4D Quick3DMatrix4x4ValueType::column(int m) const
{
    return v.column(m);
}

qreal Quick3DMatrix4x4ValueType::determinant() const
{
    return v.determinant();
}

QMatrix4x4 Quick3DMatrix4x4ValueType::inverted() const
{
    return v.inverted();
}

QMatrix4x4 Quick3DMatrix4x4ValueType::transposed() const
{
    return v.transposed();
}

bool Quick3DMatrix4x4ValueType::fuzzyEquals(const QMatrix4x4 &m, qreal epsilon) const
{
    qreal absEps = qAbs(epsilon);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (qAbs(v(i,j) - m(i,j)) > absEps) {
                return false;
            }
        }
    }
    return true;
}

bool Quick3DMatrix4x4ValueType::fuzzyEquals(const QMatrix4x4 &m) const
{
    return qFuzzyCompare(v, m);
}

QString Quick3DMatrix4x4ValueType::toString() const
{
    return QString(QLatin1String("QMatrix4x4(%1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, %16)"))
            .arg(v(0, 0)).arg(v(0, 1)).arg(v(0, 2)).arg(v(0, 3))
            .arg(v(1, 0)).arg(v(1, 1)).arg(v(1, 2)).arg(v(1, 3))
            .arg(v(2, 0)).arg(v(2, 1)).arg(v(2, 2)).arg(v(2, 3))
            .arg(v(3, 0)).arg(v(3, 1)).arg(v(3, 2)).arg(v(3, 3));
}

} // namespace Quick
} // namespace Qt3DCore

QT_END_NAMESPACE
