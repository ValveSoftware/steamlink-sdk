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


#include "qv4stringobject_p.h"
#include "qv4regexpobject_p.h"
#include "qv4objectproto_p.h"
#include "qv4mm_p.h"
#include "qv4scopedvalue_p.h"
#include <QtCore/qnumeric.h>
#include <QtCore/qmath.h>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QDebug>
#include <cmath>
#include <qmath.h>
#include <qnumeric.h>
#include <cassert>

#include <private/qqmljsengine_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljsast_p.h>
#include <qv4jsir_p.h>
#include <qv4codegen_p.h>

#ifndef Q_OS_WIN
#  include <time.h>
#  ifndef Q_OS_VXWORKS
#    include <sys/time.h>
#  else
#    include "qplatformdefs.h"
#  endif
#else
#  include <windows.h>
#endif

using namespace QV4;

DEFINE_OBJECT_VTABLE(StringObject);

StringObject::Data::Data(InternalClass *ic)
    : Object::Data(ic)
{
    Q_ASSERT(internalClass->vtable == staticVTable());
    value = ic->engine->newString(QStringLiteral(""))->asReturnedValue();
    tmpProperty.value = Primitive::undefinedValue();

    Scope scope(ic->engine);
    ScopedObject s(scope, this);
    s->defineReadonlyProperty(ic->engine->id_length, Primitive::fromInt32(0));
}

StringObject::Data::Data(ExecutionEngine *engine, const ValueRef val)
    : Object::Data(engine->stringObjectClass)
{
    value = val;
    Q_ASSERT(value.isString());
    tmpProperty.value = Primitive::undefinedValue();
    setVTable(staticVTable());

    Scope scope(engine);
    ScopedObject s(scope, this);
    s->defineReadonlyProperty(engine->id_length, Primitive::fromUInt32(value.stringValue()->toQString().length()));
}

Property *StringObject::getIndex(uint index) const
{
    QString str = d()->value.stringValue()->toQString();
    if (index >= (uint)str.length())
        return 0;
    d()->tmpProperty.value = Encode(internalClass()->engine->newString(str.mid(index, 1)));
    return &d()->tmpProperty;
}

bool StringObject::deleteIndexedProperty(Managed *m, uint index)
{
    ExecutionEngine *v4 = m->engine();
    Scope scope(v4);
    Scoped<StringObject> o(scope, m->asStringObject());
    if (!o) {
        v4->currentContext()->throwTypeError();
        return false;
    }

    if (index < static_cast<uint>(o->d()->value.stringValue()->toQString().length())) {
        if (v4->currentContext()->d()->strictMode)
            v4->currentContext()->throwTypeError();
        return false;
    }
    return true;
}

void StringObject::advanceIterator(Managed *m, ObjectIterator *it, String *&name, uint *index, Property *p, PropertyAttributes *attrs)
{
    name = (String *)0;
    StringObject *s = static_cast<StringObject *>(m);
    uint slen = s->d()->value.stringValue()->toQString().length();
    if (it->arrayIndex <= slen) {
        while (it->arrayIndex < slen) {
            *index = it->arrayIndex;
            ++it->arrayIndex;
            PropertyAttributes a;
            Property *pd = s->__getOwnProperty__(*index, &a);
            if (!(it->flags & ObjectIterator::EnumerableOnly) || a.isEnumerable()) {
                *attrs = a;
                p->copy(*pd, a);
                return;
            }
        }
        if (s->arrayData()) {
            it->arrayNode = s->sparseBegin();
            // iterate until we're past the end of the string
            while (it->arrayNode && it->arrayNode->key() < slen)
                it->arrayNode = it->arrayNode->nextNode();
        }
    }

    return Object::advanceIterator(m, it, name, index, p, attrs);
}

void StringObject::markObjects(Managed *that, ExecutionEngine *e)
{
    StringObject *o = static_cast<StringObject *>(that);
    o->d()->value.stringValue()->mark(e);
    o->d()->tmpProperty.value.mark(e);
    Object::markObjects(that, e);
}

DEFINE_OBJECT_VTABLE(StringCtor);

