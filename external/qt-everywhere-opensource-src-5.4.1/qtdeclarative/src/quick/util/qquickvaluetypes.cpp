/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include <private/qquickvaluetypes_p.h>

#include <qtquickglobal.h>
#include <private/qqmlvaluetype_p.h>
#include <private/qfont_p.h>


QT_BEGIN_NAMESPACE

namespace QQuickValueTypes {
    void registerValueTypes()
    {
        QQmlValueTypeFactory::registerValueTypes("QtQuick", 2, 0);
        qmlRegisterValueTypeEnums<QQuickFontValueType>("QtQuick", 2, 0, "Font");
    }
}

QQuickColorValueType::QQuickColorValueType(QObject *parent)
    : QQmlValueTypeBase<QColor>(QMetaType::QColor, parent)
{
}

QString QQuickColorValueType::toString() const
{
    // to maintain behaviour with QtQuick 1.0, we just output normal toString() value.
    return QVariant(v).toString();
}

qreal QQuickColorValueType::r() const
{
    return v.redF();
}

qreal QQuickColorValueType::g() const
{
    return v.greenF();
}

qreal QQuickColorValueType::b() const
{
    return v.blueF();
}

qreal QQuickColorValueType::a() const
{
    return v.alphaF();
}

void QQuickColorValueType::setR(qreal r)
{
    v.setRedF(r);
}

void QQuickColorValueType::setG(qreal g)
{
    v.setGreenF(g);
}

void QQuickColorValueType::setB(qreal b)
{
    v.setBlueF(b);
}

void QQuickColorValueType::setA(qreal a)
{
    v.setAlphaF(a);
}


QQuickVector2DValueType::QQuickVector2DValueType(QObject *parent)
    : QQmlValueTypeBase<QVector2D>(QMetaType::QVector2D, parent)
{
}

QString QQuickVector2DValueType::toString() const
{
    return QString(QLatin1String("QVector2D(%1, %2)")).arg(v.x()).arg(v.y());
}

bool QQuickVector2DValueType::isEqual(const QVariant &other) const
{
    if (other.userType() != QMetaType::QVector2D)
        return false;

    QVector2D otherVector = other.value<QVector2D>();
    return (v == otherVector);
}

qreal QQuickVector2DValueType::x() const
{
    return v.x();
}

qreal QQuickVector2DValueType::y() const
{
    return v.y();
}

void QQuickVector2DValueType::setX(qreal x)
{
    v.setX(x);
}

void QQuickVector2DValueType::setY(qreal y)
{
    v.setY(y);
}

qreal QQuickVector2DValueType::dotProduct(const QVector2D &vec) const
{
    return QVector2D::dotProduct(v, vec);
}

QVector2D QQuickVector2DValueType::times(const QVector2D &vec) const
{
    return v * vec;
}

QVector2D QQuickVector2DValueType::times(qreal scalar) const
{
    return v * scalar;
}

QVector2D QQuickVector2DValueType::plus(const QVector2D &vec) const
{
    return v + vec;
}

QVector2D QQuickVector2DValueType::minus(const QVector2D &vec) const
{
    return v - vec;
}

QVector2D QQuickVector2DValueType::normalized() const
{
    return v.normalized();
}

qreal QQuickVector2DValueType::length() const
{
    return v.length();
}

QVector3D QQuickVector2DValueType::toVector3d() const
{
    return v.toVector3D();
}

QVector4D QQuickVector2DValueType::toVector4d() const
{
    return v.toVector4D();
}

bool QQuickVector2DValueType::fuzzyEquals(const QVector2D &vec, qreal epsilon) const
{
    qreal absEps = qAbs(epsilon);
    if (qAbs(v.x() - vec.x()) > absEps)
        return false;
    if (qAbs(v.y() - vec.y()) > absEps)
        return false;
    return true;
}

bool QQuickVector2DValueType::fuzzyEquals(const QVector2D &vec) const
{
    return qFuzzyCompare(v, vec);
}


QQuickVector3DValueType::QQuickVector3DValueType(QObject *parent)
    : QQmlValueTypeBase<QVector3D>(QMetaType::QVector3D, parent)
{
}

