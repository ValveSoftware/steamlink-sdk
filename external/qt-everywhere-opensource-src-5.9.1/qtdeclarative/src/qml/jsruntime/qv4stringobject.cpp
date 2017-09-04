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


#include "qv4stringobject_p.h"
#include "qv4regexp_p.h"
#include "qv4regexpobject_p.h"
#include "qv4objectproto_p.h"
#include <private/qv4mm_p.h>
#include "qv4scopedvalue_p.h"
#include "qv4alloca_p.h"
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QStringList>

#include <private/qqmljsengine_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljsast_p.h>
#include <qv4jsir_p.h>
#include <qv4codegen_p.h>

#include <cassert>

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

void Heap::StringObject::init()
{
    Object::init();
    Q_ASSERT(vtable() == QV4::StringObject::staticVTable());
    string = internalClass->engine->id_empty()->d();
    *propertyData(LengthPropertyIndex) = Primitive::fromInt32(0);
}

void Heap::StringObject::init(const QV4::String *str)
{
    Object::init();
    string = str->d();
    *propertyData(LengthPropertyIndex) = Primitive::fromInt32(length());
}

Heap::String *Heap::StringObject::getIndex(uint index) const
{
    QString str = string->toQString();
    if (index >= (uint)str.length())
        return 0;
    return internalClass->engine->newString(str.mid(index, 1));
}

uint Heap::StringObject::length() const
{
    return string->len;
}

bool StringObject::deleteIndexedProperty(Managed *m, uint index)
{
    ExecutionEngine *v4 = static_cast<StringObject *>(m)->engine();
    Scope scope(v4);
    Scoped<StringObject> o(scope, m->as<StringObject>());
    Q_ASSERT(!!o);

    if (index < static_cast<uint>(o->d()->string->toQString().length())) {
        if (v4->current->strictMode)
            v4->throwTypeError();
        return false;
    }
    return true;
}

