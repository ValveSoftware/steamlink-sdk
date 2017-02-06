/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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
#include <qv4engine_p.h>
#include <qv4runtime_p.h>
#include <qv4string_p.h>
#ifndef V4_BOOTSTRAP
#include <qv4object_p.h>
#include <qv4objectproto_p.h>
#include <private/qv4mm_p.h>
#endif

#include <wtf/MathExtras.h>

using namespace QV4;

int Value::toUInt16() const
{
    if (integerCompatible())
        return (ushort)(uint)integerValue();

    double number = toNumber();

    double D16 = 65536.0;
    if ((number >= 0 && number < D16))
        return static_cast<ushort>(number);

    if (!std::isfinite(number))
        return +0;

    double d = ::floor(::fabs(number));
    if (std::signbit(number))
        d = -d;

    number = ::fmod(d , D16);

    if (number < 0)
        number += D16;

    return (unsigned short)number;
}

bool Value::toBoolean() const
{
    switch (type()) {
    case Value::Undefined_Type:
    case Value::Null_Type:
        return false;
    case Value::Boolean_Type:
    case Value::Integer_Type:
        return (bool)int_32();
    case Value::Managed_Type:
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        if (isString())
            return stringValue()->toQString().length() > 0;
#endif
        return true;
    default: // double
        return doubleValue() && !std::isnan(doubleValue());
    }
}

double Value::toInteger() const
{
    if (integerCompatible())
        return int_32();

    return Primitive::toInteger(toNumber());
}

double Value::toNumberImpl() const
{
    switch (type()) {
    case QV4::Value::Undefined_Type:
        return std::numeric_limits<double>::quiet_NaN();
    case QV4::Value::Managed_Type:
#ifdef V4_BOOTSTRAP
        Q_UNIMPLEMENTED();
#else
        if (isString())
            return RuntimeHelpers::stringToNumber(stringValue()->toQString());
    {
        Q_ASSERT(isObject());
        Scope scope(objectValue()->engine());
        ScopedValue prim(scope, RuntimeHelpers::toPrimitive(*this, NUMBER_HINT));
            if (scope.engine->hasException)
                return 0;
            return prim->toNumber();
        }
#endif
    case QV4::Value::Null_Type:
    case QV4::Value::Boolean_Type:
    case QV4::Value::Integer_Type:
        return int_32();
    default: // double
        Q_UNREACHABLE();
    }
}

#ifndef V4_BOOTSTRAP
QString Value::toQStringNoThrow() const
{
    switch (type()) {
    case Value::Empty_Type:
        Q_ASSERT(!"empty Value encountered");
    case Value::Undefined_Type:
        return QStringLiteral("undefined");
    case Value::Null_Type:
        return QStringLiteral("null");
    case Value::Boolean_Type:
        if (booleanValue())
            return QStringLiteral("true");
        else
            return QStringLiteral("false");
    case Value::Managed_Type:
        if (isString())
            return stringValue()->toQString();
        {
            Q_ASSERT(isObject());
            Scope scope(objectValue()->engine());
            ScopedValue ex(scope);
            bool caughtException = false;
            ScopedValue prim(scope, RuntimeHelpers::toPrimitive(*this, STRING_HINT));
            if (scope.hasException()) {
                ex = scope.engine->catchException();
                caughtException = true;
            } else if (prim->isPrimitive()) {
                    return prim->toQStringNoThrow();
            }
            // Can't nest try/catch due to CXX ABI limitations for foreign exception nesting.
            if (caughtException) {
                ScopedValue prim(scope, RuntimeHelpers::toPrimitive(ex, STRING_HINT));
                if (scope.hasException()) {
                    ex = scope.engine->catchException();
                } else if (prim->isPrimitive()) {
                    return prim->toQStringNoThrow();
                }
            }
            return QString();
        }
    case Value::Integer_Type: {
        QString str;
        RuntimeHelpers::numberToString(&str, (double)int_32(), 10);
        return str;
    }
    default: { // double
        QString str;
        RuntimeHelpers::numberToString(&str, doubleValue(), 10);
        return str;
    }
    } // switch
}

