/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qv4numberobject_p.h"
#include <QtCore/qnumeric.h>
#include <QtCore/qmath.h>
#include <QtCore/QDebug>
#include <cassert>
#include <double-conversion.h>

using namespace QV4;

DEFINE_OBJECT_VTABLE(NumberCtor);
DEFINE_OBJECT_VTABLE(NumberObject);

NumberCtor::Data::Data(ExecutionContext *scope)
    : FunctionObject::Data(scope, QStringLiteral("Number"))
{
    setVTable(staticVTable());
}

ReturnedValue NumberCtor::construct(Managed *m, CallData *callData)
{
    Scope scope(m->engine());
    double dbl = callData->argc ? callData->args[0].toNumber() : 0.;
    ScopedValue d(scope, QV4::Primitive::fromDouble(dbl));
    return Encode(m->engine()->newNumberObject(d));
}

ReturnedValue NumberCtor::call(Managed *, CallData *callData)
{
    double dbl = callData->argc ? callData->args[0].toNumber() : 0.;
    return Encode(dbl);
}

void NumberPrototype::init(ExecutionEngine *engine, Object *ctor)
{
    Scope scope(engine);
    ScopedObject o(scope);
    ctor->defineReadonlyProperty(engine->id_prototype, (o = this));
    ctor->defineReadonlyProperty(engine->id_length, Primitive::fromInt32(1));

    ctor->defineReadonlyProperty(QStringLiteral("NaN"), Primitive::fromDouble(qSNaN()));
    ctor->defineReadonlyProperty(QStringLiteral("NEGATIVE_INFINITY"), Primitive::fromDouble(-qInf()));
    ctor->defineReadonlyProperty(QStringLiteral("POSITIVE_INFINITY"), Primitive::fromDouble(qInf()));
    ctor->defineReadonlyProperty(QStringLiteral("MAX_VALUE"), Primitive::fromDouble(1.7976931348623158e+308));

#ifdef __INTEL_COMPILER
# pragma warning( push )
# pragma warning(disable: 239)
#endif
    ctor->defineReadonlyProperty(QStringLiteral("MIN_VALUE"), Primitive::fromDouble(5e-324));
#ifdef __INTEL_COMPILER
# pragma warning( pop )
#endif

    defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    defineDefaultProperty(engine->id_toString, method_toString);
    defineDefaultProperty(QStringLiteral("toLocaleString"), method_toLocaleString);
    defineDefaultProperty(engine->id_valueOf, method_valueOf);
    defineDefaultProperty(QStringLiteral("toFixed"), method_toFixed, 1);
    defineDefaultProperty(QStringLiteral("toExponential"), method_toExponential);
    defineDefaultProperty(QStringLiteral("toPrecision"), method_toPrecision);
}

inline ReturnedValue thisNumberValue(ExecutionContext *ctx)
{
    if (ctx->d()->callData->thisObject.isNumber())
        return ctx->d()->callData->thisObject.asReturnedValue();
    NumberObject *n = ctx->d()->callData->thisObject.asNumberObject();
    if (!n)
        return ctx->throwTypeError();
    return n->value().asReturnedValue();
}

inline double thisNumber(ExecutionContext *ctx)
{
    if (ctx->d()->callData->thisObject.isNumber())
        return ctx->d()->callData->thisObject.asDouble();
    NumberObject *n = ctx->d()->callData->thisObject.asNumberObject();
    if (!n)
        return ctx->throwTypeError();
    return n->value().asDouble();
}

