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
#include "qv4lookup_p.h"
#include "qv4functionobject_p.h"
#include "qv4scopedvalue_p.h"

QT_BEGIN_NAMESPACE

using namespace QV4;


ReturnedValue Lookup::lookup(ValueRef thisObject, Object *obj, PropertyAttributes *attrs)
{
    int i = 0;
    while (i < Size && obj) {
        classList[i] = obj->internalClass();

        index = obj->internalClass()->find(name);
        if (index != UINT_MAX) {
            level = i;
            *attrs = obj->internalClass()->propertyData.at(index);
            return !attrs->isAccessor() ? obj->memberData()[index].asReturnedValue() : obj->getValue(thisObject, obj->propertyAt(index), *attrs);
        }

        obj = obj->prototype();
        ++i;
    }
    level = Size;

    while (obj) {
        index = obj->internalClass()->find(name);
        if (index != UINT_MAX) {
            *attrs = obj->internalClass()->propertyData.at(index);
            return !attrs->isAccessor() ? obj->memberData()[index].asReturnedValue() : obj->getValue(thisObject, obj->propertyAt(index), *attrs);
        }

        obj = obj->prototype();
    }
    return Primitive::emptyValue().asReturnedValue();
}

ReturnedValue Lookup::lookup(Object *obj, PropertyAttributes *attrs)
{
    Object *thisObject = obj;
    int i = 0;
    while (i < Size && obj) {
        classList[i] = obj->internalClass();

        index = obj->internalClass()->find(name);
        if (index != UINT_MAX) {
            level = i;
            *attrs = obj->internalClass()->propertyData.at(index);
            return !attrs->isAccessor() ? obj->memberData()[index].asReturnedValue() : thisObject->getValue(obj->propertyAt(index), *attrs);
        }

        obj = obj->prototype();
        ++i;
    }
    level = Size;

    while (obj) {
        index = obj->internalClass()->find(name);
        if (index != UINT_MAX) {
            *attrs = obj->internalClass()->propertyData.at(index);
            return !attrs->isAccessor() ? obj->memberData()[index].asReturnedValue() : thisObject->getValue(obj->propertyAt(index), *attrs);
        }

        obj = obj->prototype();
    }
    return Primitive::emptyValue().asReturnedValue();
}

ReturnedValue Lookup::indexedGetterGeneric(Lookup *l, const ValueRef object, const ValueRef index)
{
    if (object->isObject() && index->asArrayIndex() < UINT_MAX) {
        l->indexedGetter = indexedGetterObjectInt;
        return indexedGetterObjectInt(l, object, index);
    }
    return indexedGetterFallback(l, object, index);
}

ReturnedValue Lookup::indexedGetterFallback(Lookup *l, const ValueRef object, const ValueRef index)
{
    Q_UNUSED(l);
    ExecutionContext *ctx = l->engine->currentContext();
    Scope scope(ctx);
    uint idx = index->asArrayIndex();

    Scoped<Object> o(scope, object);
    if (!o) {
        if (idx < UINT_MAX) {
            if (String *str = object->asString()) {
                if (idx >= (uint)str->toQString().length()) {
                    return Encode::undefined();
                }
                const QString s = str->toQString().mid(idx, 1);
                return scope.engine->newString(s)->asReturnedValue();
            }
        }

        if (object->isNullOrUndefined()) {
            QString message = QStringLiteral("Cannot read property '%1' of %2").arg(index->toQStringNoThrow()).arg(object->toQStringNoThrow());
            return ctx->throwTypeError(message);
        }

        o = RuntimeHelpers::convertToObject(ctx, object);
        if (!o) // type error
            return Encode::undefined();
    }

    if (idx < UINT_MAX) {
        if (o->arrayData() && !o->arrayData()->hasAttributes()) {
            ScopedValue v(scope, o->arrayData()->get(idx));
            if (!v->isEmpty())
                return v->asReturnedValue();
        }

        return o->getIndexed(idx);
    }

    ScopedString name(scope, index->toString(ctx));
    if (scope.hasException())
        return Encode::undefined();
    return o->get(name.getPointer());

}