StringCtor::Data::Data(ExecutionContext *scope)
    : FunctionObject::Data(scope, QStringLiteral("String"))
{
    setVTable(staticVTable());
}

ReturnedValue StringCtor::construct(Managed *m, CallData *callData)
{
    ExecutionEngine *v4 = m->engine();
    Scope scope(v4);
    ScopedValue value(scope);
    if (callData->argc)
        value = callData->args[0].toString(v4->currentContext());
    else
        value = v4->newString(QString());
    return Encode(v4->newStringObject(value));
}

ReturnedValue StringCtor::call(Managed *m, CallData *callData)
{
    ExecutionEngine *v4 = m->engine();
    Scope scope(v4);
    ScopedValue value(scope);
    if (callData->argc)
        value = callData->args[0].toString(v4->currentContext());
    else
        value = v4->newString(QString());
    return value.asReturnedValue();
}

void StringPrototype::init(ExecutionEngine *engine, Object *ctor)
{
    Scope scope(engine);
    ScopedObject o(scope);

    ctor->defineReadonlyProperty(engine->id_prototype, (o = this));
    ctor->defineReadonlyProperty(engine->id_length, Primitive::fromInt32(1));
    ctor->defineDefaultProperty(QStringLiteral("fromCharCode"), method_fromCharCode, 1);

    defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    defineDefaultProperty(engine->id_toString, method_toString);
    defineDefaultProperty(engine->id_valueOf, method_toString); // valueOf and toString are identical
    defineDefaultProperty(QStringLiteral("charAt"), method_charAt, 1);
    defineDefaultProperty(QStringLiteral("charCodeAt"), method_charCodeAt, 1);
    defineDefaultProperty(QStringLiteral("concat"), method_concat, 1);
    defineDefaultProperty(QStringLiteral("indexOf"), method_indexOf, 1);
    defineDefaultProperty(QStringLiteral("lastIndexOf"), method_lastIndexOf, 1);
    defineDefaultProperty(QStringLiteral("localeCompare"), method_localeCompare, 1);
    defineDefaultProperty(QStringLiteral("match"), method_match, 1);
    defineDefaultProperty(QStringLiteral("replace"), method_replace, 2);
    defineDefaultProperty(QStringLiteral("search"), method_search, 1);
    defineDefaultProperty(QStringLiteral("slice"), method_slice, 2);
    defineDefaultProperty(QStringLiteral("split"), method_split, 2);
    defineDefaultProperty(QStringLiteral("substr"), method_substr, 2);
    defineDefaultProperty(QStringLiteral("substring"), method_substring, 2);
    defineDefaultProperty(QStringLiteral("toLowerCase"), method_toLowerCase);
    defineDefaultProperty(QStringLiteral("toLocaleLowerCase"), method_toLocaleLowerCase);
    defineDefaultProperty(QStringLiteral("toUpperCase"), method_toUpperCase);
    defineDefaultProperty(QStringLiteral("toLocaleUpperCase"), method_toLocaleUpperCase);
    defineDefaultProperty(QStringLiteral("trim"), method_trim);
}

static QString getThisString(ExecutionContext *ctx)
{
    Scope scope(ctx);
    ScopedValue t(scope, ctx->d()->callData->thisObject);
    if (t->isString())
        return t->stringValue()->toQString();
    if (StringObject *thisString = t->asStringObject())
        return thisString->d()->value.stringValue()->toQString();
    if (t->isUndefined() || t->isNull()) {
        ctx->throwTypeError();
        return QString();
    }
    return t->toQString();
}

ReturnedValue StringPrototype::method_toString(CallContext *context)
{
    if (context->d()->callData->thisObject.isString())
        return context->d()->callData->thisObject.asReturnedValue();

    StringObject *o = context->d()->callData->thisObject.asStringObject();
    if (!o)
        return context->throwTypeError();
    return o->d()->value.asReturnedValue();
}

ReturnedValue StringPrototype::method_charAt(CallContext *context)
{
    const QString str = getThisString(context);
    if (context->d()->engine->hasException)
        return Encode::undefined();

    int pos = 0;
    if (context->d()->callData->argc > 0)
        pos = (int) context->d()->callData->args[0].toInteger();

    QString result;
    if (pos >= 0 && pos < str.length())
        result += str.at(pos);

    return context->d()->engine->newString(result)->asReturnedValue();
}

