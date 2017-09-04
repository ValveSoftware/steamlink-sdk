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

#include "qv4numberobject_p.h"
#include "qv4runtime_p.h"
#include "qv4string_p.h"

#include <QtCore/qnumeric.h>
#include <QtCore/qmath.h>
#include <QtCore/QDebug>
#include <cassert>
#include <limits>

using namespace QV4;

DEFINE_OBJECT_VTABLE(NumberCtor);
DEFINE_OBJECT_VTABLE(NumberObject);

struct NumberLocaleHolder : public NumberLocale
{
    NumberLocaleHolder() {}
};

Q_GLOBAL_STATIC(NumberLocaleHolder, numberLocaleHolder)

NumberLocale::NumberLocale() : QLocale(QLocale::C),
    // -128 means shortest string that can accurately represent the number.
    defaultDoublePrecision(0xffffff80)
{
    setNumberOptions(QLocale::OmitGroupSeparator |
                     QLocale::OmitLeadingZeroInExponent |
                     QLocale::IncludeTrailingZeroesAfterDot);
}

const NumberLocale *NumberLocale::instance()
{
    return numberLocaleHolder();
}

void Heap::NumberCtor::init(QV4::ExecutionContext *scope)
{
    Heap::FunctionObject::init(scope, QStringLiteral("Number"));
}

void NumberCtor::construct(const Managed *, Scope &scope, CallData *callData)
{
    double dbl = callData->argc ? callData->args[0].toNumber() : 0.;
    scope.result = Encode(scope.engine->newNumberObject(dbl));
}

void NumberCtor::call(const Managed *, Scope &scope, CallData *callData)
{
    double dbl = callData->argc ? callData->args[0].toNumber() : 0.;
    scope.result = Encode(dbl);
}

void NumberPrototype::init(ExecutionEngine *engine, Object *ctor)
{
    Scope scope(engine);
    ScopedObject o(scope);
    ctor->defineReadonlyProperty(engine->id_prototype(), (o = this));
    ctor->defineReadonlyProperty(engine->id_length(), Primitive::fromInt32(1));

    ctor->defineReadonlyProperty(QStringLiteral("NaN"), Primitive::fromDouble(qt_qnan()));
    ctor->defineReadonlyProperty(QStringLiteral("NEGATIVE_INFINITY"), Primitive::fromDouble(-qInf()));
    ctor->defineReadonlyProperty(QStringLiteral("POSITIVE_INFINITY"), Primitive::fromDouble(qInf()));
    ctor->defineReadonlyProperty(QStringLiteral("MAX_VALUE"), Primitive::fromDouble(1.7976931348623158e+308));
    ctor->defineReadonlyProperty(QStringLiteral("EPSILON"), Primitive::fromDouble(std::numeric_limits<double>::epsilon()));

QT_WARNING_PUSH
QT_WARNING_DISABLE_INTEL(239)
    ctor->defineReadonlyProperty(QStringLiteral("MIN_VALUE"), Primitive::fromDouble(5e-324));
QT_WARNING_POP

    ctor->defineDefaultProperty(QStringLiteral("isFinite"), method_isFinite, 1);
    ctor->defineDefaultProperty(QStringLiteral("isNaN"), method_isNaN, 1);

    defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    defineDefaultProperty(engine->id_toString(), method_toString);
    defineDefaultProperty(QStringLiteral("toLocaleString"), method_toLocaleString);
    defineDefaultProperty(engine->id_valueOf(), method_valueOf);
    defineDefaultProperty(QStringLiteral("toFixed"), method_toFixed, 1);
    defineDefaultProperty(QStringLiteral("toExponential"), method_toExponential);
    defineDefaultProperty(QStringLiteral("toPrecision"), method_toPrecision);
}

inline ReturnedValue thisNumberValue(Scope &scope, CallData *callData)
{
    if (callData->thisObject.isNumber())
        return callData->thisObject.asReturnedValue();
    NumberObject *n = callData->thisObject.as<NumberObject>();
    if (!n) {
        scope.engine->throwTypeError();
        return Encode::undefined();
    }
    return Encode(n->value());
}

inline double thisNumber(Scope &scope, CallData *callData)
{
    if (callData->thisObject.isNumber())
        return callData->thisObject.asDouble();
    NumberObject *n = callData->thisObject.as<NumberObject>();
    if (!n) {
        scope.engine->throwTypeError();
        return 0;
    }
    return n->value();
}

void NumberPrototype::method_isFinite(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (!callData->argc) {
        scope.result = Encode(false);
        return;
    }

    double v = callData->args[0].toNumber();
    scope.result = Encode(!std::isnan(v) && !qt_is_inf(v));
}

