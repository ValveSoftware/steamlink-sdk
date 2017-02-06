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
    setNumberOptions(QLocale::OmitGroupSeparator | QLocale::OmitLeadingZeroInExponent);
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

inline ReturnedValue thisNumberValue(ExecutionContext *ctx)
{
    if (ctx->thisObject().isNumber())
        return ctx->thisObject().asReturnedValue();
    NumberObject *n = ctx->thisObject().as<NumberObject>();
    if (!n)
        return ctx->engine()->throwTypeError();
    return Encode(n->value());
}

inline double thisNumber(ExecutionContext *ctx)
{
    if (ctx->thisObject().isNumber())
        return ctx->thisObject().asDouble();
    NumberObject *n = ctx->thisObject().as<NumberObject>();
    if (!n)
        return ctx->engine()->throwTypeError();
    return n->value();
}

ReturnedValue NumberPrototype::method_isFinite(CallContext *ctx)
{
    if (!ctx->argc())
        return Encode(false);

    double v = ctx->args()[0].toNumber();
    return Encode(!std::isnan(v) && !qt_is_inf(v));
}

ReturnedValue NumberPrototype::method_isNaN(CallContext *ctx)
{
    if (!ctx->argc())
        return Encode(false);

    double v = ctx->args()[0].toNumber();
    return Encode(std::isnan(v));
}

ReturnedValue NumberPrototype::method_toString(CallContext *ctx)
{
    Scope scope(ctx);
    double num = thisNumber(ctx);
    if (scope.engine->hasException)
        return Encode::undefined();

    if (ctx->argc() && !ctx->args()[0].isUndefined()) {
        int radix = ctx->args()[0].toInt32();
        if (radix < 2 || radix > 36)
            return ctx->engine()->throwError(QStringLiteral("Number.prototype.toString: %0 is not a valid radix")
                            .arg(radix));

        if (std::isnan(num)) {
            return scope.engine->newString(QStringLiteral("NaN"))->asReturnedValue();
        } else if (qt_is_inf(num)) {
            return scope.engine->newString(QLatin1String(num < 0 ? "-Infinity" : "Infinity"))->asReturnedValue();
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
            return scope.engine->newString(str)->asReturnedValue();
        }
    }

    return Primitive::fromDouble(num).toString(scope.engine)->asReturnedValue();
}

ReturnedValue NumberPrototype::method_toLocaleString(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue v(scope, thisNumberValue(ctx));
    ScopedString str(scope, v->toString(scope.engine));
    if (scope.engine->hasException)
        return Encode::undefined();
    return str.asReturnedValue();
}

ReturnedValue NumberPrototype::method_valueOf(CallContext *ctx)
{
    return thisNumberValue(ctx);
}

ReturnedValue NumberPrototype::method_toFixed(CallContext *ctx)
{
    Scope scope(ctx);
    double v = thisNumber(ctx);
    if (scope.engine->hasException)
        return Encode::undefined();

    double fdigits = 0;

    if (ctx->argc() > 0)
        fdigits = ctx->args()[0].toInteger();

    if (std::isnan(fdigits))
        fdigits = 0;

    if (fdigits < 0 || fdigits > 20)
        return ctx->engine()->throwRangeError(ctx->thisObject());

    QString str;
    if (std::isnan(v))
        str = QStringLiteral("NaN");
    else if (qt_is_inf(v))
        str = QString::fromLatin1(v < 0 ? "-Infinity" : "Infinity");
    else if (v < 1.e21)
        str = NumberLocale::instance()->toString(v, 'f', int(fdigits));
    else
        return RuntimeHelpers::stringFromNumber(ctx->engine(), v)->asReturnedValue();
    return scope.engine->newString(str)->asReturnedValue();
}

ReturnedValue NumberPrototype::method_toExponential(CallContext *ctx)
{
    Scope scope(ctx);
    double d = thisNumber(ctx);
    if (scope.engine->hasException)
        return Encode::undefined();

    int fdigits = NumberLocale::instance()->defaultDoublePrecision;

    if (ctx->argc() && !ctx->args()[0].isUndefined()) {
        fdigits = ctx->args()[0].toInt32();
        if (fdigits < 0 || fdigits > 20) {
            ScopedString error(scope, scope.engine->newString(QStringLiteral("Number.prototype.toExponential: fractionDigits out of range")));
            return ctx->engine()->throwRangeError(error);
        }
    }

    QString result = NumberLocale::instance()->toString(d, 'e', fdigits);
    return scope.engine->newString(result)->asReturnedValue();
}

ReturnedValue NumberPrototype::method_toPrecision(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue v(scope, thisNumberValue(ctx));
    if (scope.engine->hasException)
        return Encode::undefined();

    if (!ctx->argc() || ctx->args()[0].isUndefined())
        return RuntimeHelpers::toString(scope.engine, v);

    int precision = ctx->args()[0].toInt32();
    if (precision < 1 || precision > 21) {
        ScopedString error(scope, scope.engine->newString(QStringLiteral("Number.prototype.toPrecision: precision out of range")));
        return ctx->engine()->throwRangeError(error);
    }

    // TODO: Once we get a NumberOption to retain trailing zeroes, replace the code below with:
    // QString result = NumberLocale::instance()->toString(v->asDouble(), 'g', precision);
    QByteArray format = "%#." + QByteArray::number(precision) + "g";
    QString result = QString::asprintf(format.constData(), v->asDouble());
    if (result.endsWith(QLatin1Char('.'))) {
        // This is 'f' notation, not 'e'.
        result.chop(1);
    } else {
        int ePos = result.indexOf(QLatin1Char('e'));
        if (ePos != -1) {
            Q_ASSERT(ePos + 2 < result.length()); // always '+' or '-', and number, after 'e'
            Q_ASSERT(ePos > 0);                   // 'e' is not the first character

            if (result.at(ePos + 2) == QLatin1Char('0')) // Drop leading zeroes in exponent
                result = result.remove(ePos + 2, 1);
            if (result.at(ePos - 1) == QLatin1Char('.')) // Drop trailing dots before 'e'
                result = result.remove(ePos - 1, 1);
        }
    }
    return scope.engine->newString(result)->asReturnedValue();
}
