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

#include "qv4regexpobject_p.h"
#include "qv4jsir_p.h"
#include "qv4isel_p.h"
#include "qv4objectproto_p.h"
#include "qv4regexp_p.h"
#include "qv4stringobject_p.h"
#include <private/qv4mm_p.h>
#include "qv4scopedvalue_p.h"

#include <private/qqmljsengine_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljsast_p.h>
#include <qv4jsir_p.h>
#include <qv4codegen_p.h>
#include "private/qlocale_tools_p.h"

#include <QtCore/QDebug>
#include <QtCore/qregexp.h>
#include <cassert>
#include <typeinfo>
#include <iostream>
#include "qv4alloca_p.h"

QT_BEGIN_NAMESPACE

Q_CORE_EXPORT QString qt_regexp_toCanonical(const QString &, QRegExp::PatternSyntax);

using namespace QV4;

DEFINE_OBJECT_VTABLE(RegExpObject);

void Heap::RegExpObject::init()
{
    Object::init();
    Scope scope(internalClass->engine);
    Scoped<QV4::RegExpObject> o(scope, this);
    o->d()->value = QV4::RegExp::create(scope.engine, QString(), false, false);
    o->d()->global = false;
    o->initProperties();
}

void Heap::RegExpObject::init(QV4::RegExp *value, bool global)
{
    Object::init();
    this->global = global;
    this->value = value->d();
    Scope scope(internalClass->engine);
    Scoped<QV4::RegExpObject> o(scope, this);
    o->initProperties();
}

// Converts a QRegExp to a JS RegExp.
// The conversion is not 100% exact since ECMA regexp and QRegExp
// have different semantics/flags, but we try to do our best.
void Heap::RegExpObject::init(const QRegExp &re)
{
    Object::init();
    global = false;

    // Convert the pattern to a ECMAScript pattern.
    QString pattern = QT_PREPEND_NAMESPACE(qt_regexp_toCanonical)(re.pattern(), re.patternSyntax());
    if (re.isMinimal()) {
        QString ecmaPattern;
        int len = pattern.length();
        ecmaPattern.reserve(len);
        int i = 0;
        const QChar *wc = pattern.unicode();
        bool inBracket = false;
        while (i < len) {
            QChar c = wc[i++];
            ecmaPattern += c;
            switch (c.unicode()) {
            case '?':
            case '+':
            case '*':
            case '}':
                if (!inBracket)
                    ecmaPattern += QLatin1Char('?');
                break;
            case '\\':
                if (i < len)
                    ecmaPattern += wc[i++];
                break;
            case '[':
                inBracket = true;
                break;
            case ']':
                inBracket = false;
                break;
            default:
                break;
            }
        }
        pattern = ecmaPattern;
    }

    Scope scope(internalClass->engine);
    Scoped<QV4::RegExpObject> o(scope, this);

    o->d()->value = QV4::RegExp::create(scope.engine, pattern, re.caseSensitivity() == Qt::CaseInsensitive, false);

    o->initProperties();
}

void RegExpObject::initProperties()
{
    *propertyData(Index_LastIndex) = Primitive::fromInt32(0);

    Q_ASSERT(value());

    QString p = *value()->pattern;
    if (p.isEmpty()) {
        p = QStringLiteral("(?:)");
    } else {
        // escape certain parts, see ch. 15.10.4
        p.replace('/', QLatin1String("\\/"));
    }

    *propertyData(Index_Source) = engine()->newString(p);
    *propertyData(Index_Global) = Primitive::fromBoolean(global());
    *propertyData(Index_IgnoreCase) = Primitive::fromBoolean(value()->ignoreCase);
    *propertyData(Index_Multiline) = Primitive::fromBoolean(value()->multiLine);
}


void RegExpObject::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    RegExpObject::Data *re = static_cast<RegExpObject::Data *>(that);
    if (re->value)
        re->value->mark(e);
    Object::markObjects(that, e);
}

Value *RegExpObject::lastIndexProperty()
{
    Q_ASSERT(0 == internalClass()->find(engine()->id_lastIndex()));
    return propertyData(0);
}