void StringObject::advanceIterator(Managed *m, ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attrs)
{
    name->setM(0);
    StringObject *s = static_cast<StringObject *>(m);
    uint slen = s->d()->string->toQString().length();
    if (it->arrayIndex <= slen) {
        while (it->arrayIndex < slen) {
            *index = it->arrayIndex;
            ++it->arrayIndex;
            PropertyAttributes a;
            Property pd;
            s->getOwnProperty(*index, &a, &pd);
            if (!(it->flags & ObjectIterator::EnumerableOnly) || a.isEnumerable()) {
                *attrs = a;
                p->copy(&pd, a);
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

void StringObject::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    StringObject::Data *o = static_cast<StringObject::Data *>(that);
    o->string->mark(e);
    Object::markObjects(that, e);
}

DEFINE_OBJECT_VTABLE(StringCtor);

void Heap::StringCtor::init(QV4::ExecutionContext *scope)
{
    Heap::FunctionObject::init(scope, QStringLiteral("String"));
}

void StringCtor::construct(const Managed *m, Scope &scope, CallData *callData)
{
    ExecutionEngine *v4 = static_cast<const Object *>(m)->engine();
    ScopedString value(scope);
    if (callData->argc)
        value = callData->args[0].toString(v4);
    else
        value = v4->newString();
    scope.result = Encode(v4->newStringObject(value));
}

void StringCtor::call(const Managed *, Scope &scope, CallData *callData)
{
    ExecutionEngine *v4 = scope.engine;
    if (callData->argc)
        scope.result = callData->args[0].toString(v4);
    else
        scope.result = v4->newString();
}

void StringPrototype::init(ExecutionEngine *engine, Object *ctor)
{
    Scope scope(engine);
    ScopedObject o(scope);

    ctor->defineReadonlyProperty(engine->id_prototype(), (o = this));
    ctor->defineReadonlyProperty(engine->id_length(), Primitive::fromInt32(1));
    ctor->defineDefaultProperty(QStringLiteral("fromCharCode"), method_fromCharCode, 1);

    defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    defineDefaultProperty(engine->id_toString(), method_toString);
    defineDefaultProperty(engine->id_valueOf(), method_toString); // valueOf and toString are identical
    defineDefaultProperty(QStringLiteral("charAt"), method_charAt, 1);
    defineDefaultProperty(QStringLiteral("charCodeAt"), method_charCodeAt, 1);
    defineDefaultProperty(QStringLiteral("concat"), method_concat, 1);
    defineDefaultProperty(QStringLiteral("endsWith"), method_endsWith, 1);
    defineDefaultProperty(QStringLiteral("indexOf"), method_indexOf, 1);
    defineDefaultProperty(QStringLiteral("includes"), method_includes, 1);
    defineDefaultProperty(QStringLiteral("lastIndexOf"), method_lastIndexOf, 1);
    defineDefaultProperty(QStringLiteral("localeCompare"), method_localeCompare, 1);
    defineDefaultProperty(QStringLiteral("match"), method_match, 1);
    defineDefaultProperty(QStringLiteral("replace"), method_replace, 2);
    defineDefaultProperty(QStringLiteral("search"), method_search, 1);
    defineDefaultProperty(QStringLiteral("slice"), method_slice, 2);
    defineDefaultProperty(QStringLiteral("split"), method_split, 2);
    defineDefaultProperty(QStringLiteral("startsWith"), method_startsWith, 1);
    defineDefaultProperty(QStringLiteral("substr"), method_substr, 2);
    defineDefaultProperty(QStringLiteral("substring"), method_substring, 2);
    defineDefaultProperty(QStringLiteral("toLowerCase"), method_toLowerCase);
    defineDefaultProperty(QStringLiteral("toLocaleLowerCase"), method_toLocaleLowerCase);
    defineDefaultProperty(QStringLiteral("toUpperCase"), method_toUpperCase);
    defineDefaultProperty(QStringLiteral("toLocaleUpperCase"), method_toLocaleUpperCase);
    defineDefaultProperty(QStringLiteral("trim"), method_trim);
}

static QString getThisString(Scope &scope, CallData *callData)
{
    ScopedValue t(scope, callData->thisObject);
    if (String *s = t->stringValue())
        return s->toQString();
    if (StringObject *thisString = t->as<StringObject>())
        return thisString->d()->string->toQString();
    if (t->isUndefined() || t->isNull()) {
        scope.engine->throwTypeError();
        return QString();
    }
    return t->toQString();
}

void StringPrototype::method_toString(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->thisObject.isString())
        RETURN_RESULT(callData->thisObject);

    StringObject *o = callData->thisObject.as<StringObject>();
    if (!o)
        THROW_TYPE_ERROR();
    scope.result = o->d()->string;
}

void StringPrototype::method_charAt(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    const QString str = getThisString(scope, callData);
    CHECK_EXCEPTION();

    int pos = 0;
    if (callData->argc > 0)
        pos = (int) callData->args[0].toInteger();

    QString result;
    if (pos >= 0 && pos < str.length())
        result += str.at(pos);

    scope.result = scope.engine->newString(result);
}

void StringPrototype::method_charCodeAt(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    const QString str = getThisString(scope, callData);
    CHECK_EXCEPTION();

    int pos = 0;
    if (callData->argc > 0)
        pos = (int) callData->args[0].toInteger();


    if (pos >= 0 && pos < str.length())
        RETURN_RESULT(Encode(str.at(pos).unicode()));

    scope.result = Encode(qt_qnan());
}

void StringPrototype::method_concat(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    ScopedString s(scope);
    for (int i = 0; i < callData->argc; ++i) {
        s = callData->args[i].toString(scope.engine);
        CHECK_EXCEPTION();

        Q_ASSERT(s->isString());
        value += s->toQString();
    }

    scope.result = scope.engine->newString(value);
}

void StringPrototype::method_endsWith(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    QString searchString;
    if (callData->argc) {
        if (callData->args[0].as<RegExpObject>())
            THROW_TYPE_ERROR();
        searchString = callData->args[0].toQString();
    }

    int pos = value.length();
    if (callData->argc > 1)
        pos = (int) callData->args[1].toInteger();

    if (pos == value.length())
        RETURN_RESULT(Encode(value.endsWith(searchString)));

    QStringRef stringToSearch = value.leftRef(pos);
    scope.result = Encode(stringToSearch.endsWith(searchString));
}

void StringPrototype::method_indexOf(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    QString searchString;
    if (callData->argc)
        searchString = callData->args[0].toQString();

    int pos = 0;
    if (callData->argc > 1)
        pos = (int) callData->args[1].toInteger();

    int index = -1;
    if (! value.isEmpty())
        index = value.indexOf(searchString, qMin(qMax(pos, 0), value.length()));

    scope.result = Encode(index);
}

void StringPrototype::method_includes(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    QString searchString;
    if (callData->argc) {
        if (callData->args[0].as<RegExpObject>())
            THROW_TYPE_ERROR();
        searchString = callData->args[0].toQString();
    }

    int pos = 0;
    if (callData->argc > 1) {
        ScopedValue posArg(scope, callData->argument(1));
        pos = (int) posArg->toInteger();
        if (!posArg->isInteger() && posArg->isNumber() && qIsInf(posArg->toNumber()))
            pos = value.length();
    }

    if (pos == 0)
        RETURN_RESULT(Encode(value.contains(searchString)));

    QStringRef stringToSearch = value.midRef(pos);
    scope.result = Encode(stringToSearch.contains(searchString));
}

void StringPrototype::method_lastIndexOf(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    const QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    QString searchString;
    if (callData->argc)
        searchString = callData->args[0].toQString();

    ScopedValue posArg(scope, callData->argument(1));
    double position = RuntimeHelpers::toNumber(posArg);
    if (std::isnan(position))
        position = +qInf();
    else
        position = trunc(position);

    int pos = trunc(qMin(qMax(position, 0.0), double(value.length())));
    if (!searchString.isEmpty() && pos == value.length())
        --pos;
    if (searchString.isNull() && pos == 0)
        RETURN_RESULT(Encode(-1));
    int index = value.lastIndexOf(searchString, pos);
    scope.result = Encode(index);
}

void StringPrototype::method_localeCompare(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    const QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    ScopedValue v(scope, callData->argument(0));
    const QString that = v->toQString();
    scope.result = Encode(QString::localeAwareCompare(value, that));
}

void StringPrototype::method_match(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->thisObject.isUndefined() || callData->thisObject.isNull())
        THROW_TYPE_ERROR();

    ScopedString s(scope, callData->thisObject.toString(scope.engine));

    ScopedValue regexp(scope, callData->argument(0));
    Scoped<RegExpObject> rx(scope, regexp);
    if (!rx) {
        ScopedCallData callData(scope, 1);
        callData->args[0] = regexp;
        scope.engine->regExpCtor()->construct(scope, callData);
        rx = scope.result.asReturnedValue();
    }

    if (!rx)
        // ### CHECK
        THROW_TYPE_ERROR();

    bool global = rx->global();

    // ### use the standard builtin function, not the one that might be redefined in the proto
    ScopedString execString(scope, scope.engine->newString(QStringLiteral("exec")));
    ScopedFunctionObject exec(scope, scope.engine->regExpPrototype()->get(execString));

    ScopedCallData cData(scope, 1);
    cData->thisObject = rx;
    cData->args[0] = s;
    if (!global) {
        exec->call(scope, cData);
        return;
    }

    ScopedString lastIndex(scope, scope.engine->newString(QStringLiteral("lastIndex")));
    rx->put(lastIndex, ScopedValue(scope, Primitive::fromInt32(0)));
    ScopedArrayObject a(scope, scope.engine->newArrayObject());

    double previousLastIndex = 0;
    uint n = 0;
    ScopedValue matchStr(scope);
    ScopedValue index(scope);
    while (1) {
        exec->call(scope, cData);
        if (scope.result.isNull())
            break;
        assert(scope.result.isObject());
        index = rx->get(lastIndex, 0);
        double thisIndex = index->toInteger();
        if (previousLastIndex == thisIndex) {
            previousLastIndex = thisIndex + 1;
            rx->put(lastIndex, ScopedValue(scope, Primitive::fromDouble(previousLastIndex)));
        } else {
            previousLastIndex = thisIndex;
        }
        matchStr = scope.result.objectValue()->getIndexed(0);
        a->arraySet(n, matchStr);
        ++n;
    }
    if (!n)
        scope.result = Encode::null();
    else
        scope.result = a;
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

void StringPrototype::method_replace(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString string;
    if (StringObject *thisString = callData->thisObject.as<StringObject>())
        string = thisString->d()->string->toQString();
    else
        string = callData->thisObject.toQString();

    int numCaptures = 0;
    int numStringMatches = 0;

    uint allocatedMatchOffsets = 64;
    uint _matchOffsets[64];
    uint *matchOffsets = _matchOffsets;

    ScopedValue searchValue(scope, callData->argument(0));
    Scoped<RegExpObject> regExp(scope, searchValue);
    if (regExp) {
        uint offset = 0;
        uint nMatchOffsets = 0;

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
            *regExp->lastIndexProperty() = Primitive::fromUInt32(0);
        numStringMatches = nMatchOffsets / (regExp->value()->captureCount() * 2);
        numCaptures = regExp->value()->captureCount();
    } else {
        numCaptures = 1;
        QString searchString = searchValue->toQString();
        int idx = string.indexOf(searchString);
        if (idx != -1) {
            numStringMatches = 1;
            matchOffsets[0] = idx;
            matchOffsets[1] = idx + searchString.length();
        }
    }

    QString result;
    ScopedValue replaceValue(scope, callData->argument(1));
    ScopedFunctionObject searchCallback(scope, replaceValue);
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
                    entry = scope.engine->newString(string.mid(start, end - start));
                callData->args[k] = entry;
            }
            uint matchStart = matchOffsets[i * numCaptures * 2];
            Q_ASSERT(matchStart >= static_cast<uint>(lastEnd));
            uint matchEnd = matchOffsets[i * numCaptures * 2 + 1];
            callData->args[numCaptures] = Primitive::fromUInt32(matchStart);
            callData->args[numCaptures + 1] = scope.engine->newString(string);

            searchCallback->call(scope, callData);
            result += string.midRef(lastEnd, matchStart - lastEnd);
            result += scope.result.toQString();
            lastEnd = matchEnd;
        }
        result += string.midRef(lastEnd);
    } else {
        QString newString = replaceValue->toQString();
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

    scope.result = scope.engine->newString(result);
}