ReturnedValue NumberPrototype::method_toString(CallContext *ctx)
{
    double num = thisNumber(ctx);
    if (ctx->d()->engine->hasException)
        return Encode::undefined();

    if (ctx->d()->callData->argc && !ctx->d()->callData->args[0].isUndefined()) {
        int radix = ctx->d()->callData->args[0].toInt32();
        if (radix < 2 || radix > 36)
            return ctx->throwError(QString::fromLatin1("Number.prototype.toString: %0 is not a valid radix")
                            .arg(radix));

        if (std::isnan(num)) {
            return ctx->d()->engine->newString(QStringLiteral("NaN"))->asReturnedValue();
        } else if (qIsInf(num)) {
            return ctx->d()->engine->newString(QLatin1String(num < 0 ? "-Infinity" : "Infinity"))->asReturnedValue();
        }

        if (radix != 10) {
            QString str;
            bool negative = false;
            if (num < 0) {
                negative = true;
                num = -num;
            }
            double frac = num - ::floor(num);
            num = Primitive::toInteger(num);
            do {
                char c = (char)::fmod(num, radix);
                c = (c < 10) ? (c + '0') : (c - 10 + 'a');
                str.prepend(QLatin1Char(c));
                num = ::floor(num / radix);
            } while (num != 0);
            if (frac != 0) {
                str.append(QLatin1Char('.'));
                do {
                    frac = frac * radix;
                    char c = (char)::floor(frac);
                    c = (c < 10) ? (c + '0') : (c - 10 + 'a');
                    str.append(QLatin1Char(c));
                    frac = frac - ::floor(frac);
                } while (frac != 0);
            }
            if (negative)
                str.prepend(QLatin1Char('-'));
            return ctx->d()->engine->newString(str)->asReturnedValue();
        }
    }

    return Primitive::fromDouble(num).toString(ctx)->asReturnedValue();
}

ReturnedValue NumberPrototype::method_toLocaleString(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue v(scope, thisNumberValue(ctx));
    ScopedString str(scope, v->toString(ctx));
    if (ctx->d()->engine->hasException)
        return Encode::undefined();
    return str.asReturnedValue();
}

ReturnedValue NumberPrototype::method_valueOf(CallContext *ctx)
{
    return thisNumberValue(ctx);
}

ReturnedValue NumberPrototype::method_toFixed(CallContext *ctx)
{
    double v = thisNumber(ctx);
    if (ctx->d()->engine->hasException)
        return Encode::undefined();

    double fdigits = 0;

    if (ctx->d()->callData->argc > 0)
        fdigits = ctx->d()->callData->args[0].toInteger();

    if (std::isnan(fdigits))
        fdigits = 0;

    if (fdigits < 0 || fdigits > 20)
        return ctx->throwRangeError(ctx->d()->callData->thisObject);

    QString str;
    if (std::isnan(v))
        str = QString::fromLatin1("NaN");
    else if (qIsInf(v))
        str = QString::fromLatin1(v < 0 ? "-Infinity" : "Infinity");
    else if (v < 1.e21)
        str = QString::number(v, 'f', int (fdigits));
    else
        return RuntimeHelpers::stringFromNumber(ctx, v)->asReturnedValue();
    return ctx->d()->engine->newString(str)->asReturnedValue();
}

ReturnedValue NumberPrototype::method_toExponential(CallContext *ctx)
{
    Scope scope(ctx);
    double d = thisNumber(ctx);
    if (ctx->d()->engine->hasException)
        return Encode::undefined();

    int fdigits = -1;

    if (ctx->d()->callData->argc && !ctx->d()->callData->args[0].isUndefined()) {
        fdigits = ctx->d()->callData->args[0].toInt32();
        if (fdigits < 0 || fdigits > 20) {
            ScopedString error(scope, ctx->d()->engine->newString(QStringLiteral("Number.prototype.toExponential: fractionDigits out of range")));
            return ctx->throwRangeError(error);
        }
    }

    char str[100];
    double_conversion::StringBuilder builder(str, sizeof(str));
    double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToExponential(d, fdigits, &builder);
    QString result = QString::fromLatin1(builder.Finalize());

    return ctx->d()->engine->newString(result)->asReturnedValue();
}

ReturnedValue NumberPrototype::method_toPrecision(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue v(scope, thisNumberValue(ctx));
    if (ctx->d()->engine->hasException)
        return Encode::undefined();

    if (!ctx->d()->callData->argc || ctx->d()->callData->args[0].isUndefined())
        return RuntimeHelpers::toString(ctx, v);

    double precision = ctx->d()->callData->args[0].toInt32();
    if (precision < 1 || precision > 21) {
        ScopedString error(scope, ctx->d()->engine->newString(QStringLiteral("Number.prototype.toPrecision: precision out of range")));
        return ctx->throwRangeError(error);
    }

    char str[100];
    double_conversion::StringBuilder builder(str, sizeof(str));
    double_conversion::DoubleToStringConverter::EcmaScriptConverter().ToPrecision(v->asDouble(), precision, &builder);
    QString result = QString::fromLatin1(builder.Finalize());

    return ctx->d()->engine->newString(result)->asReturnedValue();
}
