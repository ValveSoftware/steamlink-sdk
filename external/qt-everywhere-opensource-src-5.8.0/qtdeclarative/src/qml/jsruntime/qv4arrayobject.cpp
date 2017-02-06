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

ReturnedValue ArrayPrototype::method_isArray(CallContext *ctx)
{
    bool isArray = ctx->argc() && ctx->args()[0].as<ArrayObject>();
    return Encode(isArray);
}

ReturnedValue ArrayPrototype::method_toString(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->thisObject(), ScopedObject::Convert);
    if (ctx->d()->engine->hasException)
        return Encode::undefined();
    ScopedString s(scope, ctx->d()->engine->newString(QStringLiteral("join")));
    ScopedFunctionObject f(scope, o->get(s));
    if (!!f) {
        ScopedCallData d(scope, 0);
        d->thisObject = ctx->thisObject();
        f->call(scope, d);
        return scope.result.asReturnedValue();
    }
    return ObjectPrototype::method_toString(ctx);
}

ReturnedValue ArrayPrototype::method_toLocaleString(CallContext *ctx)
{
    return method_toString(ctx);
}

ReturnedValue ArrayPrototype::method_concat(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject result(scope, ctx->d()->engine->newArrayObject());

    ScopedObject thisObject(scope, ctx->thisObject().toObject(scope.engine));
    if (!thisObject)
        return Encode::undefined();
    if (thisObject->isArrayObject()) {
        result->copyArrayData(thisObject);
    } else {
        result->arraySet(0, thisObject);
    }

    ScopedArrayObject elt(scope);
    ScopedObject eltAsObj(scope);
    ScopedValue entry(scope);
    for (int i = 0; i < ctx->argc(); ++i) {
        eltAsObj = ctx->args()[i];
        elt = ctx->args()[i];
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
            result->arraySet(result->getLength(), ctx->args()[i]);
        }
    }

    return result.asReturnedValue();
}

ReturnedValue ArrayPrototype::method_join(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue arg(scope, ctx->argument(0));
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));

    if (!instance)
        return ctx->d()->engine->newString()->asReturnedValue();

    QString r4;
    if (arg->isUndefined())
        r4 = QStringLiteral(",");
    else
        r4 = arg->toQString();

    ScopedValue length(scope, instance->get(ctx->d()->engine->id_length()));
    const quint32 r2 = length->isUndefined() ? 0 : length->toUInt32();

    if (!r2)
        return ctx->d()->engine->newString()->asReturnedValue();

    QString R;

    // ### FIXME
    if (ArrayObject *a = instance->as<ArrayObject>()) {
        ScopedValue e(scope);
        for (uint i = 0; i < a->getLength(); ++i) {
            if (i)
                R += r4;

            e = a->getIndexed(i);
            if (scope.hasException())
                return Encode::undefined();
            if (!e->isNullOrUndefined())
                R += e->toQString();
        }
    } else {
        //
        // crazy!
        //
        ScopedString name(scope, ctx->d()->engine->newString(QStringLiteral("0")));
        ScopedValue r6(scope, instance->get(name));
        if (!r6->isNullOrUndefined())
            R = r6->toQString();

        ScopedValue r12(scope);
        for (quint32 k = 1; k < r2; ++k) {
            R += r4;

            name = Primitive::fromDouble(k).toString(scope.engine);
            r12 = instance->get(name);
            if (scope.hasException())
                return Encode::undefined();

            if (!r12->isNullOrUndefined())
                R += r12->toQString();
        }
    }

    return ctx->d()->engine->newString(R)->asReturnedValue();
}

ReturnedValue ArrayPrototype::method_pop(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();
    uint len = instance->getLength();

    if (!len) {
        if (!instance->isArrayObject())
            instance->put(ctx->d()->engine->id_length(), ScopedValue(scope, Primitive::fromInt32(0)));
        return Encode::undefined();
    }

    ScopedValue result(scope, instance->getIndexed(len - 1));
    if (scope.hasException())
        return Encode::undefined();

    instance->deleteIndexedProperty(len - 1);
    if (scope.hasException())
        return Encode::undefined();
    if (instance->isArrayObject())
        instance->setArrayLength(len - 1);
    else
        instance->put(ctx->d()->engine->id_length(), ScopedValue(scope, Primitive::fromDouble(len - 1)));
    return result->asReturnedValue();
}