ReturnedValue Lookup::indexedGetterObjectInt(Lookup *l, const ValueRef object, const ValueRef index)
{
    uint idx = index->asArrayIndex();
    if (idx == UINT_MAX || !object->isObject())
        return indexedGetterGeneric(l, object, index);

    Object *o = object->objectValue();
    if (o->arrayData() && o->arrayData()->type() == ArrayData::Simple) {
        SimpleArrayData *s = static_cast<SimpleArrayData *>(o->arrayData());
        if (idx < s->len())
            if (!s->data(idx).isEmpty())
                return s->data(idx).asReturnedValue();
    }

    return indexedGetterFallback(l, object, index);
}

void Lookup::indexedSetterGeneric(Lookup *l, const ValueRef object, const ValueRef index, const ValueRef v)
{
    if (object->isObject()) {
        Object *o = object->objectValue();
        if (o->arrayData() && o->arrayData()->type() == ArrayData::Simple && index->asArrayIndex() < UINT_MAX) {
            l->indexedSetter = indexedSetterObjectInt;
            indexedSetterObjectInt(l, object, index, v);
            return;
        }
    }
    indexedSetterFallback(l, object, index, v);
}

void Lookup::indexedSetterFallback(Lookup *l, const ValueRef object, const ValueRef index, const ValueRef value)
{
    ExecutionContext *ctx = l->engine->currentContext();
    Scope scope(ctx);
    ScopedObject o(scope, object->toObject(ctx));
    if (scope.engine->hasException)
        return;

    uint idx = index->asArrayIndex();
    if (idx < UINT_MAX) {
        if (o->arrayData() && o->arrayData()->type() == ArrayData::Simple) {
            SimpleArrayData *s = static_cast<SimpleArrayData *>(o->arrayData());
            if (idx < s->len() && !s->data(idx).isEmpty()) {
                s->data(idx) = value;
                return;
            }
        }
        o->putIndexed(idx, value);
        return;
    }

    ScopedString name(scope, index->toString(ctx));
    o->put(name.getPointer(), value);
}

void Lookup::indexedSetterObjectInt(Lookup *l, const ValueRef object, const ValueRef index, const ValueRef v)
{
    uint idx = index->asArrayIndex();
    if (idx == UINT_MAX || !object->isObject()) {
        indexedSetterGeneric(l, object, index, v);
        return;
    }

    Object *o = object->objectValue();
    if (o->arrayData() && o->arrayData()->type() == ArrayData::Simple) {
        SimpleArrayData *s = static_cast<SimpleArrayData *>(o->arrayData());
        if (idx < s->len() && !s->data(idx).isEmpty()) {
            s->data(idx) = v;
            return;
        }
    }
    indexedSetterFallback(l, object, index, v);
}

ReturnedValue Lookup::getterGeneric(QV4::Lookup *l, const ValueRef object)
{
    if (Object *o = object->asObject())
        return o->getLookup(l);

    ExecutionEngine *engine = l->name->engine();
    Object *proto;
    switch (object->type()) {
    case Value::Undefined_Type:
    case Value::Null_Type:
        return engine->currentContext()->throwTypeError();
    case Value::Boolean_Type:
        proto = engine->booleanClass->prototype;
        break;
    case Value::Managed_Type:
        Q_ASSERT(object->isString());
        proto = engine->stringObjectClass->prototype;
        if (l->name->equals(engine->id_length.getPointer())) {
            // special case, as the property is on the object itself
            l->getter = stringLengthGetter;
            return stringLengthGetter(l, object);
        }
        break;
    case Value::Integer_Type:
    default: // Number
        proto = engine->numberClass->prototype;
    }

    PropertyAttributes attrs;
    ReturnedValue v = l->lookup(object, proto, &attrs);
    if (v != Primitive::emptyValue().asReturnedValue()) {
        l->type = object->type();
        l->proto = proto;
        if (attrs.isData()) {
            if (l->level == 0)
                l->getter = Lookup::primitiveGetter0;
            else if (l->level == 1)
                l->getter = Lookup::primitiveGetter1;
            return v;
        } else {
            if (l->level == 0)
                l->getter = Lookup::primitiveGetterAccessor0;
            else if (l->level == 1)
                l->getter = Lookup::primitiveGetterAccessor1;
            return v;
        }
    }

    return Encode::undefined();
}