void StringPrototype::method_search(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString string = getThisString(scope, callData);
    scope.result = callData->argument(0);
    CHECK_EXCEPTION();

    Scoped<RegExpObject> regExp(scope, scope.result.as<RegExpObject>());
    if (!regExp) {
        ScopedCallData callData(scope, 1);
        callData->args[0] = scope.result;
        scope.engine->regExpCtor()->construct(scope, callData);
        CHECK_EXCEPTION();

        regExp = scope.result.as<RegExpObject>();
        Q_ASSERT(regExp);
    }
    Scoped<RegExp> re(scope, regExp->value());
    Q_ALLOCA_VAR(uint, matchOffsets, regExp->value()->captureCount() * 2 * sizeof(uint));
    uint result = re->match(string, /*offset*/0, matchOffsets);
    if (result == JSC::Yarr::offsetNoMatch)
        scope.result = Encode(-1);
    else
        scope.result = Encode(result);
}

void StringPrototype::method_slice(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    const QString text = getThisString(scope, callData);
    CHECK_EXCEPTION();

    const double length = text.length();

    double start = callData->argc ? callData->args[0].toInteger() : 0;
    double end = (callData->argc < 2 || callData->args[1].isUndefined())
            ? length : callData->args[1].toInteger();

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
    scope.result = scope.engine->newString(text.mid(intStart, count));
}