void NumberPrototype::method_isNaN(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (!callData->argc) {
        scope.result = Encode(false);
        return;
    }

    double v = callData->args[0].toNumber();
    scope.result = Encode(std::isnan(v));
}

void NumberPrototype::method_toString(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double num = thisNumber(scope, callData);
    CHECK_EXCEPTION();

    if (callData->argc && !callData->args[0].isUndefined()) {
        int radix = callData->args[0].toInt32();
        if (radix < 2 || radix > 36) {
            scope.result = scope.engine->throwError(QStringLiteral("Number.prototype.toString: %0 is not a valid radix")
                            .arg(radix));
            return;
        }

        if (std::isnan(num)) {
            scope.result = scope.engine->newString(QStringLiteral("NaN"));
            return;
        } else if (qt_is_inf(num)) {
            scope.result = scope.engine->newString(QLatin1String(num < 0 ? "-Infinity" : "Infinity"));
            return;
        }

        if (radix != 10) {
            QString str;
            bool negative = false;
            if (num < 0) {
                negative = true;
                num = -num;
            }
            double frac = num - std::floor(num);
            num = Primitive::toInteger(num);
            do {
                char c = (char)std::fmod(num, radix);
                c = (c < 10) ? (c + '0') : (c - 10 + 'a');
                str.prepend(QLatin1Char(c));
                num = std::floor(num / radix);
            } while (num != 0);
            if (frac != 0) {
                str.append(QLatin1Char('.'));
                do {
                    frac = frac * radix;
                    char c = (char)std::floor(frac);
                    c = (c < 10) ? (c + '0') : (c - 10 + 'a');
                    str.append(QLatin1Char(c));
                    frac = frac - std::floor(frac);
                } while (frac != 0);
            }
            if (negative)
                str.prepend(QLatin1Char('-'));
            scope.result = scope.engine->newString(str);
            return;
        }
    }

    scope.result = Primitive::fromDouble(num).toString(scope.engine);
}

void NumberPrototype::method_toLocaleString(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedValue v(scope, thisNumberValue(scope, callData));
    scope.result = v->toString(scope.engine);
    CHECK_EXCEPTION();
}

void NumberPrototype::method_valueOf(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    scope.result = thisNumberValue(scope, callData);
}

void NumberPrototype::method_toFixed(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = thisNumber(scope, callData);
    CHECK_EXCEPTION();

    double fdigits = 0;

    if (callData->argc > 0)
        fdigits = callData->args[0].toInteger();

    if (std::isnan(fdigits))
        fdigits = 0;

    if (fdigits < 0 || fdigits > 20) {
        scope.result = scope.engine->throwRangeError(callData->thisObject);
        return;
    }

    QString str;
    if (std::isnan(v))
        str = QStringLiteral("NaN");
    else if (qt_is_inf(v))
        str = QString::fromLatin1(v < 0 ? "-Infinity" : "Infinity");
    else if (v < 1.e21)
        str = NumberLocale::instance()->toString(v, 'f', int(fdigits));
    else {
        scope.result = RuntimeHelpers::stringFromNumber(scope.engine, v);
        return;
    }
    scope.result = scope.engine->newString(str);
}

void NumberPrototype::method_toExponential(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double d = thisNumber(scope, callData);
    CHECK_EXCEPTION();

    int fdigits = NumberLocale::instance()->defaultDoublePrecision;

    if (callData->argc && !callData->args[0].isUndefined()) {
        fdigits = callData->args[0].toInt32();
        if (fdigits < 0 || fdigits > 20) {
            ScopedString error(scope, scope.engine->newString(QStringLiteral("Number.prototype.toExponential: fractionDigits out of range")));
            scope.result = scope.engine->throwRangeError(error);
            return;
        }
    }

    QString result = NumberLocale::instance()->toString(d, 'e', fdigits);
    scope.result = scope.engine->newString(result);
}

void NumberPrototype::method_toPrecision(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedValue v(scope, thisNumberValue(scope, callData));
    CHECK_EXCEPTION();

    if (!callData->argc || callData->args[0].isUndefined()) {
        scope.result = v->toString(scope.engine);
        return;
    }

    int precision = callData->args[0].toInt32();
    if (precision < 1 || precision > 21) {
        ScopedString error(scope, scope.engine->newString(QStringLiteral("Number.prototype.toPrecision: precision out of range")));
        scope.result = scope.engine->throwRangeError(error);
        return;
    }

    QString result = NumberLocale::instance()->toString(v->asDouble(), 'g', precision);
    scope.result = scope.engine->newString(result);
}