QString QQuickVector3DValueType::toString() const
{
    return QString(QLatin1String("QVector3D(%1, %2, %3)")).arg(v.x()).arg(v.y()).arg(v.z());
}

bool QQuickVector3DValueType::isEqual(const QVariant &other) const
{
    if (other.userType() != QMetaType::QVector3D)
        return false;

    QVector3D otherVector = other.value<QVector3D>();
    return (v == otherVector);
}

qreal QQuickVector3DValueType::x() const
{
    return v.x();
}

qreal QQuickVector3DValueType::y() const
{
    return v.y();
}

qreal QQuickVector3DValueType::z() const
{
    return v.z();
}

void QQuickVector3DValueType::setX(qreal x)
{
    v.setX(x);
}

void QQuickVector3DValueType::setY(qreal y)
{
    v.setY(y);
}

void QQuickVector3DValueType::setZ(qreal z)
{
    v.setZ(z);
}

QVector3D QQuickVector3DValueType::crossProduct(const QVector3D &vec) const
{
    return QVector3D::crossProduct(v, vec);
}

qreal QQuickVector3DValueType::dotProduct(const QVector3D &vec) const
{
    return QVector3D::dotProduct(v, vec);
}

QVector3D QQuickVector3DValueType::times(const QMatrix4x4 &m) const
{
    return v * m;
}

QVector3D QQuickVector3DValueType::times(const QVector3D &vec) const
{
    return v * vec;
}

QVector3D QQuickVector3DValueType::times(qreal scalar) const
{
    return v * scalar;
}

QVector3D QQuickVector3DValueType::plus(const QVector3D &vec) const
{
    return v + vec;
}

QVector3D QQuickVector3DValueType::minus(const QVector3D &vec) const
{
    return v - vec;
}

QVector3D QQuickVector3DValueType::normalized() const
{
    return v.normalized();
}

qreal QQuickVector3DValueType::length() const
{
    return v.length();
}

QVector2D QQuickVector3DValueType::toVector2d() const
{
    return v.toVector2D();
}

QVector4D QQuickVector3DValueType::toVector4d() const
{
    return v.toVector4D();
}

bool QQuickVector3DValueType::fuzzyEquals(const QVector3D &vec, qreal epsilon) const
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

bool QQuickVector3DValueType::fuzzyEquals(const QVector3D &vec) const
{
    return qFuzzyCompare(v, vec);
}


QQuickVector4DValueType::QQuickVector4DValueType(QObject *parent)
    : QQmlValueTypeBase<QVector4D>(QMetaType::QVector4D, parent)
{
}

QString QQuickVector4DValueType::toString() const
{
    return QString(QLatin1String("QVector4D(%1, %2, %3, %4)")).arg(v.x()).arg(v.y()).arg(v.z()).arg(v.w());
}

bool QQuickVector4DValueType::isEqual(const QVariant &other) const
{
    if (other.userType() != QMetaType::QVector4D)
        return false;

    QVector4D otherVector = other.value<QVector4D>();
    return (v == otherVector);
}

qreal QQuickVector4DValueType::x() const
{
    return v.x();
}

qreal QQuickVector4DValueType::y() const
{
    return v.y();
}

qreal QQuickVector4DValueType::z() const
{
    return v.z();
}

qreal QQuickVector4DValueType::w() const
{
    return v.w();
}

void QQuickVector4DValueType::setX(qreal x)
{
    v.setX(x);
}

void QQuickVector4DValueType::setY(qreal y)
{
    v.setY(y);
}

void QQuickVector4DValueType::setZ(qreal z)
{
    v.setZ(z);
}

void QQuickVector4DValueType::setW(qreal w)
{
    v.setW(w);
}

qreal QQuickVector4DValueType::dotProduct(const QVector4D &vec) const
{
    return QVector4D::dotProduct(v, vec);
}

QVector4D QQuickVector4DValueType::times(const QVector4D &vec) const
{
    return v * vec;
}

QVector4D QQuickVector4DValueType::times(const QMatrix4x4 &m) const
{
    return v * m;
}

QVector4D QQuickVector4DValueType::times(qreal scalar) const
{
    return v * scalar;
}

QVector4D QQuickVector4DValueType::plus(const QVector4D &vec) const
{
    return v + vec;
}