ReturnedValue ArrayPrototype::method_push(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    instance->arrayCreate();
    Q_ASSERT(instance->arrayData());

    uint len = instance->getLength();

    if (len + ctx->argc() < len) {
        // ughh...
        double l = len;
        ScopedString s(scope);
        for (int i = 0; i < ctx->argc(); ++i) {
            s = Primitive::fromDouble(l + i).toString(scope.engine);
            instance->put(s, ctx->args()[i]);
        }
        double newLen = l + ctx->argc();
        if (!instance->isArrayObject())
            instance->put(ctx->d()->engine->id_length(), ScopedValue(scope, Primitive::fromDouble(newLen)));
        else {
            ScopedString str(scope, ctx->d()->engine->newString(QStringLiteral("Array.prototype.push: Overflow")));
            return ctx->engine()->throwRangeError(str);
        }
        return Encode(newLen);
    }

    if (!ctx->argc())
        ;
    else if (!instance->protoHasArray() && instance->arrayData()->length() <= len && instance->arrayData()->type == Heap::ArrayData::Simple) {
        instance->arrayData()->vtable()->putArray(instance, len, ctx->args(), ctx->argc());
        len = instance->arrayData()->length();
    } else {
        for (int i = 0; i < ctx->argc(); ++i)
            instance->putIndexed(len + i, ctx->args()[i]);
        len += ctx->argc();
    }
    if (instance->isArrayObject())
        instance->setArrayLengthUnchecked(len);
    else
        instance->put(ctx->d()->engine->id_length(), ScopedValue(scope, Primitive::fromDouble(len)));

    return Encode(len);
}

ReturnedValue ArrayPrototype::method_reverse(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();
    uint length = instance->getLength();

    int lo = 0, hi = length - 1;

    ScopedValue lval(scope);
    ScopedValue hval(scope);
    for (; lo < hi; ++lo, --hi) {
        bool loExists, hiExists;
        lval = instance->getIndexed(lo, &loExists);
        hval = instance->getIndexed(hi, &hiExists);
        if (scope.hasException())
            return Encode::undefined();
        if (hiExists)
            instance->putIndexed(lo, hval);
        else
            instance->deleteIndexedProperty(lo);
        if (scope.hasException())
            return Encode::undefined();
        if (loExists)
            instance->putIndexed(hi, lval);
        else
            instance->deleteIndexedProperty(hi);
    }
    return instance.asReturnedValue();
}

ReturnedValue ArrayPrototype::method_shift(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    instance->arrayCreate();
    Q_ASSERT(instance->arrayData());

    uint len = instance->getLength();

    if (!len) {
        if (!instance->isArrayObject())
            instance->put(ctx->d()->engine->id_length(), ScopedValue(scope, Primitive::fromInt32(0)));
        return Encode::undefined();
    }

    ScopedValue result(scope);

    if (!instance->protoHasArray() && !instance->arrayData()->attrs && instance->arrayData()->length() <= len && instance->arrayData()->type != Heap::ArrayData::Custom) {
        result = instance->arrayData()->vtable()->pop_front(instance);
    } else {
        result = instance->getIndexed(0);
        if (scope.hasException())
            return Encode::undefined();
        ScopedValue v(scope);
        // do it the slow way
        for (uint k = 1; k < len; ++k) {
            bool exists;
            v = instance->getIndexed(k, &exists);
            if (scope.hasException())
                return Encode::undefined();
            if (exists)
                instance->putIndexed(k - 1, v);
            else
                instance->deleteIndexedProperty(k - 1);
            if (scope.hasException())
                return Encode::undefined();
        }
        instance->deleteIndexedProperty(len - 1);
        if (scope.hasException())
            return Encode::undefined();
    }

    if (instance->isArrayObject())
        instance->setArrayLengthUnchecked(len - 1);
    else
        instance->put(ctx->d()->engine->id_length(), ScopedValue(scope, Primitive::fromDouble(len - 1)));
    return result->asReturnedValue();
}

ReturnedValue ArrayPrototype::method_slice(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->thisObject().toObject(scope.engine));
    if (!o)
        return Encode::undefined();

    ScopedArrayObject result(scope, ctx->d()->engine->newArrayObject());
    uint len = o->getLength();
    double s = ScopedValue(scope, ctx->argument(0))->toInteger();
    uint start;
    if (s < 0)
        start = (uint)qMax(len + s, 0.);
    else if (s > len)
        start = len;
    else
        start = (uint) s;
    uint end = len;
    if (ctx->argc() > 1 && !ctx->args()[1].isUndefined()) {
        double e = ctx->args()[1].toInteger();
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
        if (scope.hasException())
            return Encode::undefined();
        if (exists)
            result->arraySet(n, v);
        ++n;
    }
    return result.asReturnedValue();
}