void StringPrototype::method_split(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString text = getThisString(scope, callData);
    CHECK_EXCEPTION();

    ScopedValue separatorValue(scope, callData->argument(0));
    ScopedValue limitValue(scope, callData->argument(1));

    ScopedArrayObject array(scope, scope.engine->newArrayObject());

    if (separatorValue->isUndefined()) {
        if (limitValue->isUndefined()) {
            ScopedString s(scope, scope.engine->newString(text));
            array->push_back(s);
            RETURN_RESULT(array);
        }
        RETURN_RESULT(scope.engine->newString(text.left(limitValue->toInteger())));
    }

    uint limit = limitValue->isUndefined() ? UINT_MAX : limitValue->toUInt32();

    if (limit == 0)
        RETURN_RESULT(array);

    Scoped<RegExpObject> re(scope, separatorValue);
    if (re) {
        if (re->value()->pattern->isEmpty()) {
            re = (RegExpObject *)0;
            separatorValue = scope.engine->newString();
        }
    }

    ScopedString s(scope);
    if (re) {
        uint offset = 0;
        Q_ALLOCA_VAR(uint, matchOffsets, re->value()->captureCount() * 2 * sizeof(uint));
        while (true) {
            Scoped<RegExp> regexp(scope, re->value());
            uint result = regexp->match(text, offset, matchOffsets);
            if (result == JSC::Yarr::offsetNoMatch)
                break;

            array->push_back((s = scope.engine->newString(text.mid(offset, matchOffsets[0] - offset))));
            offset = qMax(offset + 1, matchOffsets[1]);

            if (array->getLength() >= limit)
                break;

            for (int i = 1; i < re->value()->captureCount(); ++i) {
                uint start = matchOffsets[i * 2];
                uint end = matchOffsets[i * 2 + 1];
                array->push_back((s = scope.engine->newString(text.mid(start, end - start))));
                if (array->getLength() >= limit)
                    break;
            }
        }
        if (array->getLength() < limit)
            array->push_back((s = scope.engine->newString(text.mid(offset))));
    } else {
        QString separator = separatorValue->toQString();
        if (separator.isEmpty()) {
            for (uint i = 0; i < qMin(limit, uint(text.length())); ++i)
                array->push_back((s = scope.engine->newString(text.mid(i, 1))));
            RETURN_RESULT(array);
        }

        int start = 0;
        int end;
        while ((end = text.indexOf(separator, start)) != -1) {
            array->push_back((s = scope.engine->newString(text.mid(start, end - start))));
            start = end + separator.size();
            if (array->getLength() >= limit)
                break;
        }
        if (array->getLength() < limit && start != -1)
            array->push_back((s = scope.engine->newString(text.mid(start))));
    }
    RETURN_RESULT(array);
}