QVector4D QQuickVector4DValueType::minus(const QVector4D &vec) const
{
    return v - vec;
}

QVector4D QQuickVector4DValueType::normalized() const
{
    return v.normalized();
}

qreal QQuickVector4DValueType::length() const
{
    return v.length();
}

QVector2D QQuickVector4DValueType::toVector2d() const
{
    return v.toVector2D();
}

QVector3D QQuickVector4DValueType::toVector3d() const
{
    return v.toVector3D();
}

bool QQuickVector4DValueType::fuzzyEquals(const QVector4D &vec, qreal epsilon) const
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

bool QQuickVector4DValueType::fuzzyEquals(const QVector4D &vec) const
{
    return qFuzzyCompare(v, vec);
}

QQuickQuaternionValueType::QQuickQuaternionValueType(QObject *parent)
    : QQmlValueTypeBase<QQuaternion>(QMetaType::QQuaternion, parent)
{
}

QString QQuickQuaternionValueType::toString() const
{
    return QString(QLatin1String("QQuaternion(%1, %2, %3, %4)")).arg(v.scalar()).arg(v.x()).arg(v.y()).arg(v.z());
}

qreal QQuickQuaternionValueType::scalar() const
{
    return v.scalar();
}

qreal QQuickQuaternionValueType::x() const
{
    return v.x();
}

qreal QQuickQuaternionValueType::y() const
{
    return v.y();
}

qreal QQuickQuaternionValueType::z() const
{
    return v.z();
}

void QQuickQuaternionValueType::setScalar(qreal scalar)
{
    v.setScalar(scalar);
}

void QQuickQuaternionValueType::setX(qreal x)
{
    v.setX(x);
}

void QQuickQuaternionValueType::setY(qreal y)
{
    v.setY(y);
}

void QQuickQuaternionValueType::setZ(qreal z)
{
    v.setZ(z);
}


QQuickMatrix4x4ValueType::QQuickMatrix4x4ValueType(QObject *parent)
    : QQmlValueTypeBase<QMatrix4x4>(QMetaType::QMatrix4x4, parent)
{
}

QMatrix4x4 QQuickMatrix4x4ValueType::times(const QMatrix4x4 &m) const
{
    return v * m;
}

QVector4D QQuickMatrix4x4ValueType::times(const QVector4D &vec) const
{
    return v * vec;
}

QVector3D QQuickMatrix4x4ValueType::times(const QVector3D &vec) const
{
    return v * vec;
}

QMatrix4x4 QQuickMatrix4x4ValueType::times(qreal factor) const
{
    return v * factor;
}

QMatrix4x4 QQuickMatrix4x4ValueType::plus(const QMatrix4x4 &m) const
{
    return v + m;
}

QMatrix4x4 QQuickMatrix4x4ValueType::minus(const QMatrix4x4 &m) const
{
    return v - m;
}

QVector4D QQuickMatrix4x4ValueType::row(int n) const
{
    return v.row(n);
}

QVector4D QQuickMatrix4x4ValueType::column(int m) const
{
    return v.column(m);
}

qreal QQuickMatrix4x4ValueType::determinant() const
{
    return v.determinant();
}

QMatrix4x4 QQuickMatrix4x4ValueType::inverted() const
{
    return v.inverted();
}

QMatrix4x4 QQuickMatrix4x4ValueType::transposed() const
{
    return v.transposed();
}

bool QQuickMatrix4x4ValueType::fuzzyEquals(const QMatrix4x4 &m, qreal epsilon) const
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

bool QQuickMatrix4x4ValueType::fuzzyEquals(const QMatrix4x4 &m) const
{
    return qFuzzyCompare(v, m);
}

QString QQuickMatrix4x4ValueType::toString() const
{
    return QString(QLatin1String("QMatrix4x4(%1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, %16)"))
            .arg(v(0, 0)).arg(v(0, 1)).arg(v(0, 2)).arg(v(0, 3))
            .arg(v(1, 0)).arg(v(1, 1)).arg(v(1, 2)).arg(v(1, 3))
            .arg(v(2, 0)).arg(v(2, 1)).arg(v(2, 2)).arg(v(2, 3))
            .arg(v(3, 0)).arg(v(3, 1)).arg(v(3, 2)).arg(v(3, 3));
}

