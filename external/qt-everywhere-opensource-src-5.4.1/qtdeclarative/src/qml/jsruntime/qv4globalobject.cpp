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

#include "qv4globalobject_p.h"
#include "qv4mm_p.h"
#include "qv4value_inl_p.h"
#include "qv4context_p.h"
#include "qv4function_p.h"
#include "qv4debugging_p.h"
#include "qv4script_p.h"
#include "qv4scopedvalue_p.h"

#include <private/qqmljsengine_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljsast_p.h>
#include <qv4jsir_p.h>
#include <qv4codegen_p.h>
#include "private/qlocale_tools_p.h"

#include <QtCore/QDebug>
#include <QtCore/QString>
#include <iostream>
#include "qv4alloca_p.h"

#include <wtf/MathExtras.h>

using namespace QV4;


static inline char toHex(char c)
{
    static const char hexnumbers[] = "0123456789ABCDEF";
    return hexnumbers[c & 0xf];
}

static int fromHex(QChar ch)
{
    ushort c = ch.unicode();
    if ((c >= '0') && (c <= '9'))
        return c - '0';
    if ((c >= 'A') && (c <= 'F'))
        return c - 'A' + 10;
    if ((c >= 'a') && (c <= 'f'))
        return c - 'a' + 10;
    return -1;
}

static QString escape(const QString &input)
{
    QString output;
    output.reserve(input.size() * 3);
    const int length = input.length();
    for (int i = 0; i < length; ++i) {
        ushort uc = input.at(i).unicode();
        if (uc < 0x100) {
            if (   (uc > 0x60 && uc < 0x7B)
                || (uc > 0x3F && uc < 0x5B)
                || (uc > 0x2C && uc < 0x3A)
                || (uc == 0x2A)
                || (uc == 0x2B)
                || (uc == 0x5F)) {
                output.append(QChar(uc));
            } else {
                output.append('%');
                output.append(QLatin1Char(toHex(uc >> 4)));
                output.append(QLatin1Char(toHex(uc)));
            }
        } else {
            output.append('%');
            output.append('u');
            output.append(QLatin1Char(toHex(uc >> 12)));
            output.append(QLatin1Char(toHex(uc >> 8)));
            output.append(QLatin1Char(toHex(uc >> 4)));
            output.append(QLatin1Char(toHex(uc)));
        }
    }
    return output;
}

static QString unescape(const QString &input)
{
    QString result;
    result.reserve(input.length());
    int i = 0;
    const int length = input.length();
    while (i < length) {
        QChar c = input.at(i++);
        if ((c == '%') && (i + 1 < length)) {
            QChar a = input.at(i);
            if ((a == 'u') && (i + 4 < length)) {
                int d3 = fromHex(input.at(i+1));
                int d2 = fromHex(input.at(i+2));
                int d1 = fromHex(input.at(i+3));
                int d0 = fromHex(input.at(i+4));
                if ((d3 != -1) && (d2 != -1) && (d1 != -1) && (d0 != -1)) {
                    ushort uc = ushort((d3 << 12) | (d2 << 8) | (d1 << 4) | d0);
                    result.append(QChar(uc));
                    i += 5;
                } else {
                    result.append(c);
                }
            } else {
                int d1 = fromHex(a);
                int d0 = fromHex(input.at(i+1));
                if ((d1 != -1) && (d0 != -1)) {
                    c = (d1 << 4) | d0;
                    i += 2;
                }
                result.append(c);
            }
        } else {
            result.append(c);
        }
    }
    return result;
}

static const char uriReserved[] = ";/?:@&=+$,#";
static const char uriUnescaped[] = "-_.!~*'()";
static const char uriUnescapedReserved[] = "-_.!~*'();/?:@&=+$,#";

static void addEscapeSequence(QString &output, uchar ch)
{
    output.append(QLatin1Char('%'));
    output.append(QLatin1Char(toHex(ch >> 4)));
    output.append(QLatin1Char(toHex(ch & 0xf)));
}