void StringPrototype::method_startsWith(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    QString searchString;
    if (callData->argc) {
        if (callData->args[0].as<RegExpObject>())
            THROW_TYPE_ERROR();
        searchString = callData->args[0].toQString();
    }

    int pos = 0;
    if (callData->argc > 1)
        pos = (int) callData->args[1].toInteger();

    if (pos == 0)
        RETURN_RESULT(Encode(value.startsWith(searchString)));

    QStringRef stringToSearch = value.midRef(pos);
    RETURN_RESULT(Encode(stringToSearch.startsWith(searchString)));
}

void StringPrototype::method_substr(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    const QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    double start = 0;
    if (callData->argc > 0)
        start = callData->args[0].toInteger();

    double length = +qInf();
    if (callData->argc > 1)
        length = callData->args[1].toInteger();

    double count = value.length();
    if (start < 0)
        start = qMax(count + start, 0.0);

    length = qMin(qMax(length, 0.0), count - start);

    qint32 x = Primitive::toInt32(start);
    qint32 y = Primitive::toInt32(length);
    scope.result = scope.engine->newString(value.mid(x, y));
}

void StringPrototype::method_substring(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    int length = value.length();

    double start = 0;
    double end = length;

    if (callData->argc > 0)
        start = callData->args[0].toInteger();

    ScopedValue endValue(scope, callData->argument(1));
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
    scope.result = scope.engine->newString(value.mid(x, y));
}

void StringPrototype::method_toLowerCase(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    scope.result = scope.engine->newString(value.toLower());
}

void StringPrototype::method_toLocaleLowerCase(const BuiltinFunction *b, Scope &scope, CallData *callData)
{
    method_toLowerCase(b, scope, callData);
}

void StringPrototype::method_toUpperCase(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString value = getThisString(scope, callData);
    CHECK_EXCEPTION();

    scope.result = scope.engine->newString(value.toUpper());
}

void StringPrototype::method_toLocaleUpperCase(const BuiltinFunction *b, Scope &scope, CallData *callData)
{
    return method_toUpperCase(b, scope, callData);
}

void StringPrototype::method_fromCharCode(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString str(callData->argc, Qt::Uninitialized);
    QChar *ch = str.data();
    for (int i = 0; i < callData->argc; ++i) {
        *ch = QChar(callData->args[i].toUInt16());
        ++ch;
    }
    scope.result = scope.engine->newString(str);
}

void StringPrototype::method_trim(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    QString s = getThisString(scope, callData);
    CHECK_EXCEPTION();

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

    scope.result = scope.engine->newString(QString(chars + start, end - start + 1));
}