ReturnedValue StringPrototype::method_charCodeAt(CallContext *context)
{
    const QString str = getThisString(context);
    if (context->d()->engine->hasException)
        return Encode::undefined();

    int pos = 0;
    if (context->d()->callData->argc > 0)
        pos = (int) context->d()->callData->args[0].toInteger();


    if (pos >= 0 && pos < str.length())
        return Encode(str.at(pos).unicode());

    return Encode(qSNaN());
}

ReturnedValue StringPrototype::method_concat(CallContext *context)
{
    Scope scope(context);

    QString value = getThisString(context);
    if (scope.engine->hasException)
        return Encode::undefined();

    ScopedValue v(scope);
    for (int i = 0; i < context->d()->callData->argc; ++i) {
        v = RuntimeHelpers::toString(context, ValueRef(&context->d()->callData->args[i]));
        if (scope.hasException())
            return Encode::undefined();
        Q_ASSERT(v->isString());
        value += v->stringValue()->toQString();
    }

    return context->d()->engine->newString(value)->asReturnedValue();
}

ReturnedValue StringPrototype::method_indexOf(CallContext *context)
{
    QString value = getThisString(context);
    if (context->d()->engine->hasException)
        return Encode::undefined();

    QString searchString;
    if (context->d()->callData->argc)
        searchString = context->d()->callData->args[0].toString(context)->toQString();

    int pos = 0;
    if (context->d()->callData->argc > 1)
        pos = (int) context->d()->callData->args[1].toInteger();

    int index = -1;
    if (! value.isEmpty())
        index = value.indexOf(searchString, qMin(qMax(pos, 0), value.length()));

    return Encode(index);
}

ReturnedValue StringPrototype::method_lastIndexOf(CallContext *context)
{
    Scope scope(context);

    const QString value = getThisString(context);
    if (scope.engine->hasException)
        return Encode::undefined();

    QString searchString;
    if (context->d()->callData->argc)
        searchString = context->d()->callData->args[0].toQString();

    ScopedValue posArg(scope, context->argument(1));
    double position = RuntimeHelpers::toNumber(posArg);
    if (std::isnan(position))
        position = +qInf();
    else
        position = trunc(position);

    int pos = trunc(qMin(qMax(position, 0.0), double(value.length())));
    if (!searchString.isEmpty() && pos == value.length())
        --pos;
    if (searchString.isNull() && pos == 0)
        return Encode(-1);
    int index = value.lastIndexOf(searchString, pos);
    return Encode(index);
}

ReturnedValue StringPrototype::method_localeCompare(CallContext *context)
{
    Scope scope(context);
    const QString value = getThisString(context);
    if (scope.engine->hasException)
        return Encode::undefined();

    ScopedValue v(scope, context->d()->callData->argument(0));
    const QString that = v->toQString();
    return Encode(QString::localeAwareCompare(value, that));
}