static QString encode(const QString &input, const char *unescapedSet, bool *ok)
{
    *ok = true;
    QString output;
    const int length = input.length();
    int i = 0;
    while (i < length) {
        const QChar c = input.at(i);
        bool escape = true;
        if ((c.unicode() >= 'a' && c.unicode() <= 'z') ||
            (c.unicode() >= 'A' && c.unicode() <= 'Z') ||
            (c.unicode() >= '0' && c.unicode() <= '9')) {
            escape = false;
        } else {
            const char *r = unescapedSet;
            while (*r) {
                if (*r == c.unicode()) {
                    escape = false;
                    break;
                }
                ++r;
            }
        }
        if (escape) {
            uint uc = c.unicode();
            if ((uc >= 0xDC00) && (uc <= 0xDFFF)) {
                *ok = false;
                break;
            }
            if (!((uc < 0xD800) || (uc > 0xDBFF))) {
                ++i;
                if (i == length) {
                    *ok = false;
                    break;
                }
                const uint uc2 = input.at(i).unicode();
                if ((uc2 < 0xDC00) || (uc2 > 0xDFFF)) {
                    *ok = false;
                    break;
                }
                uc = ((uc - 0xD800) * 0x400) + (uc2 - 0xDC00) + 0x10000;
            }
            if (uc < 0x80) {
                addEscapeSequence(output, (uchar)uc);
            } else {
                if (uc < 0x0800) {
                    addEscapeSequence(output, 0xc0 | ((uchar) (uc >> 6)));
                } else {

                    if (QChar::requiresSurrogates(uc)) {
                        addEscapeSequence(output, 0xf0 | ((uchar) (uc >> 18)));
                        addEscapeSequence(output, 0x80 | (((uchar) (uc >> 12)) & 0x3f));
                    } else {
                        addEscapeSequence(output, 0xe0 | (((uchar) (uc >> 12)) & 0x3f));
                    }
                    addEscapeSequence(output, 0x80 | (((uchar) (uc >> 6)) & 0x3f));
                }
                addEscapeSequence(output, 0x80 | ((uchar) (uc&0x3f)));
            }
        } else {
            output.append(c);
        }
        ++i;
    }
    if (i != length)
        *ok = false;
    return output;
}

enum DecodeMode {
    DecodeAll,
    DecodeNonReserved
};

static QString decode(const QString &input, DecodeMode decodeMode, bool *ok)
{
    *ok = true;
    QString output;
    output.reserve(input.length());
    const int length = input.length();
    int i = 0;
    const QChar percent = QLatin1Char('%');
    while (i < length) {
        const QChar ch = input.at(i);
        if (ch == percent) {
            int start = i;
            if (i + 2 >= length)
                goto error;

            int d1 = fromHex(input.at(i+1));
            int d0 = fromHex(input.at(i+2));
            if ((d1 == -1) || (d0 == -1))
                goto error;

            int b = (d1 << 4) | d0;
            i += 2;
            if (b & 0x80) {
                int uc;
                int min_uc;
                int need;
                if ((b & 0xe0) == 0xc0) {
                    uc = b & 0x1f;
                    need = 1;
                    min_uc = 0x80;
                } else if ((b & 0xf0) == 0xe0) {
                    uc = b & 0x0f;
                    need = 2;
                    min_uc = 0x800;
                } else if ((b & 0xf8) == 0xf0) {
                    uc = b & 0x07;
                    need = 3;
                    min_uc = 0x10000;
                } else {
                    goto error;
                }

                if (i + (3 * need) >= length)
                    goto error;

                for (int j = 0; j < need; ++j) {
                    ++i;
                    if (input.at(i) != percent)
                        goto error;

                    d1 = fromHex(input.at(i+1));
                    d0 = fromHex(input.at(i+2));
                    if ((d1 == -1) || (d0 == -1))
                        goto error;

                    b = (d1 << 4) | d0;
                    if ((b & 0xC0) != 0x80)
                        goto error;

                    i += 2;
                    uc = (uc << 6) + (b & 0x3f);
                }
                if (uc < min_uc)
                    goto error;

                if (uc < 0x10000) {
                    output.append(QChar(uc));
                } else {
                    if (uc > 0x10FFFF)
                        goto error;

                    ushort l = ushort(((uc - 0x10000) & 0x3FF) + 0xDC00);
                    ushort h = ushort((((uc - 0x10000) >> 10) & 0x3FF) + 0xD800);
                    output.append(QChar(h));
                    output.append(QChar(l));
                }
            } else {
                if (decodeMode == DecodeNonReserved && b <= 0x40) {
                    const char *r = uriReserved;
                    while (*r) {
                        if (*r == b)
                            break;
                        ++r;
                    }
                    if (*r)
                        output.append(input.mid(start, i - start + 1));
                    else
                        output.append(QChar(b));
                } else {
                    output.append(QChar(b));
                }
            }
        } else {
            output.append(ch);
        }
        ++i;
    }
    if (i != length)
        *ok = false;
    return output;
  error:
    *ok = false;
    return QString();
}

DEFINE_OBJECT_VTABLE(EvalFunction);

EvalFunction::Data::Data(ExecutionContext *scope)
    : FunctionObject::Data(scope, scope->d()->engine->id_eval)
{
    setVTable(staticVTable());
    Scope s(scope);
    ScopedFunctionObject f(s, this);
    f->defineReadonlyProperty(s.engine->id_length, Primitive::fromInt32(1));
}

