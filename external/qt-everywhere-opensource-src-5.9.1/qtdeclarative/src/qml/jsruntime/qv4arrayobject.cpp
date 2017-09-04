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

#include "qv4arrayobject_p.h"
#include "qv4sparsearray_p.h"
#include "qv4objectproto_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4argumentsobject_p.h"
#include "qv4runtime_p.h"
#include "qv4string_p.h"
#include <QtCore/qscopedvaluerollback.h>

using namespace QV4;

DEFINE_OBJECT_VTABLE(ArrayCtor);

void Heap::ArrayCtor::init(QV4::ExecutionContext *scope)
{
    Heap::FunctionObject::init(scope, QStringLiteral("Array"));
}

void ArrayCtor::construct(const Managed *m, Scope &scope, CallData *callData)
{
    ExecutionEngine *v4 = static_cast<const ArrayCtor *>(m)->engine();
    ScopedArrayObject a(scope, v4->newArrayObject());
    uint len;
    if (callData->argc == 1 && callData->args[0].isNumber()) {
        bool ok;
        len = callData->args[0].asArrayLength(&ok);

        if (!ok) {
            scope.result = v4->throwRangeError(callData->args[0]);
            return;
        }

        if (len < 0x1000)
            a->arrayReserve(len);
    } else {
        len = callData->argc;
        a->arrayReserve(len);
        a->arrayPut(0, callData->args, len);
    }
    a->setArrayLengthUnchecked(len);

    scope.result = a.asReturnedValue();
}

void ArrayCtor::call(const Managed *that, Scope &scope, CallData *callData)
{
    construct(that, scope, callData);
}

void ArrayPrototype::init(ExecutionEngine *engine, Object *ctor)
{
    Scope scope(engine);
    ScopedObject o(scope);
    ctor->defineReadonlyProperty(engine->id_length(), Primitive::fromInt32(1));
    ctor->defineReadonlyProperty(engine->id_prototype(), (o = this));
    ctor->defineDefaultProperty(QStringLiteral("isArray"), method_isArray, 1);
    defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    defineDefaultProperty(engine->id_toString(), method_toString, 0);
    defineDefaultProperty(QStringLiteral("toLocaleString"), method_toLocaleString, 0);
    defineDefaultProperty(QStringLiteral("concat"), method_concat, 1);
    defineDefaultProperty(QStringLiteral("find"), method_find, 1);
    defineDefaultProperty(QStringLiteral("findIndex"), method_findIndex, 1);
    defineDefaultProperty(QStringLiteral("join"), method_join, 1);
    defineDefaultProperty(QStringLiteral("pop"), method_pop, 0);
    defineDefaultProperty(QStringLiteral("push"), method_push, 1);
    defineDefaultProperty(QStringLiteral("reverse"), method_reverse, 0);
    defineDefaultProperty(QStringLiteral("shift"), method_shift, 0);
    defineDefaultProperty(QStringLiteral("slice"), method_slice, 2);
    defineDefaultProperty(QStringLiteral("sort"), method_sort, 1);
    defineDefaultProperty(QStringLiteral("splice"), method_splice, 2);
    defineDefaultProperty(QStringLiteral("unshift"), method_unshift, 1);
    defineDefaultProperty(QStringLiteral("indexOf"), method_indexOf, 1);
    defineDefaultProperty(QStringLiteral("lastIndexOf"), method_lastIndexOf, 1);
    defineDefaultProperty(QStringLiteral("every"), method_every, 1);
    defineDefaultProperty(QStringLiteral("some"), method_some, 1);
    defineDefaultProperty(QStringLiteral("forEach"), method_forEach, 1);
    defineDefaultProperty(QStringLiteral("map"), method_map, 1);
    defineDefaultProperty(QStringLiteral("filter"), method_filter, 1);
    defineDefaultProperty(QStringLiteral("reduce"), method_reduce, 1);
    defineDefaultProperty(QStringLiteral("reduceRight"), method_reduceRight, 1);
}

void ArrayPrototype::method_isArray(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    bool isArray = callData->argc && callData->args[0].as<ArrayObject>();
    scope.result = Encode(isArray);
}

void ArrayPrototype::method_toString(const BuiltinFunction *builtin, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->thisObject, ScopedObject::Convert);
    CHECK_EXCEPTION();
    ScopedString s(scope, scope.engine->newString(QStringLiteral("join")));
    ScopedFunctionObject f(scope, o->get(s));
    if (!!f) {
        ScopedCallData d(scope, 0);
        d->thisObject = callData->thisObject;
        f->call(scope, d);
        return;
    }
    ObjectPrototype::method_toString(builtin, scope, callData);
}