ReturnedValue Lookup::getterTwoClasses(Lookup *l, const ValueRef object)
{
    Lookup l1 = *l;

    if (l1.getter == Lookup::getter0 || l1.getter == Lookup::getter1) {
        if (Object *o = object->asObject()) {
            ReturnedValue v = o->getLookup(l);
            Lookup l2 = *l;

            if (l2.getter == Lookup::getter0 || l2.getter == Lookup::getter1) {
                // if we have a getter0, make sure it comes first
                if (l2.getter == Lookup::getter0)
                    qSwap(l1, l2);

                l->classList[0] = l1.classList[0];
                l->classList[1] = l1.classList[1];
                l->classList[2] = l2.classList[0];
                l->classList[3] = l2.classList[1];
                l->index = l1.index;
                l->index2 = l2.index;

                if (l1.getter == Lookup::getter0) {
                    l->getter = (l2.getter == Lookup::getter0) ? Lookup::getter0getter0 : Lookup::getter0getter1;
                } else {
                    Q_ASSERT(l1.getter == Lookup::getter1 && l2.getter == Lookup::getter1);
                    l->getter = Lookup::getter1getter1;
                }
                return v;
            }
        }
    }

    l->getter = getterFallback;
    return getterFallback(l, object);
}

ReturnedValue Lookup::getterFallback(Lookup *l, const ValueRef object)
{
    QV4::Scope scope(l->name->engine());
    QV4::ScopedObject o(scope, object->toObject(scope.engine->currentContext()));
    if (!o)
        return Encode::undefined();
    QV4::ScopedString s(scope, l->name);
    return o->get(s.getPointer());
}

ReturnedValue Lookup::getter0(Lookup *l, const ValueRef object)
{
    if (object->isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object->objectValue();
        if (l->classList[0] == o->internalClass())
            return o->memberData()[l->index].asReturnedValue();
    }
    return getterTwoClasses(l, object);
}

ReturnedValue Lookup::getter1(Lookup *l, const ValueRef object)
{
    if (object->isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object->objectValue();
        if (l->classList[0] == o->internalClass() &&
            l->classList[1] == o->prototype()->internalClass())
            return o->prototype()->memberData()[l->index].asReturnedValue();
    }
    return getterTwoClasses(l, object);
}

ReturnedValue Lookup::getter2(Lookup *l, const ValueRef object)
{
    if (object->isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object->objectValue();
        if (l->classList[0] == o->internalClass()) {
            o = o->prototype();
            if (l->classList[1] == o->internalClass()) {
                o = o->prototype();
                if (l->classList[2] == o->internalClass())
                    return o->memberData()[l->index].asReturnedValue();
            }
        }
    }
    l->getter = getterFallback;
    return getterFallback(l, object);
}

ReturnedValue Lookup::getter0getter0(Lookup *l, const ValueRef object)
{
    if (object->isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object->objectValue();
        if (l->classList[0] == o->internalClass())
            return o->memberData()[l->index].asReturnedValue();
        if (l->classList[2] == o->internalClass())
            return o->memberData()[l->index2].asReturnedValue();
    }
    l->getter = getterFallback;
    return getterFallback(l, object);
}

ReturnedValue Lookup::getter0getter1(Lookup *l, const ValueRef object)
{
    if (object->isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object->objectValue();
        if (l->classList[0] == o->internalClass())
            return o->memberData()[l->index].asReturnedValue();
        if (l->classList[2] == o->internalClass() &&
            l->classList[3] == o->prototype()->internalClass())
            return o->prototype()->memberData()[l->index2].asReturnedValue();
    }
    l->getter = getterFallback;
    return getterFallback(l, object);
}

ReturnedValue Lookup::getter1getter1(Lookup *l, const ValueRef object)
{
    if (object->isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object->objectValue();
        if (l->classList[0] == o->internalClass() &&
            l->classList[1] == o->prototype()->internalClass())
            return o->prototype()->memberData()[l->index].asReturnedValue();
        if (l->classList[2] == o->internalClass() &&
            l->classList[3] == o->prototype()->internalClass())
            return o->prototype()->memberData()[l->index2].asReturnedValue();
        return getterFallback(l, object);
    }
    l->getter = getterFallback;
    return getterFallback(l, object);
}