ReturnedValue EvalFunction::evalCall(CallData *callData, bool directCall)
{
    if (callData->argc < 1)
        return Encode::undefined();

    ExecutionEngine *v4 = engine();
    ExecutionContext *parentContext = v4->currentContext();
    ExecutionContextSaver ctxSaver(parentContext);

    ExecutionContext *ctx = parentContext;
    Scope scope(ctx);

    if (!directCall) {
        // the context for eval should be the global scope, so we fake a root
        // context
        ctx = v4->pushGlobalContext();
    }

    if (!callData->args[0].isString())
        return callData->args[0].asReturnedValue();

    const QString code = callData->args[0].stringValue()->toQString();
    bool inheritContext = !ctx->d()->strictMode;

    Script script(ctx, code, QStringLiteral("eval code"));
    script.strictMode = (directCall && parentContext->d()->strictMode);
    script.inheritContext = inheritContext;
    script.parse();
    if (scope.engine->hasException)
        return Encode::undefined();

    Function *function = script.function();
    if (!function)
        return Encode::undefined();

    d()->strictMode = function->isStrict() || (ctx->d()->strictMode);

    d()->needsActivation = function->needsActivation();

    if (strictMode()) {
        ScopedFunctionObject e(scope, FunctionObject::createScriptFunction(ctx, function));
        ScopedCallData callData(scope, 0);
        callData->thisObject = ctx->d()->callData->thisObject;
        return e->call(callData);
    }

    ContextStateSaver stateSaver(ctx);

    // set the correct strict mode flag on the context
    ctx->d()->strictMode = strictMode();
    ctx->d()->compilationUnit = function->compilationUnit;

    return function->code(ctx, function->codeData);
}


ReturnedValue EvalFunction::call(Managed *that, CallData *callData)
{
    // indirect call
    return static_cast<EvalFunction *>(that)->evalCall(callData, false);
}


static inline int toInt(const QChar &qc, int R)
{
    ushort c = qc.unicode();
    int v = -1;
    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (c >= 'A' && c <= 'Z')
        v = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        v = c - 'a' + 10;
    if (v >= 0 && v < R)
        return v;
    else
        return -1;
}

// parseInt [15.1.2.2]
ReturnedValue GlobalFunctions::method_parseInt(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue string(scope, ctx->argument(0));
    ScopedValue radix(scope, ctx->argument(1));
    int R = radix->isUndefined() ? 0 : radix->toInt32();

    // [15.1.2.2] step by step:
    String *inputString = string->toString(ctx); // 1
    QString trimmed = inputString->toQString().trimmed(); // 2

    if (ctx->d()->engine->hasException)
        return Encode::undefined();

    const QChar *pos = trimmed.constData();
    const QChar *end = pos + trimmed.length();

    int sign = 1; // 3
    if (pos != end) {
        if (*pos == QLatin1Char('-'))
            sign = -1; // 4
        if (*pos == QLatin1Char('-') || *pos == QLatin1Char('+'))
            ++pos; // 5
    }
    bool stripPrefix = true; // 7
    if (R) { // 8
        if (R < 2 || R > 36)
            return Encode(std::numeric_limits<double>::quiet_NaN()); // 8a
        if (R != 16)
            stripPrefix = false; // 8b
    } else { // 9
        R = 10; // 9a
    }
    if (stripPrefix) { // 10
        if ((end - pos >= 2)
                && (pos[0] == QLatin1Char('0'))
                && (pos[1] == QLatin1Char('x') || pos[1] == QLatin1Char('X'))) { // 10a
            pos += 2;
            R = 16;
        }
    }
    // 11: Z is progressively built below
    // 13: this is handled by the toInt function
    if (pos == end) // 12
        return Encode(std::numeric_limits<double>::quiet_NaN());
    bool overflow = false;
    qint64 v_overflow = 0;
    unsigned overflow_digit_count = 0;
    int d = toInt(*pos++, R);
    if (d == -1)
        return Encode(std::numeric_limits<double>::quiet_NaN());
    qint64 v = d;
    while (pos != end) {
        d = toInt(*pos++, R);
        if (d == -1)
            break;
        if (overflow) {
            if (overflow_digit_count == 0) {
                v_overflow = v;
                v = 0;
            }
            ++overflow_digit_count;
            v = v * R + d;
        } else {
            qint64 vNew = v * R + d;
            if (vNew < v) {
                overflow = true;
                --pos;
            } else {
                v = vNew;
            }
        }
    }

    if (overflow) {
        double result = (double) v_overflow * pow(R, overflow_digit_count);
        result += v;
        return Encode(sign * result);
    } else {
        return Encode(sign * (double) v); // 15
    }
}