void ArrayPrototype::method_toLocaleString(const BuiltinFunction *builtin, Scope &scope, CallData *callData)
{
    return method_toString(builtin, scope, callData);
}

void ArrayPrototype::method_concat(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject thisObject(scope, callData->thisObject.toObject(scope.engine));
    if (!thisObject)
        RETURN_UNDEFINED();

    ScopedArrayObject result(scope, scope.engine->newArrayObject());

    if (thisObject->isArrayObject()) {
        result->copyArrayData(thisObject);
    } else {
        result->arraySet(0, thisObject);
    }

    ScopedArrayObject elt(scope);
    ScopedObject eltAsObj(scope);
    ScopedValue entry(scope);
    for (int i = 0; i < callData->argc; ++i) {
        eltAsObj = callData->args[i];
        elt = callData->args[i];
        if (elt) {
            uint n = elt->getLength();
            uint newLen = ArrayData::append(result, elt, n);
            result->setArrayLengthUnchecked(newLen);
        } else if (eltAsObj && eltAsObj->isListType()) {
            const uint startIndex = result->getLength();
            for (int i = 0, len = eltAsObj->getLength(); i < len; ++i) {
                entry = eltAsObj->getIndexed(i);
                result->putIndexed(startIndex + i, entry);
            }
        } else {
            result->arraySet(result->getLength(), callData->args[i]);
        }
    }

    scope.result = result;
}

void ArrayPrototype::method_find(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, callData->argument(0));
    if (!callback)
        THROW_TYPE_ERROR();

    ScopedCallData cData(scope, 3);
    cData->thisObject = callData->argument(1);
    cData->args[2] = instance;

    ScopedValue v(scope);

    for (uint k = 0; k < len; ++k) {
        v = instance->getIndexed(k);
        CHECK_EXCEPTION();

        cData->args[0] = v;
        cData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, cData);

        CHECK_EXCEPTION();
        if (scope.result.toBoolean())
            RETURN_RESULT(v);
    }

    RETURN_UNDEFINED();
}

void ArrayPrototype::method_findIndex(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, callData->argument(0));
    if (!callback)
        THROW_TYPE_ERROR();

    ScopedCallData cData(scope, 3);
    cData->thisObject = callData->argument(1);
    cData->args[2] = instance;

    ScopedValue v(scope);

    for (uint k = 0; k < len; ++k) {
        v = instance->getIndexed(k);
        CHECK_EXCEPTION();

        cData->args[0] = v;
        cData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, cData);

        CHECK_EXCEPTION();
        if (scope.result.toBoolean())
            RETURN_RESULT(Encode(k));
    }

    RETURN_RESULT(Encode(-1));
}

void ArrayPrototype::method_join(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedValue arg(scope, callData->argument(0));
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));

    if (!instance) {
        scope.result = scope.engine->newString();
        return;
    }

    QString r4;
    if (arg->isUndefined())
        r4 = QStringLiteral(",");
    else
        r4 = arg->toQString();

    ScopedValue length(scope, instance->get(scope.engine->id_length()));
    const quint32 r2 = length->isUndefined() ? 0 : length->toUInt32();

    if (!r2) {
        scope.result = scope.engine->newString();
        return;
    }

    QString R;

    // ### FIXME
    if (ArrayObject *a = instance->as<ArrayObject>()) {
        ScopedValue e(scope);
        for (uint i = 0; i < a->getLength(); ++i) {
            if (i)
                R += r4;

            e = a->getIndexed(i);
            CHECK_EXCEPTION();
            if (!e->isNullOrUndefined())
                R += e->toQString();
        }
    } else {
        //
        // crazy!
        //
        ScopedString name(scope, scope.engine->newString(QStringLiteral("0")));
        ScopedValue r6(scope, instance->get(name));
        if (!r6->isNullOrUndefined())
            R = r6->toQString();

        ScopedValue r12(scope);
        for (quint32 k = 1; k < r2; ++k) {
            R += r4;

            name = Primitive::fromDouble(k).toString(scope.engine);
            r12 = instance->get(name);
            CHECK_EXCEPTION();

            if (!r12->isNullOrUndefined())
                R += r12->toQString();
        }
    }

    scope.result = scope.engine->newString(R);
}