// Converts a JS RegExp to a QRegExp.
// The conversion is not 100% exact since ECMA regexp and QRegExp
// have different semantics/flags, but we try to do our best.
QRegExp RegExpObject::toQRegExp() const
{
    Qt::CaseSensitivity caseSensitivity = value()->ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive;
    return QRegExp(*value()->pattern, caseSensitivity, QRegExp::RegExp2);
}

QString RegExpObject::toString() const
{
    QString result = QLatin1Char('/') + source() + QLatin1Char('/');
    if (global())
        result += QLatin1Char('g');
    if (value()->ignoreCase)
        result += QLatin1Char('i');
    if (value()->multiLine)
        result += QLatin1Char('m');
    return result;
}

QString RegExpObject::source() const
{
    Scope scope(engine());
    ScopedString source(scope, scope.engine->newIdentifier(QStringLiteral("source")));
    ScopedValue s(scope, const_cast<RegExpObject *>(this)->get(source));
    return s->toQString();
}

uint RegExpObject::flags() const
{
    uint f = 0;
    if (global())
        f |= QV4::RegExpObject::RegExp_Global;
    if (value()->ignoreCase)
        f |= QV4::RegExpObject::RegExp_IgnoreCase;
    if (value()->multiLine)
        f |= QV4::RegExpObject::RegExp_Multiline;
    return f;
}

DEFINE_OBJECT_VTABLE(RegExpCtor);

void Heap::RegExpCtor::init(QV4::ExecutionContext *scope)
{
    Heap::FunctionObject::init(scope, QStringLiteral("RegExp"));
    clearLastMatch();
}

void Heap::RegExpCtor::clearLastMatch()
{
    lastMatch = Primitive::nullValue();
    lastInput = internalClass->engine->id_empty()->d();
    lastMatchStart = 0;
    lastMatchEnd = 0;
}

void RegExpCtor::construct(const Managed *, Scope &scope, CallData *callData)
{
    ScopedValue r(scope, callData->argument(0));
    ScopedValue f(scope, callData->argument(1));
    Scoped<RegExpObject> re(scope, r);
    if (re) {
        if (!f->isUndefined()) {
            scope.result = scope.engine->throwTypeError();
            return;
        }

        Scoped<RegExp> regexp(scope, re->value());
        scope.result = Encode(scope.engine->newRegExpObject(regexp, re->global()));
        return;
    }

    QString pattern;
    if (!r->isUndefined())
        pattern = r->toQString();
    if (scope.hasException()) {
        scope.result = Encode::undefined();
        return;
    }

    bool global = false;
    bool ignoreCase = false;
    bool multiLine = false;
    if (!f->isUndefined()) {
        f = RuntimeHelpers::toString(scope.engine, f);
        if (scope.hasException()) {
            scope.result = Encode::undefined();
            return;
        }
        QString str = f->stringValue()->toQString();
        for (int i = 0; i < str.length(); ++i) {
            if (str.at(i) == QLatin1Char('g') && !global) {
                global = true;
            } else if (str.at(i) == QLatin1Char('i') && !ignoreCase) {
                ignoreCase = true;
            } else if (str.at(i) == QLatin1Char('m') && !multiLine) {
                multiLine = true;
            } else {
                scope.result = scope.engine->throwSyntaxError(QStringLiteral("Invalid flags supplied to RegExp constructor"));
                return;
            }
        }
    }

    Scoped<RegExp> regexp(scope, RegExp::create(scope.engine, pattern, ignoreCase, multiLine));
    if (!regexp->isValid()) {
        scope.result = scope.engine->throwSyntaxError(QStringLiteral("Invalid regular expression"));
        return;
    }

    scope.result = Encode(scope.engine->newRegExpObject(regexp, global));
}

void RegExpCtor::call(const Managed *that, Scope &scope, CallData *callData)
{
    if (callData->argc > 0 && callData->args[0].as<RegExpObject>()) {
        if (callData->argc == 1 || callData->args[1].isUndefined()) {
            scope.result = callData->args[0];
            return;
        }
    }

    construct(that, scope, callData);
}