ReturnedValue StringPrototype::method_match(CallContext *context)
{
    if (context->d()->callData->thisObject.isUndefined() || context->d()->callData->thisObject.isNull())
        return context->throwTypeError();

    Scope scope(context);
    ScopedString s(scope, context->d()->callData->thisObject.toString(context));

    ScopedValue regexp(scope, context->d()->callData->argument(0));
    Scoped<RegExpObject> rx(scope, regexp);
    if (!rx) {
        ScopedCallData callData(scope, 1);
        callData->args[0] = regexp;
        rx = context->d()->engine->regExpCtor.asFunctionObject()->construct(callData);
    }

    if (!rx)
        // ### CHECK
        return context->throwTypeError();

    bool global = rx->global();

    // ### use the standard builtin function, not the one that might be redefined in the proto
    ScopedString execString(scope, context->d()->engine->newString(QStringLiteral("exec")));
    Scoped<FunctionObject> exec(scope, context->d()->engine->regExpClass->prototype->get(execString.getPointer()));

    ScopedCallData callData(scope, 1);
    callData->thisObject = rx;
    callData->args[0] = s;
    if (!global)
        return exec->call(callData);

    ScopedString lastIndex(scope, context->d()->engine->newString(QStringLiteral("lastIndex")));
    rx->put(lastIndex.getPointer(), ScopedValue(scope, Primitive::fromInt32(0)));
    Scoped<ArrayObject> a(scope, context->d()->engine->newArrayObject());

    double previousLastIndex = 0;
    uint n = 0;
    ScopedValue result(scope);
    ScopedValue matchStr(scope);
    ScopedValue index(scope);
    while (1) {
        result = exec->call(callData);
        if (result->isNull())
            break;
        assert(result->isObject());
        index = rx->get(lastIndex.getPointer(), 0);
        double thisIndex = index->toInteger();
        if (previousLastIndex == thisIndex) {
            previousLastIndex = thisIndex + 1;
            rx->put(lastIndex.getPointer(), ScopedValue(scope, Primitive::fromDouble(previousLastIndex)));
        } else {
            previousLastIndex = thisIndex;
        }
        matchStr = result->objectValue()->getIndexed(0);
        a->arraySet(n, matchStr);
        ++n;
    }
    if (!n)
        return Encode::null();

    return a.asReturnedValue();

}

static void appendReplacementString(QString *result, const QString &input, const QString& replaceValue, uint* matchOffsets, int captureCount)
{
    result->reserve(result->length() + replaceValue.length());
    for (int i = 0; i < replaceValue.length(); ++i) {
        if (replaceValue.at(i) == QLatin1Char('$') && i < replaceValue.length() - 1) {
            ushort ch = replaceValue.at(++i).unicode();
            uint substStart = JSC::Yarr::offsetNoMatch;
            uint substEnd = JSC::Yarr::offsetNoMatch;
            if (ch == '$') {
                *result += QChar(ch);
                continue;
            } else if (ch == '&') {
                substStart = matchOffsets[0];
                substEnd = matchOffsets[1];
            } else if (ch == '`') {
                substStart = 0;
                substEnd = matchOffsets[0];
            } else if (ch == '\'') {
                substStart = matchOffsets[1];
                substEnd = input.length();
            } else if (ch >= '1' && ch <= '9') {
                uint capture = ch - '0';
                Q_ASSERT(capture > 0);
                if (capture < static_cast<uint>(captureCount)) {
                    substStart = matchOffsets[capture * 2];
                    substEnd = matchOffsets[capture * 2 + 1];
                }
            } else if (ch == '0' && i < replaceValue.length() - 1) {
                int capture = (ch - '0') * 10;
                ch = replaceValue.at(++i).unicode();
                if (ch >= '0' && ch <= '9') {
                    capture += ch - '0';
                    if (capture > 0 && capture < captureCount) {
                        substStart = matchOffsets[capture * 2];
                        substEnd = matchOffsets[capture * 2 + 1];
                    }
                }
            }
            if (substStart != JSC::Yarr::offsetNoMatch && substEnd != JSC::Yarr::offsetNoMatch)
                *result += input.midRef(substStart, substEnd - substStart);
        } else {
            *result += replaceValue.at(i);
        }
    }
}