void ArrayPrototype::method_pop(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    if (!len) {
        if (!instance->isArrayObject())
            instance->put(scope.engine->id_length(), ScopedValue(scope, Primitive::fromInt32(0)));
        RETURN_UNDEFINED();
    }

    ScopedValue result(scope, instance->getIndexed(len - 1));
    CHECK_EXCEPTION();

    instance->deleteIndexedProperty(len - 1);
    CHECK_EXCEPTION();

    if (instance->isArrayObject())
        instance->setArrayLength(len - 1);
    else
        instance->put(scope.engine->id_length(), ScopedValue(scope, Primitive::fromDouble(len - 1)));
    scope.result = result;
}

void ArrayPrototype::method_push(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    instance->arrayCreate();
    Q_ASSERT(instance->arrayData());

    uint len = instance->getLength();

    if (len + callData->argc < len) {
        // ughh...
        double l = len;
        ScopedString s(scope);
        for (int i = 0; i < callData->argc; ++i) {
            s = Primitive::fromDouble(l + i).toString(scope.engine);
            instance->put(s, callData->args[i]);
        }
        double newLen = l + callData->argc;
        if (!instance->isArrayObject())
            instance->put(scope.engine->id_length(), ScopedValue(scope, Primitive::fromDouble(newLen)));
        else {
            ScopedString str(scope, scope.engine->newString(QStringLiteral("Array.prototype.push: Overflow")));
            scope.result = scope.engine->throwRangeError(str);
            return;
        }
        scope.result = Encode(newLen);
        return;
    }

    if (!callData->argc)
        ;
    else if (!instance->protoHasArray() && instance->arrayData()->length() <= len && instance->arrayData()->type == Heap::ArrayData::Simple) {
        instance->arrayData()->vtable()->putArray(instance, len, callData->args, callData->argc);
        len = instance->arrayData()->length();
    } else {
        for (int i = 0; i < callData->argc; ++i)
            instance->putIndexed(len + i, callData->args[i]);
        len += callData->argc;
    }
    if (instance->isArrayObject())
        instance->setArrayLengthUnchecked(len);
    else
        instance->put(scope.engine->id_length(), ScopedValue(scope, Primitive::fromDouble(len)));

    scope.result = Encode(len);
}

void ArrayPrototype::method_reverse(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint length = instance->getLength();

    int lo = 0, hi = length - 1;

    ScopedValue lval(scope);
    ScopedValue hval(scope);
    for (; lo < hi; ++lo, --hi) {
        bool loExists, hiExists;
        lval = instance->getIndexed(lo, &loExists);
        hval = instance->getIndexed(hi, &hiExists);
        CHECK_EXCEPTION();
        if (hiExists)
            instance->putIndexed(lo, hval);
        else
            instance->deleteIndexedProperty(lo);
        CHECK_EXCEPTION();
        if (loExists)
            instance->putIndexed(hi, lval);
        else
            instance->deleteIndexedProperty(hi);
    }
    scope.result = instance;
}

void ArrayPrototype::method_shift(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    instance->arrayCreate();
    Q_ASSERT(instance->arrayData());

    uint len = instance->getLength();

    if (!len) {
        if (!instance->isArrayObject())
            instance->put(scope.engine->id_length(), ScopedValue(scope, Primitive::fromInt32(0)));
        RETURN_UNDEFINED();
    }

    if (!instance->protoHasArray() && !instance->arrayData()->attrs && instance->arrayData()->length() <= len && instance->arrayData()->type != Heap::ArrayData::Custom) {
        scope.result = instance->arrayData()->vtable()->pop_front(instance);
    } else {
        scope.result = instance->getIndexed(0);
        CHECK_EXCEPTION();
        ScopedValue v(scope);
        // do it the slow way
        for (uint k = 1; k < len; ++k) {
            bool exists;
            v = instance->getIndexed(k, &exists);
            CHECK_EXCEPTION();
            if (exists)
                instance->putIndexed(k - 1, v);
            else
                instance->deleteIndexedProperty(k - 1);
            CHECK_EXCEPTION();
        }
        instance->deleteIndexedProperty(len - 1);
        CHECK_EXCEPTION();
    }

    if (instance->isArrayObject())
        instance->setArrayLengthUnchecked(len - 1);
    else
        instance->put(scope.engine->id_length(), ScopedValue(scope, Primitive::fromDouble(len - 1)));
}