ReturnedValue Lookup::getterAccessor0(Lookup *l, const ValueRef object)
{
    if (object->isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object->objectValue();
        if (l->classList[0] == o->internalClass()) {
            Scope scope(o->engine());
            FunctionObject *getter = o->propertyAt(l->index)->getter();
            if (!getter)
                return Encode::undefined();

            ScopedCallData callData(scope, 0);
            callData->thisObject = *object;
            return getter->call(callData);
        }
    }
    l->getter = getterFallback;
    return getterFallback(l, object);
}

ReturnedValue Lookup::getterAccessor1(Lookup *l, const ValueRef object)
{
    if (object->isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object->objectValue();
        if (l->classList[0] == o->internalClass() &&
            l->classList[1] == o->prototype()->internalClass()) {
            Scope scope(o->engine());
            FunctionObject *getter = o->prototype()->propertyAt(l->index)->getter();
            if (!getter)
                return Encode::undefined();

            ScopedCallData callData(scope, 0);
            callData->thisObject = *object;
            return getter->call(callData);
        }
    }
    l->getter = getterFallback;
    return getterFallback(l, object);
}

ReturnedValue Lookup::getterAccessor2(Lookup *l, const ValueRef object)
{
    if (object->isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object->objectValue();
        if (l->classList[0] == o->internalClass()) {
            o = o->prototype();
            if (l->classList[1] == o->internalClass()) {
                o = o->prototype();
                if (l->classList[2] == o->internalClass()) {
                    Scope scope(o->engine());
                    FunctionObject *getter = o->propertyAt(l->index)->getter();
                    if (!getter)
                        return Encode::undefined();

                    ScopedCallData callData(scope, 0);
                    callData->thisObject = *object;
                    return getter->call(callData);
                }
            }
        }
    }
    l->getter = getterFallback;
    return getterFallback(l, object);
}

ReturnedValue Lookup::primitiveGetter0(Lookup *l, const ValueRef object)
{
    if (object->type() == l->type) {
        Object *o = l->proto;
        if (l->classList[0] == o->internalClass())
            return o->memberData()[l->index].asReturnedValue();
    }
    l->getter = getterGeneric;
    return getterGeneric(l, object);
}

ReturnedValue Lookup::primitiveGetter1(Lookup *l, const ValueRef object)
{
    if (object->type() == l->type) {
        Object *o = l->proto;
        if (l->classList[0] == o->internalClass() &&
            l->classList[1] == o->prototype()->internalClass())
            return o->prototype()->memberData()[l->index].asReturnedValue();
    }
    l->getter = getterGeneric;
    return getterGeneric(l, object);
}

ReturnedValue Lookup::primitiveGetterAccessor0(Lookup *l, const ValueRef object)
{
    if (object->type() == l->type) {
        Object *o = l->proto;
        if (l->classList[0] == o->internalClass()) {
            Scope scope(o->engine());
            FunctionObject *getter = o->propertyAt(l->index)->getter();
            if (!getter)
                return Encode::undefined();

            ScopedCallData callData(scope, 0);
            callData->thisObject = *object;
            return getter->call(callData);
        }
    }
    l->getter = getterGeneric;
    return getterGeneric(l, object);
}

ReturnedValue Lookup::primitiveGetterAccessor1(Lookup *l, const ValueRef object)
{
    if (object->type() == l->type) {
        Object *o = l->proto;
        if (l->classList[0] == o->internalClass() &&
            l->classList[1] == o->prototype()->internalClass()) {
            Scope scope(o->engine());
            FunctionObject *getter = o->prototype()->propertyAt(l->index)->getter();
            if (!getter)
                return Encode::undefined();

            ScopedCallData callData(scope, 0);
            callData->thisObject = *object;
            return getter->call(callData);
        }
    }
    l->getter = getterGeneric;
    return getterGeneric(l, object);
}

ReturnedValue Lookup::stringLengthGetter(Lookup *l, const ValueRef object)
{
    if (String *s = object->asString())
        return Encode(s->d()->length());

    l->getter = getterGeneric;
    return getterGeneric(l, object);
}