ReturnedValue StringPrototype::method_replace(CallContext *ctx)
{
    Scope scope(ctx);
    QString string;
    if (StringObject *thisString = ctx->d()->callData->thisObject.asStringObject())
        string = thisString->d()->value.stringValue()->toQString();
    else
        string = ctx->d()->callData->thisObject.toString(ctx)->toQString();

    int numCaptures = 0;
    int numStringMatches = 0;

    uint allocatedMatchOffsets = 64;
    uint _matchOffsets[64];
    uint *matchOffsets = _matchOffsets;
    uint nMatchOffsets = 0;

    ScopedValue searchValue(scope, ctx->argument(0));
    Scoped<RegExpObject> regExp(scope, searchValue);
    if (regExp) {
        uint offset = 0;

        // We extract the pointer here to work around a compiler bug on Android.
        Scoped<RegExp> re(scope, regExp->value());
        while (true) {
            int oldSize = nMatchOffsets;
            if (allocatedMatchOffsets < nMatchOffsets + re->captureCount() * 2) {
                allocatedMatchOffsets = qMax(allocatedMatchOffsets * 2, nMatchOffsets + re->captureCount() * 2);
                uint *newOffsets = (uint *)malloc(allocatedMatchOffsets*sizeof(uint));
                memcpy(newOffsets, matchOffsets, nMatchOffsets*sizeof(uint));
                if (matchOffsets != _matchOffsets)
                    free(matchOffsets);
                matchOffsets = newOffsets;
            }
            if (re->match(string, offset, matchOffsets + oldSize) == JSC::Yarr::offsetNoMatch) {
                nMatchOffsets = oldSize;
                break;
            }
            nMatchOffsets += re->captureCount() * 2;
            if (!regExp->d()->global)
                break;
            offset = qMax(offset + 1, matchOffsets[oldSize + 1]);
        }
        if (regExp->global())
            regExp->lastIndexProperty(ctx)->value = Primitive::fromUInt32(0);
        numStringMatches = nMatchOffsets / (regExp->value()->captureCount() * 2);
        numCaptures = regExp->value()->captureCount();
    } else {
        numCaptures = 1;
        QString searchString = searchValue->toString(ctx)->toQString();
        int idx = string.indexOf(searchString);
        if (idx != -1) {
            numStringMatches = 1;
            nMatchOffsets = 2;
            matchOffsets[0] = idx;
            matchOffsets[1] = idx + searchString.length();
        }
    }

    QString result;
    ScopedValue replacement(scope);
    ScopedValue replaceValue(scope, ctx->argument(1));
    Scoped<FunctionObject> searchCallback(scope, replaceValue);
    if (!!searchCallback) {
        result.reserve(string.length() + 10*numStringMatches);
        ScopedCallData callData(scope, numCaptures + 2);
        callData->thisObject = Primitive::undefinedValue();
        int lastEnd = 0;
        ScopedValue entry(scope);
        for (int i = 0; i < numStringMatches; ++i) {
            for (int k = 0; k < numCaptures; ++k) {
                int idx = (i * numCaptures + k) * 2;
                uint start = matchOffsets[idx];
                uint end = matchOffsets[idx + 1];
                entry = Primitive::undefinedValue();
                if (start != JSC::Yarr::offsetNoMatch && end != JSC::Yarr::offsetNoMatch)
                    entry = ctx->d()->engine->newString(string.mid(start, end - start));
                callData->args[k] = entry;
            }
            uint matchStart = matchOffsets[i * numCaptures * 2];
            Q_ASSERT(matchStart >= static_cast<uint>(lastEnd));
            uint matchEnd = matchOffsets[i * numCaptures * 2 + 1];
            callData->args[numCaptures] = Primitive::fromUInt32(matchStart);
            callData->args[numCaptures + 1] = ctx->d()->engine->newString(string);

            replacement = searchCallback->call(callData);
            result += string.midRef(lastEnd, matchStart - lastEnd);
            result += replacement->toString(ctx)->toQString();
            lastEnd = matchEnd;
        }
        result += string.midRef(lastEnd);
    } else {
        QString newString = replaceValue->toString(ctx)->toQString();
        result.reserve(string.length() + numStringMatches*newString.size());

        int lastEnd = 0;
        for (int i = 0; i < numStringMatches; ++i) {
            int baseIndex = i * numCaptures * 2;
            uint matchStart = matchOffsets[baseIndex];
            uint matchEnd = matchOffsets[baseIndex + 1];
            if (matchStart == JSC::Yarr::offsetNoMatch)
                continue;

            result += string.midRef(lastEnd, matchStart - lastEnd);
            appendReplacementString(&result, string, newString, matchOffsets + baseIndex, numCaptures);
            lastEnd = matchEnd;
        }
        result += string.midRef(lastEnd);
    }

    if (matchOffsets != _matchOffsets)
        free(matchOffsets);

    return ctx->d()->engine->newString(result)->asReturnedValue();
}