void ArrayPrototype::method_slice(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->thisObject.toObject(scope.engine));
    if (!o)
        RETURN_UNDEFINED();

    ScopedArrayObject result(scope, scope.engine->newArrayObject());
    uint len = o->getLength();
    double s = ScopedValue(scope, callData->argument(0))->toInteger();
    uint start;
    if (s < 0)
        start = (uint)qMax(len + s, 0.);
    else if (s > len)
        start = len;
    else
        start = (uint) s;
    uint end = len;
    if (callData->argc > 1 && !callData->args[1].isUndefined()) {
        double e = callData->args[1].toInteger();
        if (e < 0)
            end = (uint)qMax(len + e, 0.);
        else if (e > len)
            end = len;
        else
            end = (uint) e;
    }

    ScopedValue v(scope);
    uint n = 0;
    for (uint i = start; i < end; ++i) {
        bool exists;
        v = o->getIndexed(i, &exists);
        CHECK_EXCEPTION();
        if (exists)
            result->arraySet(n, v);
        ++n;
    }
    scope.result = result;
}

void ArrayPrototype::method_sort(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedValue comparefn(scope, callData->argument(0));
    ArrayData::sort(scope.engine, instance, comparefn, len);
    scope.result = callData->thisObject;
}

void ArrayPrototype::method_splice(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedArrayObject newArray(scope, scope.engine->newArrayObject());

    double rs = ScopedValue(scope, callData->argument(0))->toInteger();
    uint start;
    if (rs < 0)
        start = (uint) qMax(0., len + rs);
    else
        start = (uint) qMin(rs, (double)len);

    uint deleteCount = (uint)qMin(qMax(ScopedValue(scope, callData->argument(1))->toInteger(), 0.), (double)(len - start));

    newArray->arrayReserve(deleteCount);
    ScopedValue v(scope);
    for (uint i = 0; i < deleteCount; ++i) {
        bool exists;
        v = instance->getIndexed(start + i, &exists);
        CHECK_EXCEPTION();
        if (exists)
            newArray->arrayPut(i, v);
    }
    newArray->setArrayLengthUnchecked(deleteCount);

    uint itemCount = callData->argc < 2 ? 0 : callData->argc - 2;

    if (itemCount < deleteCount) {
        for (uint k = start; k < len - deleteCount; ++k) {
            bool exists;
            v = instance->getIndexed(k + deleteCount, &exists);
            CHECK_EXCEPTION();
            if (exists)
                instance->putIndexed(k + itemCount, v);
            else
                instance->deleteIndexedProperty(k + itemCount);
            CHECK_EXCEPTION();
        }
        for (uint k = len; k > len - deleteCount + itemCount; --k) {
            instance->deleteIndexedProperty(k - 1);
            CHECK_EXCEPTION();
        }
    } else if (itemCount > deleteCount) {
        uint k = len - deleteCount;
        while (k > start) {
            bool exists;
            v = instance->getIndexed(k + deleteCount - 1, &exists);
            CHECK_EXCEPTION();
            if (exists)
                instance->putIndexed(k + itemCount - 1, v);
            else
                instance->deleteIndexedProperty(k + itemCount - 1);
            CHECK_EXCEPTION();
            --k;
        }
    }

    for (uint i = 0; i < itemCount; ++i) {
        instance->putIndexed(start + i, callData->args[i + 2]);
        CHECK_EXCEPTION();
    }

    bool wasStrict = scope.engine->current->strictMode;
    scope.engine->current->strictMode = true;
    instance->put(scope.engine->id_length(), ScopedValue(scope, Primitive::fromDouble(len - deleteCount + itemCount)));

    scope.result = newArray;
    scope.engine->current->strictMode = wasStrict;
}

void ArrayPrototype::method_unshift(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    instance->arrayCreate();
    Q_ASSERT(instance->arrayData());

    uint len = instance->getLength();

    if (!instance->protoHasArray() && !instance->arrayData()->attrs && instance->arrayData()->length() <= len &&
        instance->arrayData()->type != Heap::ArrayData::Custom) {
        instance->arrayData()->vtable()->push_front(instance, callData->args, callData->argc);
    } else {
        ScopedValue v(scope);
        for (uint k = len; k > 0; --k) {
            bool exists;
            v = instance->getIndexed(k - 1, &exists);
            if (exists)
                instance->putIndexed(k + callData->argc - 1, v);
            else
                instance->deleteIndexedProperty(k + callData->argc - 1);
        }
        for (int i = 0; i < callData->argc; ++i)
            instance->putIndexed(i, callData->args[i]);
    }

    uint newLen = len + callData->argc;
    if (instance->isArrayObject())
        instance->setArrayLengthUnchecked(newLen);
    else
        instance->put(scope.engine->id_length(), ScopedValue(scope, Primitive::fromDouble(newLen)));

    scope.result = Encode(newLen);
}