QString Value::toQString() const
{
    switch (type()) {
    case Value::Empty_Type:
        Q_ASSERT(!"empty Value encountered");
    case Value::Undefined_Type:
        return QStringLiteral("undefined");
    case Value::Null_Type:
        return QStringLiteral("null");
    case Value::Boolean_Type:
        if (booleanValue())
            return QStringLiteral("true");
        else
            return QStringLiteral("false");
    case Value::Managed_Type:
        if (isString())
            return stringValue()->toQString();
        {
            Q_ASSERT(isObject());
            Scope scope(objectValue()->engine());
            ScopedValue prim(scope, RuntimeHelpers::toPrimitive(*this, STRING_HINT));
            return prim->toQString();
        }
    case Value::Integer_Type: {
        QString str;
        RuntimeHelpers::numberToString(&str, (double)int_32(), 10);
        return str;
    }
    default: { // double
        QString str;
        RuntimeHelpers::numberToString(&str, doubleValue(), 10);
        return str;
    }
    } // switch
}
#endif // V4_BOOTSTRAP

bool Value::sameValue(Value other) const {
    if (_val == other._val)
        return true;
    if (isString() && other.isString())
        return stringValue()->isEqualTo(other.stringValue());
    if (isInteger() && other.isDouble())
        return int_32() ? (double(int_32()) == other.doubleValue()) : (other._val == 0);
    if (isDouble() && other.isInteger())
        return other.int_32() ? (doubleValue() == double(other.int_32())) : (_val == 0);
    return false;
}


int Primitive::toInt32(double number)
{
    const double D32 = 4294967296.0;
    const double D31 = D32 / 2.0;

    if ((number >= -D31 && number < D31))
        return static_cast<int>(number);


    if (!std::isfinite(number))
        return 0;

    double d = ::floor(::fabs(number));
    if (std::signbit(number))
        d = -d;

    number = ::fmod(d , D32);

    if (number < -D31)
        number += D32;
    else if (number >= D31)
        number -= D32;

    return int(number);
}

unsigned int Primitive::toUInt32(double number)
{
    const double D32 = 4294967296.0;
    if ((number >= 0 && number < D32))
        return static_cast<uint>(number);

    if (!std::isfinite(number))
        return +0;

    double d = ::floor(::fabs(number));
    if (std::signbit(number))
        d = -d;

    number = ::fmod(d , D32);

    if (number < 0)
        number += D32;

    return unsigned(number);
}

double Primitive::toInteger(double number)
{
    if (std::isnan(number))
        return +0;
    else if (! number || std::isinf(number))
        return number;
    const double v = floor(fabs(number));
    return std::signbit(number) ? -v : v;
}

#ifndef V4_BOOTSTRAP
Heap::String *Value::toString(ExecutionEngine *e) const
{
    if (isString())
        return stringValue()->d();
    return RuntimeHelpers::convertToString(e, *this);
}

Heap::Object *Value::toObject(ExecutionEngine *e) const
{
    if (isObject())
        return objectValue()->d();
    return RuntimeHelpers::convertToObject(e, *this);
}

uint Value::asArrayLength(bool *ok) const
{
    *ok = true;
    if (isInteger()) {
        if (int_32() >= 0) {
            return (uint)int_32();
        } else {
            *ok = false;
            return UINT_MAX;
        }
    }
    if (isNumber()) {
        double d = doubleValue();
        uint idx = (uint)d;
        if (idx != d) {
            *ok = false;
            return UINT_MAX;
        }
        return idx;
    }
    if (isString())
        return stringValue()->toUInt(ok);

    uint idx = toUInt32();
    double d = toNumber();
    if (d != idx) {
        *ok = false;
        return UINT_MAX;
    }
    return idx;
}
#endif // V4_BOOTSTRAP