ReturnedValue StringPrototype::method_search(CallContext *ctx)
{
    Scope scope(ctx);
    QString string = getThisString(ctx);
    ScopedValue regExpValue(scope, ctx->argument(0));
    if (scope.engine->hasException)
        return Encode::undefined();
    Scoped<RegExpObject> regExp(scope, regExpValue->as<RegExpObject>());
    if (!regExp) {
        ScopedCallData callData(scope, 1);
        callData->args[0] = regExpValue;
        regExpValue = ctx->d()->engine->regExpCtor.asFunctionObject()->construct(callData);
        if (scope.engine->hasException)
            return Encode::undefined();
        regExp = regExpValue->as<RegExpObject>();
        Q_ASSERT(regExp);
    }
    uint* matchOffsets = (uint*)alloca(regExp->value()->captureCount() * 2 * sizeof(uint));
    uint result = regExp->value()->match(string, /*offset*/0, matchOffsets);
    if (result == JSC::Yarr::offsetNoMatch)
        return Encode(-1);
    return Encode(result);
}

ReturnedValue StringPrototype::method_slice(CallContext *ctx)
{
    const QString text = getThisString(ctx);
    if (ctx->d()->engine->hasException)
        return Encode::undefined();

    const double length = text.length();

    double start = ctx->d()->callData->argc ? ctx->d()->callData->args[0].toInteger() : 0;
    double end = (ctx->d()->callData->argc < 2 || ctx->d()->callData->args[1].isUndefined())
            ? length : ctx->d()->callData->args[1].toInteger();

    if (start < 0)
        start = qMax(length + start, 0.);
    else
        start = qMin(start, length);

    if (end < 0)
        end = qMax(length + end, 0.);
    else
        end = qMin(end, length);

    const int intStart = int(start);
    const int intEnd = int(end);

    int count = qMax(0, intEnd - intStart);
    return ctx->d()->engine->newString(text.mid(intStart, count))->asReturnedValue();
}

ReturnedValue StringPrototype::method_split(CallContext *ctx)
{
    Scope scope(ctx);
    QString text = getThisString(ctx);
    if (scope.engine->hasException)
        return Encode::undefined();

    ScopedValue separatorValue(scope, ctx->argument(0));
    ScopedValue limitValue(scope, ctx->argument(1));

    ScopedArrayObject array(scope, ctx->d()->engine->newArrayObject());

    if (separatorValue->isUndefined()) {
        if (limitValue->isUndefined()) {
            ScopedString s(scope, ctx->d()->engine->newString(text));
            array->push_back(s);
            return array.asReturnedValue();
        }
        return ctx->d()->engine->newString(text.left(limitValue->toInteger()))->asReturnedValue();
    }

    uint limit = limitValue->isUndefined() ? UINT_MAX : limitValue->toUInt32();

    if (limit == 0)
        return array.asReturnedValue();

    Scoped<RegExpObject> re(scope, separatorValue);
    if (re) {
        if (re->value()->pattern().isEmpty()) {
            re = (RegExpObject *)0;
            separatorValue = ctx->d()->engine->newString(QString());
        }
    }

    ScopedString s(scope);
    if (re) {
        uint offset = 0;
        uint* matchOffsets = (uint*)alloca(re->value()->captureCount() * 2 * sizeof(uint));
        while (true) {
            uint result = re->value()->match(text, offset, matchOffsets);
            if (result == JSC::Yarr::offsetNoMatch)
                break;

            array->push_back((s = ctx->d()->engine->newString(text.mid(offset, matchOffsets[0] - offset))));
            offset = qMax(offset + 1, matchOffsets[1]);

            if (array->getLength() >= limit)
                break;

            for (int i = 1; i < re->value()->captureCount(); ++i) {
                uint start = matchOffsets[i * 2];
                uint end = matchOffsets[i * 2 + 1];
                array->push_back((s = ctx->d()->engine->newString(text.mid(start, end - start))));
                if (array->getLength() >= limit)
                    break;
            }
        }
        if (array->getLength() < limit)
            array->push_back((s = ctx->d()->engine->newString(text.mid(offset))));
    } else {
        QString separator = separatorValue->toString(ctx)->toQString();
        if (separator.isEmpty()) {
            for (uint i = 0; i < qMin(limit, uint(text.length())); ++i)
                array->push_back((s = ctx->d()->engine->newString(text.mid(i, 1))));
            return array.asReturnedValue();
        }

        int start = 0;
        int end;
        while ((end = text.indexOf(separator, start)) != -1) {
            array->push_back((s = ctx->d()->engine->newString(text.mid(start, end - start))));
            start = end + separator.size();
            if (array->getLength() >= limit)
                break;
        }
        if (array->getLength() < limit && start != -1)
            array->push_back((s = ctx->d()->engine->newString(text.mid(start))));
    }
    return array.asReturnedValue();
}