void ArrayPrototype::method_indexOf(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();
    if (!len) {
        scope.result = Encode(-1);
        return;
    }

    ScopedValue searchValue(scope, callData->argument(0));
    uint fromIndex = 0;

    if (callData->argc >= 2) {
        double f = callData->args[1].toInteger();
        CHECK_EXCEPTION();
        if (f >= len) {
            scope.result = Encode(-1);
            return;
        }
        if (f < 0)
            f = qMax(len + f, 0.);
        fromIndex = (uint) f;
    }

    if (instance->isStringObject()) {
        ScopedValue v(scope);
        for (uint k = fromIndex; k < len; ++k) {
            bool exists;
            v = instance->getIndexed(k, &exists);
            if (exists && RuntimeHelpers::strictEqual(v, searchValue)) {
                scope.result = Encode(k);
                return;
            }
        }
        scope.result = Encode(-1);
        return;
    }

    ScopedValue value(scope);

    if (ArgumentsObject::isNonStrictArgumentsObject(instance) ||
        (instance->arrayType() >= Heap::ArrayData::Sparse) || instance->protoHasArray()) {
        // lets be safe and slow
        for (uint i = fromIndex; i < len; ++i) {
            bool exists;
            value = instance->getIndexed(i, &exists);
            CHECK_EXCEPTION();
            if (exists && RuntimeHelpers::strictEqual(value, searchValue)) {
                scope.result = Encode(i);
                return;
            }
        }
    } else if (!instance->arrayData()) {
        scope.result = Encode(-1);
        return;
    } else {
        Q_ASSERT(instance->arrayType() == Heap::ArrayData::Simple || instance->arrayType() == Heap::ArrayData::Complex);
        Heap::SimpleArrayData *sa = instance->d()->arrayData.cast<Heap::SimpleArrayData>();
        if (len > sa->len)
            len = sa->len;
        uint idx = fromIndex;
        while (idx < len) {
            value = sa->data(idx);
            CHECK_EXCEPTION();
            if (RuntimeHelpers::strictEqual(value, searchValue)) {
                scope.result = Encode(idx);
                return;
            }
            ++idx;
        }
    }
    scope.result = Encode(-1);
}

void ArrayPrototype::method_lastIndexOf(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();
    if (!len) {
        scope.result = Encode(-1);
        return;
    }

    ScopedValue searchValue(scope);
    uint fromIndex = len;

    if (callData->argc >= 1)
        searchValue = callData->argument(0);
    else
        searchValue = Primitive::undefinedValue();

    if (callData->argc >= 2) {
        double f = callData->args[1].toInteger();
        CHECK_EXCEPTION();
        if (f > 0)
            f = qMin(f, (double)(len - 1));
        else if (f < 0) {
            f = len + f;
            if (f < 0) {
                scope.result = Encode(-1);
                return;
            }
        }
        fromIndex = (uint) f + 1;
    }

    ScopedValue v(scope);
    for (uint k = fromIndex; k > 0;) {
        --k;
        bool exists;
        v = instance->getIndexed(k, &exists);
        CHECK_EXCEPTION();
        if (exists && RuntimeHelpers::strictEqual(v, searchValue)) {
            scope.result = Encode(k);
            return;
        }
    }
    scope.result = Encode(-1);
}

void ArrayPrototype::method_every(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, callData->argument(0));
    if (!callback)
        THROW_TYPE_ERROR();

    ScopedCallData cData(scope, 3);
    cData->args[2] = instance;
    cData->thisObject = callData->argument(1);
    ScopedValue v(scope);

    bool ok = true;
    for (uint k = 0; ok && k < len; ++k) {
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (!exists)
            continue;

        cData->args[0] = v;
        cData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, cData);
        ok = scope.result.toBoolean();
    }
    scope.result = Encode(ok);
}

void ArrayPrototype::method_some(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, callData->argument(0));
    if (!callback)
        THROW_TYPE_ERROR();

    ScopedCallData cData(scope, 3);
    cData->thisObject = callData->argument(1);
    cData->args[2] = instance;
    ScopedValue v(scope);

    for (uint k = 0; k < len; ++k) {
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (!exists)
            continue;

        cData->args[0] = v;
        cData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, cData);
        if (scope.result.toBoolean()) {
            scope.result = Encode(true);
            return;
        }
    }
    scope.result = Encode(false);
}