// parseFloat [15.1.2.3]
ReturnedValue GlobalFunctions::method_parseFloat(CallContext *ctx)
{
    Scope scope(ctx);

    // [15.1.2.3] step by step:
    Scoped<String> inputString(scope, ctx->argument(0), Scoped<String>::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();

    QString trimmed = inputString->toQString().trimmed(); // 2

    // 4:
    if (trimmed.startsWith(QLatin1String("Infinity"))
            || trimmed.startsWith(QLatin1String("+Infinity")))
        return Encode(Q_INFINITY);
    if (trimmed.startsWith(QStringLiteral("-Infinity")))
        return Encode(-Q_INFINITY);
    QByteArray ba = trimmed.toLatin1();
    bool ok;
    const char *begin = ba.constData();
    const char *end = 0;
    double d = qstrtod(begin, &end, &ok);
    if (end - begin == 0)
        return Encode(std::numeric_limits<double>::quiet_NaN()); // 3
    else
        return Encode(d);
}

/// isNaN [15.1.2.4]
ReturnedValue GlobalFunctions::method_isNaN(CallContext *ctx)
{
    if (!ctx->d()->callData->argc)
        // undefined gets converted to NaN
        return Encode(true);

    if (ctx->d()->callData->args[0].integerCompatible())
        return Encode(false);

    double d = ctx->d()->callData->args[0].toNumber();
    return Encode((bool)std::isnan(d));
}

/// isFinite [15.1.2.5]
ReturnedValue GlobalFunctions::method_isFinite(CallContext *ctx)
{
    if (!ctx->d()->callData->argc)
        // undefined gets converted to NaN
        return Encode(false);

    if (ctx->d()->callData->args[0].integerCompatible())
        return Encode(true);

    double d = ctx->d()->callData->args[0].toNumber();
    return Encode((bool)std::isfinite(d));
}

/// decodeURI [15.1.3.1]
ReturnedValue GlobalFunctions::method_decodeURI(CallContext *context)
{
    if (context->d()->callData->argc == 0)
        return Encode::undefined();

    QString uriString = context->d()->callData->args[0].toString(context)->toQString();
    bool ok;
    QString out = decode(uriString, DecodeNonReserved, &ok);
    if (!ok) {
        Scope scope(context);
        ScopedString s(scope, context->d()->engine->newString(QStringLiteral("malformed URI sequence")));
        return context->throwURIError(s);
    }

    return context->d()->engine->newString(out)->asReturnedValue();
}

/// decodeURIComponent [15.1.3.2]
ReturnedValue GlobalFunctions::method_decodeURIComponent(CallContext *context)
{
    if (context->d()->callData->argc == 0)
        return Encode::undefined();

    QString uriString = context->d()->callData->args[0].toString(context)->toQString();
    bool ok;
    QString out = decode(uriString, DecodeAll, &ok);
    if (!ok) {
        Scope scope(context);
        ScopedString s(scope, context->d()->engine->newString(QStringLiteral("malformed URI sequence")));
        return context->throwURIError(s);
    }

    return context->d()->engine->newString(out)->asReturnedValue();
}

/// encodeURI [15.1.3.3]
ReturnedValue GlobalFunctions::method_encodeURI(CallContext *context)
{
    if (context->d()->callData->argc == 0)
        return Encode::undefined();

    QString uriString = context->d()->callData->args[0].toString(context)->toQString();
    bool ok;
    QString out = encode(uriString, uriUnescapedReserved, &ok);
    if (!ok) {
        Scope scope(context);
        ScopedString s(scope, context->d()->engine->newString(QStringLiteral("malformed URI sequence")));
        return context->throwURIError(s);
    }

    return context->d()->engine->newString(out)->asReturnedValue();
}

/// encodeURIComponent [15.1.3.4]
ReturnedValue GlobalFunctions::method_encodeURIComponent(CallContext *context)
{
    if (context->d()->callData->argc == 0)
        return Encode::undefined();

    QString uriString = context->d()->callData->args[0].toString(context)->toQString();
    bool ok;
    QString out = encode(uriString, uriUnescaped, &ok);
    if (!ok) {
        Scope scope(context);
        ScopedString s(scope, context->d()->engine->newString(QStringLiteral("malformed URI sequence")));
        return context->throwURIError(s);
    }

    return context->d()->engine->newString(out)->asReturnedValue();
}

ReturnedValue GlobalFunctions::method_escape(CallContext *context)
{
    if (!context->d()->callData->argc)
        return context->d()->engine->newString(QStringLiteral("undefined"))->asReturnedValue();

    QString str = context->d()->callData->args[0].toString(context)->toQString();
    return context->d()->engine->newString(escape(str))->asReturnedValue();
}

ReturnedValue GlobalFunctions::method_unescape(CallContext *context)
{
    if (!context->d()->callData->argc)
        return context->d()->engine->newString(QStringLiteral("undefined"))->asReturnedValue();

    QString str = context->d()->callData->args[0].toString(context)->toQString();
    return context->d()->engine->newString(unescape(str))->asReturnedValue();
}