ReturnedValue Lookup::arrayLengthGetter(Lookup *l, const ValueRef object)
{
    if (ArrayObject *a = object->asArrayObject())
        return a->memberData()[ArrayObject::LengthPropertyIndex].asReturnedValue();

    l->getter = getterGeneric;
    return getterGeneric(l, object);
}


ReturnedValue Lookup::globalGetterGeneric(Lookup *l, ExecutionContext *ctx)
{
    Object *o = ctx->d()->engine->globalObject;
    PropertyAttributes attrs;
    ReturnedValue v = l->lookup(o, &attrs);
    if (v != Primitive::emptyValue().asReturnedValue()) {
        if (attrs.isData()) {
            if (l->level == 0)
                l->globalGetter = globalGetter0;
            else if (l->level == 1)
                l->globalGetter = globalGetter1;
            else if (l->level == 2)
                l->globalGetter = globalGetter2;
            return v;
        } else {
            if (l->level == 0)
                l->globalGetter = globalGetterAccessor0;
            else if (l->level == 1)
                l->globalGetter = globalGetterAccessor1;
            else if (l->level == 2)
                l->globalGetter = globalGetterAccessor2;
            return v;
        }
    }
    Scope scope(ctx);
    Scoped<String> n(scope, l->name);
    return ctx->throwReferenceError(n);
}

ReturnedValue Lookup::globalGetter0(Lookup *l, ExecutionContext *ctx)
{
    Object *o = ctx->d()->engine->globalObject;
    if (l->classList[0] == o->internalClass())
        return o->memberData()[l->index].asReturnedValue();

    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, ctx);
}

ReturnedValue Lookup::globalGetter1(Lookup *l, ExecutionContext *ctx)
{
    Object *o = ctx->d()->engine->globalObject;
    if (l->classList[0] == o->internalClass() &&
        l->classList[1] == o->prototype()->internalClass())
        return o->prototype()->memberData()[l->index].asReturnedValue();

    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, ctx);
}

ReturnedValue Lookup::globalGetter2(Lookup *l, ExecutionContext *ctx)
{
    Object *o = ctx->d()->engine->globalObject;
    if (l->classList[0] == o->internalClass()) {
        o = o->prototype();
        if (l->classList[1] == o->internalClass()) {
            o = o->prototype();
            if (l->classList[2] == o->internalClass()) {
                return o->prototype()->memberData()[l->index].asReturnedValue();
            }
        }
    }
    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, ctx);
}

ReturnedValue Lookup::globalGetterAccessor0(Lookup *l, ExecutionContext *ctx)
{
    Object *o = ctx->d()->engine->globalObject;
    if (l->classList[0] == o->internalClass()) {
        Scope scope(o->engine());
        FunctionObject *getter = o->propertyAt(l->index)->getter();
        if (!getter)
            return Encode::undefined();

        ScopedCallData callData(scope, 0);
        callData->thisObject = Primitive::undefinedValue();
        return getter->call(callData);
    }
    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, ctx);
}

ReturnedValue Lookup::globalGetterAccessor1(Lookup *l, ExecutionContext *ctx)
{
    Object *o = ctx->d()->engine->globalObject;
    if (l->classList[0] == o->internalClass() &&
        l->classList[1] == o->prototype()->internalClass()) {
        Scope scope(o->engine());
        FunctionObject *getter = o->prototype()->propertyAt(l->index)->getter();
        if (!getter)
            return Encode::undefined();

        ScopedCallData callData(scope, 0);
        callData->thisObject = Primitive::undefinedValue();
        return getter->call(callData);
    }
    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, ctx);
}

ReturnedValue Lookup::globalGetterAccessor2(Lookup *l, ExecutionContext *ctx)
{
    Object *o = ctx->d()->engine->globalObject;
    if (l->classList[0] == o->internalClass()) {
        o = o->prototype();
        if (l->classList[1] == o->internalClass()) {
            o = o->prototype();
            if (l->classList[2] == o->internalClass()) {
                Scope scope(o->engine());
                FunctionObject *getter = o->propertyAt(l->index)->getter();
                if (!getter)
                    return Encode::undefined();

                ScopedCallData callData(scope, 0);
                callData->thisObject = Primitive::undefinedValue();
                return getter->call(callData);
            }
        }
    }
    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, ctx);
}