ReturnedValue ArrayPrototype::method_sort(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    uint len = instance->getLength();

    ScopedValue comparefn(scope, ctx->argument(0));
    ArrayData::sort(scope.engine, instance, comparefn, len);
    return ctx->thisObject().asReturnedValue();
}

ReturnedValue ArrayPrototype::method_splice(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();
    uint len = instance->getLength();

    ScopedArrayObject newArray(scope, ctx->d()->engine->newArrayObject());

    double rs = ScopedValue(scope, ctx->argument(0))->toInteger();
    uint start;
    if (rs < 0)
        start = (uint) qMax(0., len + rs);
    else
        start = (uint) qMin(rs, (double)len);

    uint deleteCount = (uint)qMin(qMax(ScopedValue(scope, ctx->argument(1))->toInteger(), 0.), (double)(len - start));

    newArray->arrayReserve(deleteCount);
    ScopedValue v(scope);
    for (uint i = 0; i < deleteCount; ++i) {
        bool exists;
        v = instance->getIndexed(start + i, &exists);
        if (scope.hasException())
            return Encode::undefined();
        if (exists)
            newArray->arrayPut(i, v);
    }
    newArray->setArrayLengthUnchecked(deleteCount);

    uint itemCount = ctx->argc() < 2 ? 0 : ctx->argc() - 2;

    if (itemCount < deleteCount) {
        for (uint k = start; k < len - deleteCount; ++k) {
            bool exists;
            v = instance->getIndexed(k + deleteCount, &exists);
            if (scope.hasException())
                return Encode::undefined();
            if (exists)
                instance->putIndexed(k + itemCount, v);
            else
                instance->deleteIndexedProperty(k + itemCount);
            if (scope.hasException())
                return Encode::undefined();
        }
        for (uint k = len; k > len - deleteCount + itemCount; --k) {
            instance->deleteIndexedProperty(k - 1);
            if (scope.hasException())
                return Encode::undefined();
        }
    } else if (itemCount > deleteCount) {
        uint k = len - deleteCount;
        while (k > start) {
            bool exists;
            v = instance->getIndexed(k + deleteCount - 1, &exists);
            if (scope.hasException())
                return Encode::undefined();
            if (exists)
                instance->putIndexed(k + itemCount - 1, v);
            else
                instance->deleteIndexedProperty(k + itemCount - 1);
            if (scope.hasException())
                return Encode::undefined();
            --k;
        }
    }

    for (uint i = 0; i < itemCount; ++i) {
        instance->putIndexed(start + i, ctx->args()[i + 2]);
        if (scope.hasException())
            return Encode::undefined();
    }

    ctx->d()->strictMode = true;
    instance->put(ctx->d()->engine->id_length(), ScopedValue(scope, Primitive::fromDouble(len - deleteCount + itemCount)));

    return newArray.asReturnedValue();
}

ReturnedValue ArrayPrototype::method_unshift(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    instance->arrayCreate();
    Q_ASSERT(instance->arrayData());

    uint len = instance->getLength();

    if (!instance->protoHasArray() && !instance->arrayData()->attrs && instance->arrayData()->length() <= len &&
        instance->arrayData()->type != Heap::ArrayData::Custom) {
        instance->arrayData()->vtable()->push_front(instance, ctx->args(), ctx->argc());
    } else {
        ScopedValue v(scope);
        for (uint k = len; k > 0; --k) {
            bool exists;
            v = instance->getIndexed(k - 1, &exists);
            if (exists)
                instance->putIndexed(k + ctx->argc() - 1, v);
            else
                instance->deleteIndexedProperty(k + ctx->argc() - 1);
        }
        for (int i = 0; i < ctx->argc(); ++i)
            instance->putIndexed(i, ctx->args()[i]);
    }

    uint newLen = len + ctx->argc();
    if (instance->isArrayObject())
        instance->setArrayLengthUnchecked(newLen);
    else
        instance->put(ctx->d()->engine->id_length(), ScopedValue(scope, Primitive::fromDouble(newLen)));

    return Encode(newLen);
}