void RegExpCtor::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    RegExpCtor::Data *This = static_cast<RegExpCtor::Data *>(that);
    This->lastMatch.mark(e);
    This->lastInput->mark(e);
    FunctionObject::markObjects(that, e);
}

void RegExpPrototype::init(ExecutionEngine *engine, Object *constructor)
{
    Scope scope(engine);
    ScopedObject o(scope);
    ScopedObject ctor(scope, constructor);

    ctor->defineReadonlyProperty(engine->id_prototype(), (o = this));
    ctor->defineReadonlyProperty(engine->id_length(), Primitive::fromInt32(2));

    // Properties deprecated in the spec but required by "the web" :(
    ctor->defineAccessorProperty(QStringLiteral("lastMatch"), method_get_lastMatch_n<0>, 0);
    ctor->defineAccessorProperty(QStringLiteral("$&"), method_get_lastMatch_n<0>, 0);
    ctor->defineAccessorProperty(QStringLiteral("$1"), method_get_lastMatch_n<1>, 0);
    ctor->defineAccessorProperty(QStringLiteral("$2"), method_get_lastMatch_n<2>, 0);
    ctor->defineAccessorProperty(QStringLiteral("$3"), method_get_lastMatch_n<3>, 0);
    ctor->defineAccessorProperty(QStringLiteral("$4"), method_get_lastMatch_n<4>, 0);
    ctor->defineAccessorProperty(QStringLiteral("$5"), method_get_lastMatch_n<5>, 0);
    ctor->defineAccessorProperty(QStringLiteral("$6"), method_get_lastMatch_n<6>, 0);
    ctor->defineAccessorProperty(QStringLiteral("$7"), method_get_lastMatch_n<7>, 0);
    ctor->defineAccessorProperty(QStringLiteral("$8"), method_get_lastMatch_n<8>, 0);
    ctor->defineAccessorProperty(QStringLiteral("$9"), method_get_lastMatch_n<9>, 0);
    ctor->defineAccessorProperty(QStringLiteral("lastParen"), method_get_lastParen, 0);
    ctor->defineAccessorProperty(QStringLiteral("$+"), method_get_lastParen, 0);
    ctor->defineAccessorProperty(QStringLiteral("input"), method_get_input, 0);
    ctor->defineAccessorProperty(QStringLiteral("$_"), method_get_input, 0);
    ctor->defineAccessorProperty(QStringLiteral("leftContext"), method_get_leftContext, 0);
    ctor->defineAccessorProperty(QStringLiteral("$`"), method_get_leftContext, 0);
    ctor->defineAccessorProperty(QStringLiteral("rightContext"), method_get_rightContext, 0);
    ctor->defineAccessorProperty(QStringLiteral("$'"), method_get_rightContext, 0);

    defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    defineDefaultProperty(QStringLiteral("exec"), method_exec, 1);
    defineDefaultProperty(QStringLiteral("test"), method_test, 1);
    defineDefaultProperty(engine->id_toString(), method_toString, 0);
    defineDefaultProperty(QStringLiteral("compile"), method_compile, 2);
}