void Lookup::setterGeneric(Lookup *l, const ValueRef object, const ValueRef value)
{
    Scope scope(l->name->engine());
    ScopedObject o(scope, object);
    if (!o) {
        o = RuntimeHelpers::convertToObject(scope.engine->currentContext(), object);
        if (!o) // type error
            return;
        ScopedString s(scope, l->name);
        o->put(s.getPointer(), value);
        return;
    }
    o->setLookup(l, value);
}

void Lookup::setterTwoClasses(Lookup *l, const ValueRef object, const ValueRef value)
{
    Lookup l1 = *l;

    if (Object *o = object->asObject()) {
        o->setLookup(l, value);

        if (l->setter == Lookup::setter0) {
            l->setter = setter0setter0;
            l->classList[1] = l1.classList[0];
            l->index2 = l1.index;
            return;
        }
    }

    l->setter = setterFallback;
    setterFallback(l, object, value);
}

void Lookup::setterFallback(Lookup *l, const ValueRef object, const ValueRef value)
{
    QV4::Scope scope(l->name->engine());
    QV4::ScopedObject o(scope, object->toObject(scope.engine->currentContext()));
    if (o) {
        QV4::ScopedString s(scope, l->name);
        o->put(s.getPointer(), value);
    }
}

void Lookup::setter0(Lookup *l, const ValueRef object, const ValueRef value)
{
    Object *o = static_cast<Object *>(object->asManaged());
    if (o && o->internalClass() == l->classList[0]) {
        o->memberData()[l->index] = *value;
        return;
    }

    setterTwoClasses(l, object, value);
}

void Lookup::setterInsert0(Lookup *l, const ValueRef object, const ValueRef value)
{
    Object *o = static_cast<Object *>(object->asManaged());
    if (o && o->internalClass() == l->classList[0]) {
        if (!o->prototype()) {
            if (l->index >= o->memberData().size())
                o->ensureMemberIndex(l->index);
            o->memberData()[l->index] = *value;
            o->setInternalClass(l->classList[3]);
            return;
        }
    }

    l->setter = setterFallback;
    setterFallback(l, object, value);
}

void Lookup::setterInsert1(Lookup *l, const ValueRef object, const ValueRef value)
{
    Object *o = static_cast<Object *>(object->asManaged());
    if (o && o->internalClass() == l->classList[0]) {
        Object *p = o->prototype();
        if (p && p->internalClass() == l->classList[1]) {
            if (l->index >= o->memberData().size())
                o->ensureMemberIndex(l->index);
            o->memberData()[l->index] = *value;
            o->setInternalClass(l->classList[3]);
            return;
        }
    }

    l->setter = setterFallback;
    setterFallback(l, object, value);
}

void Lookup::setterInsert2(Lookup *l, const ValueRef object, const ValueRef value)
{
    Object *o = static_cast<Object *>(object->asManaged());
    if (o && o->internalClass() == l->classList[0]) {
        Object *p = o->prototype();
        if (p && p->internalClass() == l->classList[1]) {
            p = p->prototype();
            if (p && p->internalClass() == l->classList[2]) {
                if (l->index >= o->memberData().size())
                    o->ensureMemberIndex(l->index);
                o->memberData()[l->index] = *value;
                o->setInternalClass(l->classList[3]);
                return;
            }
        }
    }

    l->setter = setterFallback;
    setterFallback(l, object, value);
}

void Lookup::setter0setter0(Lookup *l, const ValueRef object, const ValueRef value)
{
    Object *o = static_cast<Object *>(object->asManaged());
    if (o) {
        if (o->internalClass() == l->classList[0]) {
            o->memberData()[l->index] = *value;
            return;
        }
        if (o->internalClass() == l->classList[1]) {
            o->memberData()[l->index2] = *value;
            return;
        }
    }

    l->setter = setterFallback;
    setterFallback(l, object, value);

}

QT_END_NAMESPACE