ReturnedValue ArrayPrototype::method_indexOf(CallContext *ctx)
{
    Scope scope(ctx);

    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();
    uint len = instance->getLength();
    if (!len)
        return Encode(-1);

    ScopedValue searchValue(scope, ctx->argument(0));
    uint fromIndex = 0;

    if (ctx->argc() >= 2) {
        double f = ctx->args()[1].toInteger();
        if (scope.hasException())
            return Encode::undefined();
        if (f >= len)
            return Encode(-1);
        if (f < 0)
            f = qMax(len + f, 0.);
        fromIndex = (uint) f;
    }

    if (instance->isStringObject()) {
        ScopedValue v(scope);
        for (uint k = fromIndex; k < len; ++k) {
            bool exists;
            v = instance->getIndexed(k, &exists);
            if (exists && RuntimeHelpers::strictEqual(v, searchValue))
                return Encode(k);
        }
        return Encode(-1);
    }

    ScopedValue value(scope);

    if (ArgumentsObject::isNonStrictArgumentsObject(instance) ||
        (instance->arrayType() >= Heap::ArrayData::Sparse) || instance->protoHasArray()) {
        // lets be safe and slow
        for (uint i = fromIndex; i < len; ++i) {
            bool exists;
            value = instance->getIndexed(i, &exists);
            if (scope.hasException())
                return Encode::undefined();
            if (exists && RuntimeHelpers::strictEqual(value, searchValue))
                return Encode(i);
        }
    } else if (!instance->arrayData()) {
        return Encode(-1);
    } else {
        Q_ASSERT(instance->arrayType() == Heap::ArrayData::Simple || instance->arrayType() == Heap::ArrayData::Complex);
        Heap::SimpleArrayData *sa = instance->d()->arrayData.cast<Heap::SimpleArrayData>();
        if (len > sa->len)
            len = sa->len;
        uint idx = fromIndex;
        while (idx < len) {
            value = sa->data(idx);
            if (scope.hasException())
                return Encode::undefined();
            if (RuntimeHelpers::strictEqual(value, searchValue))
                return Encode(idx);
            ++idx;
        }
    }
    return Encode(-1);
}

ReturnedValue ArrayPrototype::method_lastIndexOf(CallContext *ctx)
{
    Scope scope(ctx);

    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();
    uint len = instance->getLength();
    if (!len)
        return Encode(-1);

    ScopedValue searchValue(scope);
    uint fromIndex = len;

    if (ctx->argc() >= 1)
        searchValue = ctx->argument(0);
    else
        searchValue = Primitive::undefinedValue();

    if (ctx->argc() >= 2) {
        double f = ctx->args()[1].toInteger();
        if (scope.hasException())
            return Encode::undefined();
        if (f > 0)
            f = qMin(f, (double)(len - 1));
        else if (f < 0) {
            f = len + f;
            if (f < 0)
                return Encode(-1);
        }
        fromIndex = (uint) f + 1;
    }

    ScopedValue v(scope);
    for (uint k = fromIndex; k > 0;) {
        --k;
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (scope.hasException())
            return Encode::undefined();
        if (exists && RuntimeHelpers::strictEqual(v, searchValue))
            return Encode(k);
    }
    return Encode(-1);
}

ReturnedValue ArrayPrototype::method_every(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, ctx->argument(0));
    if (!callback)
        return ctx->engine()->throwTypeError();

    ScopedCallData callData(scope, 3);
    callData->args[2] = instance;
    callData->thisObject = ctx->argument(1);
    ScopedValue v(scope);

    bool ok = true;
    for (uint k = 0; ok && k < len; ++k) {
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (!exists)
            continue;

        callData->args[0] = v;
        callData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, callData);
        ok = scope.result.toBoolean();
    }
    return Encode(ok);
}

ReturnedValue ArrayPrototype::method_some(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, ctx->argument(0));
    if (!callback)
        return ctx->engine()->throwTypeError();

    ScopedCallData callData(scope, 3);
    callData->thisObject = ctx->argument(1);
    callData->args[2] = instance;
    ScopedValue v(scope);

    for (uint k = 0; k < len; ++k) {
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (!exists)
            continue;

        callData->args[0] = v;
        callData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, callData);
        if (scope.result.toBoolean())
            return Encode(true);
    }
    return Encode(false);
}