ReturnedValue StringPrototype::method_substr(CallContext *context)
{
    const QString value = getThisString(context);
    if (context->d()->engine->hasException)
        return Encode::undefined();

    double start = 0;
    if (context->d()->callData->argc > 0)
        start = context->d()->callData->args[0].toInteger();

    double length = +qInf();
    if (context->d()->callData->argc > 1)
        length = context->d()->callData->args[1].toInteger();

    double count = value.length();
    if (start < 0)
        start = qMax(count + start, 0.0);

    length = qMin(qMax(length, 0.0), count - start);

    qint32 x = Primitive::toInt32(start);
    qint32 y = Primitive::toInt32(length);
    return context->d()->engine->newString(value.mid(x, y))->asReturnedValue();
}

ReturnedValue StringPrototype::method_substring(CallContext *context)
{
    QString value = getThisString(context);
    if (context->d()->engine->hasException)
        return Encode::undefined();
    int length = value.length();

    double start = 0;
    double end = length;

    if (context->d()->callData->argc > 0)
        start = context->d()->callData->args[0].toInteger();

    Scope scope(context);
    ScopedValue endValue(scope, context->argument(1));
    if (!endValue->isUndefined())
        end = endValue->toInteger();

    if (std::isnan(start) || start < 0)
        start = 0;

    if (std::isnan(end) || end < 0)
        end = 0;

    if (start > length)
        start = length;

    if (end > length)
        end = length;

    if (start > end) {
        double was = start;
        start = end;
        end = was;
    }

    qint32 x = (int)start;
    qint32 y = (int)(end - start);
    return context->d()->engine->newString(value.mid(x, y))->asReturnedValue();
}

ReturnedValue StringPrototype::method_toLowerCase(CallContext *ctx)
{
    QString value = getThisString(ctx);
    if (ctx->d()->engine->hasException)
        return Encode::undefined();
    return ctx->d()->engine->newString(value.toLower())->asReturnedValue();
}

ReturnedValue StringPrototype::method_toLocaleLowerCase(CallContext *ctx)
{
    return method_toLowerCase(ctx);
}

ReturnedValue StringPrototype::method_toUpperCase(CallContext *ctx)
{
    QString value = getThisString(ctx);
    if (ctx->d()->engine->hasException)
        return Encode::undefined();
    return ctx->d()->engine->newString(value.toUpper())->asReturnedValue();
}

ReturnedValue StringPrototype::method_toLocaleUpperCase(CallContext *ctx)
{
    return method_toUpperCase(ctx);
}

ReturnedValue StringPrototype::method_fromCharCode(CallContext *context)
{
    QString str(context->d()->callData->argc, Qt::Uninitialized);
    QChar *ch = str.data();
    for (int i = 0; i < context->d()->callData->argc; ++i) {
        *ch = QChar(context->d()->callData->args[i].toUInt16());
        ++ch;
    }
    return context->d()->engine->newString(str)->asReturnedValue();
}

ReturnedValue StringPrototype::method_trim(CallContext *ctx)
{
    QString s = getThisString(ctx);
    if (ctx->d()->engine->hasException)
        return Encode::undefined();

    const QChar *chars = s.constData();
    int start, end;
    for (start = 0; start < s.length(); ++start) {
        if (!chars[start].isSpace() && chars[start].unicode() != 0xfeff)
            break;
    }
    for (end = s.length() - 1; end >= start; --end) {
        if (!chars[end].isSpace() && chars[end].unicode() != 0xfeff)
            break;
    }

    return ctx->d()->engine->newString(QString(chars + start, end - start + 1))->asReturnedValue();
}