ReturnedValue RegExpPrototype::method_exec(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<RegExpObject> r(scope, ctx->thisObject().as<RegExpObject>());
    if (!r)
        return ctx->engine()->throwTypeError();

    ScopedValue arg(scope, ctx->argument(0));
    arg = RuntimeHelpers::toString(scope.engine, arg);
    if (scope.hasException())
        return Encode::undefined();
    QString s = arg->stringValue()->toQString();

    int offset = r->global() ? r->lastIndexProperty()->toInt32() : 0;
    if (offset < 0 || offset > s.length()) {
        *r->lastIndexProperty() = Primitive::fromInt32(0);
        return Encode::null();
    }

    uint* matchOffsets = (uint*)alloca(r->value()->captureCount() * 2 * sizeof(uint));
    const int result = Scoped<RegExp>(scope, r->value())->match(s, offset, matchOffsets);

    Scoped<RegExpCtor> regExpCtor(scope, ctx->d()->engine->regExpCtor());
    regExpCtor->d()->clearLastMatch();

    if (result == -1) {
        *r->lastIndexProperty() = Primitive::fromInt32(0);
        return Encode::null();
    }

    // fill in result data
    ScopedArrayObject array(scope, scope.engine->newArrayObject(scope.engine->regExpExecArrayClass, scope.engine->arrayPrototype()));
    int len = r->value()->captureCount();
    array->arrayReserve(len);
    ScopedValue v(scope);
    for (int i = 0; i < len; ++i) {
        int start = matchOffsets[i * 2];
        int end = matchOffsets[i * 2 + 1];
        v = (start != -1) ? ctx->d()->engine->newString(s.mid(start, end - start))->asReturnedValue() : Encode::undefined();
        array->arrayPut(i, v);
    }
    array->setArrayLengthUnchecked(len);
    *array->propertyData(Index_ArrayIndex) = Primitive::fromInt32(result);
    *array->propertyData(Index_ArrayInput) = arg;

    RegExpCtor::Data *dd = regExpCtor->d();
    dd->lastMatch = array;
    dd->lastInput = arg->stringValue()->d();
    dd->lastMatchStart = matchOffsets[0];
    dd->lastMatchEnd = matchOffsets[1];

    if (r->global())
        *r->lastIndexProperty() = Primitive::fromInt32(matchOffsets[1]);

    return array.asReturnedValue();
}

ReturnedValue RegExpPrototype::method_test(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue r(scope, method_exec(ctx));
    return Encode(!r->isNull());
}

ReturnedValue RegExpPrototype::method_toString(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<RegExpObject> r(scope, ctx->thisObject().as<RegExpObject>());
    if (!r)
        return ctx->engine()->throwTypeError();

    return ctx->d()->engine->newString(r->toString())->asReturnedValue();
}

ReturnedValue RegExpPrototype::method_compile(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<RegExpObject> r(scope, ctx->thisObject().as<RegExpObject>());
    if (!r)
        return ctx->engine()->throwTypeError();

    ScopedCallData callData(scope, ctx->argc());
    memcpy(callData->args, ctx->args(), ctx->argc()*sizeof(Value));

    ctx->d()->engine->regExpCtor()->as<FunctionObject>()->construct(scope, callData);
    Scoped<RegExpObject> re(scope, scope.result.asReturnedValue());

    r->d()->value = re->value();
    r->d()->global = re->global();
    return Encode::undefined();
}

template <int index>
ReturnedValue RegExpPrototype::method_get_lastMatch_n(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedArrayObject lastMatch(scope, static_cast<RegExpCtor*>(ctx->d()->engine->regExpCtor())->lastMatch());
    ScopedValue result(scope, lastMatch ? lastMatch->getIndexed(index) : Encode::undefined());
    if (result->isUndefined())
        return ctx->d()->engine->newString()->asReturnedValue();
    return result->asReturnedValue();
}

ReturnedValue RegExpPrototype::method_get_lastParen(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedArrayObject lastMatch(scope, static_cast<RegExpCtor*>(ctx->d()->engine->regExpCtor())->lastMatch());
    ScopedValue result(scope, lastMatch ? lastMatch->getIndexed(lastMatch->getLength() - 1) : Encode::undefined());
    if (result->isUndefined())
        return ctx->d()->engine->newString()->asReturnedValue();
    return result->asReturnedValue();
}

ReturnedValue RegExpPrototype::method_get_input(CallContext *ctx)
{
    return static_cast<RegExpCtor*>(ctx->d()->engine->regExpCtor())->lastInput()->asReturnedValue();
}

ReturnedValue RegExpPrototype::method_get_leftContext(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<RegExpCtor> regExpCtor(scope, ctx->d()->engine->regExpCtor());
    QString lastInput = regExpCtor->lastInput()->toQString();
    return ctx->d()->engine->newString(lastInput.left(regExpCtor->lastMatchStart()))->asReturnedValue();
}

ReturnedValue RegExpPrototype::method_get_rightContext(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<RegExpCtor> regExpCtor(scope, ctx->d()->engine->regExpCtor());
    QString lastInput = regExpCtor->lastInput()->toQString();
    return ctx->d()->engine->newString(lastInput.mid(regExpCtor->lastMatchEnd()))->asReturnedValue();
}

QT_END_NAMESPACE