void ArrayPrototype::method_forEach(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, callData->argument(0));
    if (!callback)
        THROW_TYPE_ERROR();

    ScopedCallData cData(scope, 3);
    cData->thisObject = callData->argument(1);
    cData->args[2] = instance;

    ScopedValue v(scope);
    for (uint k = 0; k < len; ++k) {
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (!exists)
            continue;

        cData->args[0] = v;
        cData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, cData);
    }
    RETURN_UNDEFINED();
}

void ArrayPrototype::method_map(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, callData->argument(0));
    if (!callback)
        THROW_TYPE_ERROR();

    ScopedArrayObject a(scope, scope.engine->newArrayObject());
    a->arrayReserve(len);
    a->setArrayLengthUnchecked(len);

    ScopedCallData cData(scope, 3);
    cData->thisObject = callData->argument(1);
    cData->args[2] = instance;

    ScopedValue v(scope);
    for (uint k = 0; k < len; ++k) {
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (!exists)
            continue;

        cData->args[0] = v;
        cData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, cData);
        a->arraySet(k, scope.result);
    }
    scope.result = a.asReturnedValue();
}

void ArrayPrototype::method_filter(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, callData->argument(0));
    if (!callback)
        THROW_TYPE_ERROR();

    ScopedArrayObject a(scope, scope.engine->newArrayObject());
    a->arrayReserve(len);

    ScopedCallData cData(scope, 3);
    cData->thisObject = callData->argument(1);
    cData->args[2] = instance;

    ScopedValue v(scope);

    uint to = 0;
    for (uint k = 0; k < len; ++k) {
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (!exists)
            continue;

        cData->args[0] = v;
        cData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, cData);
        if (scope.result.toBoolean()) {
            a->arraySet(to, v);
            ++to;
        }
    }
    scope.result = a.asReturnedValue();
}

void ArrayPrototype::method_reduce(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, callData->argument(0));
    if (!callback)
        THROW_TYPE_ERROR();

    uint k = 0;
    ScopedValue v(scope);

    if (callData->argc > 1) {
        scope.result = callData->argument(1);
    } else {
        bool kPresent = false;
        while (k < len && !kPresent) {
            v = instance->getIndexed(k, &kPresent);
            if (kPresent)
                scope.result = v;
            ++k;
        }
        if (!kPresent)
            THROW_TYPE_ERROR();
    }

    ScopedCallData cData(scope, 4);
    cData->thisObject = Primitive::undefinedValue();
    cData->args[0] = scope.result;
    cData->args[3] = instance;

    while (k < len) {
        bool kPresent;
        v = instance->getIndexed(k, &kPresent);
        if (kPresent) {
            cData->args[0] = scope.result;
            cData->args[1] = v;
            cData->args[2] = Primitive::fromDouble(k);
            callback->call(scope, cData);
        }
        ++k;
    }
}

void ArrayPrototype::method_reduceRight(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject instance(scope, callData->thisObject.toObject(scope.engine));
    if (!instance)
        RETURN_UNDEFINED();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, callData->argument(0));
    if (!callback)
        THROW_TYPE_ERROR();

    if (len == 0) {
        if (callData->argc == 1)
            THROW_TYPE_ERROR();
        scope.result = callData->argument(1);
        return;
    }

    uint k = len;
    ScopedValue v(scope);
    if (callData->argc > 1) {
        scope.result = callData->argument(1);
    } else {
        bool kPresent = false;
        while (k > 0 && !kPresent) {
            v = instance->getIndexed(k - 1, &kPresent);
            if (kPresent)
                scope.result = v;
            --k;
        }
        if (!kPresent)
            THROW_TYPE_ERROR();
    }

    ScopedCallData cData(scope, 4);
    cData->thisObject = Primitive::undefinedValue();
    cData->args[3] = instance;

    while (k > 0) {
        bool kPresent;
        v = instance->getIndexed(k - 1, &kPresent);
        if (kPresent) {
            cData->args[0] = scope.result;
            cData->args[1] = v;
            cData->args[2] = Primitive::fromDouble(k - 1);
            callback->call(scope, cData);
        }
        --k;
    }
    scope.result = scope.result.asReturnedValue();
}