bool QQuickMatrix4x4ValueType::isEqual(const QVariant &other) const
{
    if (other.userType() != qMetaTypeId<QMatrix4x4>())
        return false;

    QMatrix4x4 otherMatrix = other.value<QMatrix4x4>();
    return (v == otherMatrix);

}

QQuickFontValueType::QQuickFontValueType(QObject *parent)
    : QQmlValueTypeBase<QFont>(QMetaType::QFont, parent),
      pixelSizeSet(false),
      pointSizeSet(false)
{
}

void QQuickFontValueType::onLoad()
{
    pixelSizeSet = false;
    pointSizeSet = false;
}

QString QQuickFontValueType::toString() const
{
    return QString(QLatin1String("QFont(%1)")).arg(v.toString());
}

QString QQuickFontValueType::family() const
{
    return v.family();
}

void QQuickFontValueType::setFamily(const QString &family)
{
    v.setFamily(family);
}

bool QQuickFontValueType::bold() const
{
    return v.bold();
}

void QQuickFontValueType::setBold(bool b)
{
    v.setBold(b);
}

QQuickFontValueType::FontWeight QQuickFontValueType::weight() const
{
    return (QQuickFontValueType::FontWeight)v.weight();
}

void QQuickFontValueType::setWeight(QQuickFontValueType::FontWeight w)
{
    v.setWeight((QFont::Weight)w);
}

bool QQuickFontValueType::italic() const
{
    return v.italic();
}

void QQuickFontValueType::setItalic(bool b)
{
    v.setItalic(b);
}

bool QQuickFontValueType::underline() const
{
    return v.underline();
}

void QQuickFontValueType::setUnderline(bool b)
{
    v.setUnderline(b);
}

bool QQuickFontValueType::overline() const
{
    return v.overline();
}

void QQuickFontValueType::setOverline(bool b)
{
    v.setOverline(b);
}

bool QQuickFontValueType::strikeout() const
{
    return v.strikeOut();
}

void QQuickFontValueType::setStrikeout(bool b)
{
    v.setStrikeOut(b);
}

qreal QQuickFontValueType::pointSize() const
{
    if (v.pointSizeF() == -1) {
        if (dpi.isNull)
            dpi = qt_defaultDpi();
        return v.pixelSize() * qreal(72.) / qreal(dpi);
    }
    return v.pointSizeF();
}

void QQuickFontValueType::setPointSize(qreal size)
{
    if (pixelSizeSet) {
        qWarning() << "Both point size and pixel size set. Using pixel size.";
        return;
    }

    if (size >= 0.0) {
        pointSizeSet = true;
        v.setPointSizeF(size);
    } else {
        pointSizeSet = false;
    }
}

int QQuickFontValueType::pixelSize() const
{
    if (v.pixelSize() == -1) {
        if (dpi.isNull)
            dpi = qt_defaultDpi();
        return (v.pointSizeF() * dpi) / qreal(72.);
    }
    return v.pixelSize();
}

void QQuickFontValueType::setPixelSize(int size)
{
    if (size >0) {
        if (pointSizeSet)
            qWarning() << "Both point size and pixel size set. Using pixel size.";
        v.setPixelSize(size);
        pixelSizeSet = true;
    } else {
        pixelSizeSet = false;
    }
}

QQuickFontValueType::Capitalization QQuickFontValueType::capitalization() const
{
    return (QQuickFontValueType::Capitalization)v.capitalization();
}

void QQuickFontValueType::setCapitalization(QQuickFontValueType::Capitalization c)
{
    v.setCapitalization((QFont::Capitalization)c);
}

qreal QQuickFontValueType::letterSpacing() const
{
    return v.letterSpacing();
}

void QQuickFontValueType::setLetterSpacing(qreal size)
{
    v.setLetterSpacing(QFont::AbsoluteSpacing, size);
}

qreal QQuickFontValueType::wordSpacing() const
{
    return v.wordSpacing();
}

void QQuickFontValueType::setWordSpacing(qreal size)
{
    v.setWordSpacing(size);
}

QT_END_NAMESPACE
