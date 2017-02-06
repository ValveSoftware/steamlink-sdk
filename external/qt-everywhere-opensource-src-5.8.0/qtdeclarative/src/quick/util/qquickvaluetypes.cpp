/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

qreal QQuickColorValueType::hsvHue() const
{
    return v.hsvHueF();
}

qreal QQuickColorValueType::hsvSaturation() const
{
    return v.hsvSaturationF();
}

qreal QQuickColorValueType::hsvValue() const
{
    return v.valueF();
}

qreal QQuickColorValueType::hslHue() const
{
    return v.hslHueF();
}

qreal QQuickColorValueType::hslSaturation() const
{
    return v.hslSaturationF();
}

qreal QQuickColorValueType::hslLightness() const
{
    return v.lightnessF();
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

void QQuickColorValueType::setHsvHue(qreal hsvHue)
{
    qreal hue, saturation, value, alpha;
    v.getHsvF(&hue, &saturation, &value, &alpha);
    v.setHsvF(hsvHue, saturation, value, alpha);
}

void QQuickColorValueType::setHsvSaturation(qreal hsvSaturation)
{
    qreal hue, saturation, value, alpha;
    v.getHsvF(&hue, &saturation, &value, &alpha);
    v.setHsvF(hue, hsvSaturation, value, alpha);
}

void QQuickColorValueType::setHsvValue(qreal hsvValue)
{
    qreal hue, saturation, value, alpha;
    v.getHsvF(&hue, &saturation, &value, &alpha);
    v.setHsvF(hue, saturation, hsvValue, alpha);
}

void QQuickColorValueType::setHslHue(qreal hslHue)
{
    qreal hue, saturation, lightness, alpha;
    v.getHslF(&hue, &saturation, &lightness, &alpha);
    v.setHslF(hslHue, saturation, lightness, alpha);
}

void QQuickColorValueType::setHslSaturation(qreal hslSaturation)
{
    qreal hue, saturation, lightness, alpha;
    v.getHslF(&hue, &saturation, &lightness, &alpha);
    v.setHslF(hue, hslSaturation, lightness, alpha);
}

void QQuickColorValueType::setHslLightness(qreal hslLightness)
{
    qreal hue, saturation, lightness, alpha;
    v.getHslF(&hue, &saturation, &lightness, &alpha);
    v.setHslF(hue, saturation, hslLightness, alpha);
}

QString QQuickVector2DValueType::toString() const
{
    return QString(QLatin1String("QVector2D(%1, %2)")).arg(v.x()).arg(v.y());
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

QString QQuickVector3DValueType::toString() const
{
    return QString(QLatin1String("QVector3D(%1, %2, %3)")).arg(v.x()).arg(v.y()).arg(v.z());
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

QString QQuickVector4DValueType::toString() const
{
    return QString(QLatin1String("QVector4D(%1, %2, %3, %4)")).arg(v.x()).arg(v.y()).arg(v.z()).arg(v.w());
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

QString QQuickFontValueType::styleName() const
{
    return v.styleName();
}

void QQuickFontValueType::setStyleName(const QString &style)
{
    v.setStyleName(style);
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
        return v.pixelSize() * qreal(72.) / qreal(qt_defaultDpi());
    }
    return v.pointSizeF();
}

void QQuickFontValueType::setPointSize(qreal size)
{
    if ((v.resolve() & QFont::SizeResolved) && v.pixelSize() != -1) {
        qWarning() << "Both point size and pixel size set. Using pixel size.";
        return;
    }

    if (size >= 0.0) {
        v.setPointSizeF(size);
    }
}

int QQuickFontValueType::pixelSize() const
{
    if (v.pixelSize() == -1) {
        return (v.pointSizeF() * qt_defaultDpi()) / qreal(72.);
    }
    return v.pixelSize();
}

void QQuickFontValueType::setPixelSize(int size)
{
    if (size >0) {
        if ((v.resolve() & QFont::SizeResolved) && v.pointSizeF() != -1)
            qWarning() << "Both point size and pixel size set. Using pixel size.";
        v.setPixelSize(size);
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

QQuickFontValueType::HintingPreference QQuickFontValueType::hintingPreference() const
{
    return QQuickFontValueType::HintingPreference(v.hintingPreference());
}

void QQuickFontValueType::setHintingPreference(QQuickFontValueType::HintingPreference hintingPreference)
{
    v.setHintingPreference(QFont::HintingPreference(hintingPreference));
}

QT_END_NAMESPACE