ReturnedValue ArrayPrototype::method_forEach(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, ctx->argument(0));
    if (!callback)
        return ctx->engine()->throwTypeError();

    ScopedCallData callData(scope, 3);
    callData->thisObject = ctx->argument(1);
    callData->args[2] = instance;

    ScopedValue v(scope);
    for (uint k = 0; k < len; ++k) {
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (!exists)
            continue;

        callData->args[0] = v;
        callData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, callData);
    }
    return Encode::undefined();
}

ReturnedValue ArrayPrototype::method_map(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, ctx->argument(0));
    if (!callback)
        return ctx->engine()->throwTypeError();

    ScopedArrayObject a(scope, ctx->d()->engine->newArrayObject());
    a->arrayReserve(len);
    a->setArrayLengthUnchecked(len);

    ScopedCallData callData(scope, 3);
    callData->thisObject = ctx->argument(1);
    callData->args[2] = instance;

    ScopedValue v(scope);
    for (uint k = 0; k < len; ++k) {
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (!exists)
            continue;

        callData->args[0] = v;
        callData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, callData);
        a->arraySet(k, scope.result);
    }
    return a.asReturnedValue();
}

ReturnedValue ArrayPrototype::method_filter(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, ctx->argument(0));
    if (!callback)
        return ctx->engine()->throwTypeError();

    ScopedArrayObject a(scope, ctx->d()->engine->newArrayObject());
    a->arrayReserve(len);

    ScopedCallData callData(scope, 3);
    callData->thisObject = ctx->argument(1);
    callData->args[2] = instance;

    ScopedValue v(scope);

    uint to = 0;
    for (uint k = 0; k < len; ++k) {
        bool exists;
        v = instance->getIndexed(k, &exists);
        if (!exists)
            continue;

        callData->args[0] = v;
        callData->args[1] = Primitive::fromDouble(k);
        callback->call(scope, callData);
        if (scope.result.toBoolean()) {
            a->arraySet(to, v);
            ++to;
        }
    }
    return a.asReturnedValue();
}

ReturnedValue ArrayPrototype::method_reduce(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, ctx->argument(0));
    if (!callback)
        return ctx->engine()->throwTypeError();

    uint k = 0;
    ScopedValue v(scope);

    if (ctx->argc() > 1) {
        scope.result = ctx->argument(1);
    } else {
        bool kPresent = false;
        while (k < len && !kPresent) {
            v = instance->getIndexed(k, &kPresent);
            if (kPresent)
                scope.result = v;
            ++k;
        }
        if (!kPresent)
            return ctx->engine()->throwTypeError();
    }

    ScopedCallData callData(scope, 4);
    callData->thisObject = Primitive::undefinedValue();
    callData->args[0] = scope.result;
    callData->args[3] = instance;

    while (k < len) {
        bool kPresent;
        v = instance->getIndexed(k, &kPresent);
        if (kPresent) {
            callData->args[0] = scope.result;
            callData->args[1] = v;
            callData->args[2] = Primitive::fromDouble(k);
            callback->call(scope, callData);
        }
        ++k;
    }
    return scope.result.asReturnedValue();
}

ReturnedValue ArrayPrototype::method_reduceRight(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject instance(scope, ctx->thisObject().toObject(scope.engine));
    if (!instance)
        return Encode::undefined();

    uint len = instance->getLength();

    ScopedFunctionObject callback(scope, ctx->argument(0));
    if (!callback)
        return ctx->engine()->throwTypeError();

    if (len == 0) {
        if (ctx->argc() == 1)
            return ctx->engine()->throwTypeError();
        return ctx->argument(1);
    }

    uint k = len;
    ScopedValue v(scope);
    if (ctx->argc() > 1) {
        scope.result = ctx->argument(1);
    } else {
        bool kPresent = false;
        while (k > 0 && !kPresent) {
            v = instance->getIndexed(k - 1, &kPresent);
            if (kPresent)
                scope.result = v;
            --k;
        }
        if (!kPresent)
            return ctx->engine()->throwTypeError();
    }

    ScopedCallData callData(scope, 4);
    callData->thisObject = Primitive::undefinedValue();
    callData->args[3] = instance;

    while (k > 0) {
        bool kPresent;
        v = instance->getIndexed(k - 1, &kPresent);
        if (kPresent) {
            callData->args[0] = scope.result;
            callData->args[1] = v;
            callData->args[2] = Primitive::fromDouble(k - 1);
            callback->call(scope, callData);
        }
        --k;
    }
    return scope.result.asReturnedValue();
}

