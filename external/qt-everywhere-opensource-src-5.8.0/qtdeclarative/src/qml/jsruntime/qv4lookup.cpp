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
#include "qv4lookup_p.h"
#include "qv4functionobject_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4string_p.h"

QT_BEGIN_NAMESPACE

using namespace QV4;


ReturnedValue Lookup::lookup(const Value &thisObject, Object *o, PropertyAttributes *attrs)
{
    ExecutionEngine *engine = o->engine();
    Identifier *name = engine->current->compilationUnit->runtimeStrings[nameIndex]->identifier;
    int i = 0;
    Heap::Object *obj = o->d();
    while (i < Size && obj) {
        classList[i] = obj->internalClass;

        index = obj->internalClass->find(name);
        if (index != UINT_MAX) {
            level = i;
            *attrs = obj->internalClass->propertyData.at(index);
            Value *v = obj->propertyData(index);
            return !attrs->isAccessor() ? v->asReturnedValue() : Object::getValue(thisObject, *v, *attrs);
        }

        obj = obj->prototype;
        ++i;
    }
    level = Size;

    while (obj) {
        index = obj->internalClass->find(name);
        if (index != UINT_MAX) {
            *attrs = obj->internalClass->propertyData.at(index);
            Value *v = obj->propertyData(index);
            return !attrs->isAccessor() ? v->asReturnedValue() : Object::getValue(thisObject, *v, *attrs);
        }

        obj = obj->prototype;
    }
    return Primitive::emptyValue().asReturnedValue();
}

ReturnedValue Lookup::lookup(const Object *thisObject, PropertyAttributes *attrs)
{
    Heap::Object *obj = thisObject->d();
    ExecutionEngine *engine = thisObject->engine();
    Identifier *name = engine->current->compilationUnit->runtimeStrings[nameIndex]->identifier;
    int i = 0;
    while (i < Size && obj) {
        classList[i] = obj->internalClass;

        index = obj->internalClass->find(name);
        if (index != UINT_MAX) {
            level = i;
            *attrs = obj->internalClass->propertyData.at(index);
            Value *v = obj->propertyData(index);
            return !attrs->isAccessor() ? v->asReturnedValue() : thisObject->getValue(*v, *attrs);
        }

        obj = obj->prototype;
        ++i;
    }
    level = Size;

    while (obj) {
        index = obj->internalClass->find(name);
        if (index != UINT_MAX) {
            *attrs = obj->internalClass->propertyData.at(index);
            Value *v = obj->propertyData(index);
            return !attrs->isAccessor() ? v->asReturnedValue() : thisObject->getValue(*v, *attrs);
        }

        obj = obj->prototype;
    }
    return Primitive::emptyValue().asReturnedValue();
}

ReturnedValue Lookup::indexedGetterGeneric(Lookup *l, const Value &object, const Value &index)
{
    if (object.isObject() && index.asArrayIndex() < UINT_MAX) {
        l->indexedGetter = indexedGetterObjectInt;
        return indexedGetterObjectInt(l, object, index);
    }
    return indexedGetterFallback(l, object, index);
}

ReturnedValue Lookup::indexedGetterFallback(Lookup *l, const Value &object, const Value &index)
{
    Q_UNUSED(l);
    Scope scope(l->engine);
    uint idx = index.asArrayIndex();

    ScopedObject o(scope, object);
    if (!o) {
        if (idx < UINT_MAX) {
            if (const String *str = object.as<String>()) {
                if (idx >= (uint)str->toQString().length()) {
                    return Encode::undefined();
                }
                const QString s = str->toQString().mid(idx, 1);
                return scope.engine->newString(s)->asReturnedValue();
            }
        }

        if (object.isNullOrUndefined()) {
            QString message = QStringLiteral("Cannot read property '%1' of %2").arg(index.toQStringNoThrow()).arg(object.toQStringNoThrow());
            return l->engine->throwTypeError(message);
        }

        o = RuntimeHelpers::convertToObject(scope.engine, object);
        if (!o) // type error
            return Encode::undefined();
    }

    if (idx < UINT_MAX) {
        if (o->d()->arrayData && !o->d()->arrayData->attrs) {
            ScopedValue v(scope, Scoped<ArrayData>(scope, o->arrayData())->get(idx));
            if (!v->isEmpty())
                return v->asReturnedValue();
        }

        return o->getIndexed(idx);
    }

    ScopedString name(scope, index.toString(scope.engine));
    if (scope.hasException())
        return Encode::undefined();
    return o->get(name);

}


ReturnedValue Lookup::indexedGetterObjectInt(Lookup *l, const Value &object, const Value &index)
{
    uint idx = index.asArrayIndex();
    if (idx == UINT_MAX || !object.isObject())
        return indexedGetterFallback(l, object, index);

    Object *o = object.objectValue();
    if (o->d()->arrayData && o->d()->arrayData->type == Heap::ArrayData::Simple) {
        Heap::SimpleArrayData *s = o->d()->arrayData.cast<Heap::SimpleArrayData>();
        if (idx < s->len)
            if (!s->data(idx).isEmpty())
                return s->data(idx).asReturnedValue();
    }

    return indexedGetterFallback(l, object, index);
}

void Lookup::indexedSetterGeneric(Lookup *l, const Value &object, const Value &index, const Value &v)
{
    if (object.isObject()) {
        Object *o = object.objectValue();
        if (o->d()->arrayData && o->d()->arrayData->type == Heap::ArrayData::Simple && index.asArrayIndex() < UINT_MAX) {
            l->indexedSetter = indexedSetterObjectInt;
            indexedSetterObjectInt(l, object, index, v);
            return;
        }
    }
    indexedSetterFallback(l, object, index, v);
}

void Lookup::indexedSetterFallback(Lookup *l, const Value &object, const Value &index, const Value &value)
{
    Scope scope(l->engine);
    ScopedObject o(scope, object.toObject(scope.engine));
    if (scope.engine->hasException)
        return;

    uint idx = index.asArrayIndex();
    if (idx < UINT_MAX) {
        if (o->d()->arrayData && o->d()->arrayData->type == Heap::ArrayData::Simple) {
            Heap::SimpleArrayData *s = o->d()->arrayData.cast<Heap::SimpleArrayData>();
            if (idx < s->len) {
                s->data(idx) = value;
                return;
            }
        }
        o->putIndexed(idx, value);
        return;
    }

    ScopedString name(scope, index.toString(scope.engine));
    o->put(name, value);
}

void Lookup::indexedSetterObjectInt(Lookup *l, const Value &object, const Value &index, const Value &v)
{
    uint idx = index.asArrayIndex();
    if (idx == UINT_MAX || !object.isObject()) {
        indexedSetterGeneric(l, object, index, v);
        return;
    }

    Object *o = object.objectValue();
    if (o->d()->arrayData && o->d()->arrayData->type == Heap::ArrayData::Simple) {
        Heap::SimpleArrayData *s = o->d()->arrayData.cast<Heap::SimpleArrayData>();
        if (idx < s->len) {
            s->data(idx) = v;
            return;
        }
    }
    indexedSetterFallback(l, object, index, v);
}

ReturnedValue Lookup::getterGeneric(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (const Object *o = object.as<Object>())
        return o->getLookup(l);

    Object *proto;
    switch (object.type()) {
    case Value::Undefined_Type:
    case Value::Null_Type:
        return engine->throwTypeError();
    case Value::Boolean_Type:
        proto = engine->booleanPrototype();
        break;
    case Value::Managed_Type: {
        Q_ASSERT(object.isString());
        proto = engine->stringPrototype();
        Scope scope(engine);
        ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[l->nameIndex]);
        if (name->equals(engine->id_length())) {
            // special case, as the property is on the object itself
            l->getter = stringLengthGetter;
            return stringLengthGetter(l, engine, object);
        }
        break;
    }
    case Value::Integer_Type:
    default: // Number
        proto = engine->numberPrototype();
    }

    PropertyAttributes attrs;
    ReturnedValue v = l->lookup(object, proto, &attrs);
    if (v != Primitive::emptyValue().asReturnedValue()) {
        l->type = object.type();
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

ReturnedValue Lookup::getterTwoClasses(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    Lookup l1 = *l;

    if (l1.getter == Lookup::getter0 || l1.getter == Lookup::getter1) {
        if (const Object *o = object.as<Object>()) {
            ReturnedValue v = o->getLookup(l);
            Lookup l2 = *l;

            if (l->index != UINT_MAX && (l2.getter == Lookup::getter0 || l2.getter == Lookup::getter1)) {
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
    return getterFallback(l, engine, object);
}

ReturnedValue Lookup::getterFallback(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    QV4::Scope scope(engine);
    QV4::ScopedObject o(scope, object.toObject(scope.engine));
    if (!o)
        return Encode::undefined();
    ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[l->nameIndex]);
    return o->get(name);
}

ReturnedValue Lookup::getter0(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object.objectValue();
        if (l->classList[0] == o->internalClass())
            return o->propertyData(l->index)->asReturnedValue();
    }
    return getterTwoClasses(l, engine, object);
}

ReturnedValue Lookup::getter1(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object.objectValue();
        if (l->classList[0] == o->internalClass() &&
            l->classList[1] == o->prototype()->internalClass)
            return o->prototype()->propertyData(l->index)->asReturnedValue();
    }
    return getterTwoClasses(l, engine, object);
}

ReturnedValue Lookup::getter2(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object.objectValue();
        if (l->classList[0] == o->internalClass()) {
            Heap::Object *p = o->prototype();
            if (l->classList[1] == p->internalClass) {
                p = p->prototype;
                if (l->classList[2] == p->internalClass)
                    return p->propertyData(l->index)->asReturnedValue();
            }
        }
    }
    l->getter = getterFallback;
    return getterFallback(l, engine, object);
}

ReturnedValue Lookup::getter0getter0(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object.objectValue();
        if (l->classList[0] == o->internalClass())
            return o->propertyData(l->index)->asReturnedValue();
        if (l->classList[2] == o->internalClass())
            return o->propertyData(l->index2)->asReturnedValue();
    }
    l->getter = getterFallback;
    return getterFallback(l, engine, object);
}

ReturnedValue Lookup::getter0getter1(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object.objectValue();
        if (l->classList[0] == o->internalClass())
            return o->propertyData(l->index)->asReturnedValue();
        if (l->classList[2] == o->internalClass() &&
            l->classList[3] == o->prototype()->internalClass)
            return o->prototype()->propertyData(l->index2)->asReturnedValue();
    }
    l->getter = getterFallback;
    return getterFallback(l, engine, object);
}

ReturnedValue Lookup::getter1getter1(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object.objectValue();
        if (l->classList[0] == o->internalClass() &&
            l->classList[1] == o->prototype()->internalClass)
            return o->prototype()->propertyData(l->index)->asReturnedValue();
        if (l->classList[2] == o->internalClass() &&
            l->classList[3] == o->prototype()->internalClass)
            return o->prototype()->propertyData(l->index2)->asReturnedValue();
        return getterFallback(l, engine, object);
    }
    l->getter = getterFallback;
    return getterFallback(l, engine, object);
}


ReturnedValue Lookup::getterAccessor0(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Object *o = object.objectValue();
        if (l->classList[0] == o->internalClass()) {
            Scope scope(o->engine());
            ScopedFunctionObject getter(scope, o->propertyData(l->index + Object::GetterOffset));
            if (!getter)
                return Encode::undefined();

            ScopedCallData callData(scope, 0);
            callData->thisObject = object;
            getter->call(scope, callData);
            return scope.result.asReturnedValue();
        }
    }
    l->getter = getterFallback;
    return getterFallback(l, engine, object);
}

ReturnedValue Lookup::getterAccessor1(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Heap::Object *o = object.objectValue()->d();
        if (l->classList[0] == o->internalClass &&
            l->classList[1] == o->prototype->internalClass) {
            Scope scope(o->internalClass->engine);
            ScopedFunctionObject getter(scope, o->prototype->propertyData(l->index + Object::GetterOffset));
            if (!getter)
                return Encode::undefined();

            ScopedCallData callData(scope, 0);
            callData->thisObject = object;
            getter->call(scope, callData);
            return scope.result.asReturnedValue();
        }
    }
    l->getter = getterFallback;
    return getterFallback(l, engine, object);
}

ReturnedValue Lookup::getterAccessor2(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.isManaged()) {
        // we can safely cast to a QV4::Object here. If object is actually a string,
        // the internal class won't match
        Heap::Object *o = object.objectValue()->d();
        if (l->classList[0] == o->internalClass) {
            o = o->prototype;
            if (l->classList[1] == o->internalClass) {
                o = o->prototype;
                if (l->classList[2] == o->internalClass) {
                    Scope scope(o->internalClass->engine);
                    ScopedFunctionObject getter(scope, o->propertyData(l->index + Object::GetterOffset));
                    if (!getter)
                        return Encode::undefined();

                    ScopedCallData callData(scope, 0);
                    callData->thisObject = object;
                    getter->call(scope, callData);
                    return scope.result.asReturnedValue();
                }
            }
        }
    }
    l->getter = getterFallback;
    return getterFallback(l, engine, object);
}

ReturnedValue Lookup::primitiveGetter0(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.type() == l->type) {
        Object *o = l->proto;
        if (l->classList[0] == o->internalClass())
            return o->propertyData(l->index)->asReturnedValue();
    }
    l->getter = getterGeneric;
    return getterGeneric(l, engine, object);
}

ReturnedValue Lookup::primitiveGetter1(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.type() == l->type) {
        Object *o = l->proto;
        if (l->classList[0] == o->internalClass() &&
            l->classList[1] == o->prototype()->internalClass)
            return o->prototype()->propertyData(l->index)->asReturnedValue();
    }
    l->getter = getterGeneric;
    return getterGeneric(l, engine, object);
}

ReturnedValue Lookup::primitiveGetterAccessor0(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.type() == l->type) {
        Object *o = l->proto;
        if (l->classList[0] == o->internalClass()) {
            Scope scope(o->engine());
            ScopedFunctionObject getter(scope, o->propertyData(l->index + Object::GetterOffset));
            if (!getter)
                return Encode::undefined();

            ScopedCallData callData(scope, 0);
            callData->thisObject = object;
            getter->call(scope, callData);
            return scope.result.asReturnedValue();
        }
    }
    l->getter = getterGeneric;
    return getterGeneric(l, engine, object);
}

ReturnedValue Lookup::primitiveGetterAccessor1(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (object.type() == l->type) {
        Object *o = l->proto;
        if (l->classList[0] == o->internalClass() &&
            l->classList[1] == o->prototype()->internalClass) {
            Scope scope(o->engine());
            ScopedFunctionObject getter(scope, o->prototype()->propertyData(l->index + Object::GetterOffset));
            if (!getter)
                return Encode::undefined();

            ScopedCallData callData(scope, 0);
            callData->thisObject = object;
            getter->call(scope, callData);
            return scope.result.asReturnedValue();
        }
    }
    l->getter = getterGeneric;
    return getterGeneric(l, engine, object);
}

ReturnedValue Lookup::stringLengthGetter(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (const String *s = object.as<String>())
        return Encode(s->d()->length());

    l->getter = getterGeneric;
    return getterGeneric(l, engine, object);
}

ReturnedValue Lookup::arrayLengthGetter(Lookup *l, ExecutionEngine *engine, const Value &object)
{
    if (const ArrayObject *a = object.as<ArrayObject>())
        return a->propertyData(Heap::ArrayObject::LengthPropertyIndex)->asReturnedValue();

    l->getter = getterGeneric;
    return getterGeneric(l, engine, object);
}


ReturnedValue Lookup::globalGetterGeneric(Lookup *l, ExecutionEngine *engine)
{
    Object *o = engine->globalObject;
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
    Scope scope(engine);
    ScopedString n(scope, engine->current->compilationUnit->runtimeStrings[l->nameIndex]);
    return engine->throwReferenceError(n);
}

ReturnedValue Lookup::globalGetter0(Lookup *l, ExecutionEngine *engine)
{
    Object *o = engine->globalObject;
    if (l->classList[0] == o->internalClass())
        return o->propertyData(l->index)->asReturnedValue();

    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, engine);
}

ReturnedValue Lookup::globalGetter1(Lookup *l, ExecutionEngine *engine)
{
    Object *o = engine->globalObject;
    if (l->classList[0] == o->internalClass() &&
        l->classList[1] == o->prototype()->internalClass)
        return o->prototype()->propertyData(l->index)->asReturnedValue();

    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, engine);
}

ReturnedValue Lookup::globalGetter2(Lookup *l, ExecutionEngine *engine)
{
    Heap::Object *o = engine->globalObject->d();
    if (l->classList[0] == o->internalClass) {
        o = o->prototype;
        if (l->classList[1] == o->internalClass) {
            o = o->prototype;
            if (l->classList[2] == o->internalClass) {
                return o->prototype->propertyData(l->index)->asReturnedValue();
            }
        }
    }
    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, engine);
}

ReturnedValue Lookup::globalGetterAccessor0(Lookup *l, ExecutionEngine *engine)
{
    Object *o = engine->globalObject;
    if (l->classList[0] == o->internalClass()) {
        Scope scope(o->engine());
        ScopedFunctionObject getter(scope, o->propertyData(l->index + Object::GetterOffset));
        if (!getter)
            return Encode::undefined();

        ScopedCallData callData(scope, 0);
        callData->thisObject = Primitive::undefinedValue();
        getter->call(scope, callData);
        return scope.result.asReturnedValue();
    }
    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, engine);
}

ReturnedValue Lookup::globalGetterAccessor1(Lookup *l, ExecutionEngine *engine)
{
    Object *o = engine->globalObject;
    if (l->classList[0] == o->internalClass() &&
        l->classList[1] == o->prototype()->internalClass) {
        Scope scope(o->engine());
        ScopedFunctionObject getter(scope, o->prototype()->propertyData(l->index + Object::GetterOffset));
        if (!getter)
            return Encode::undefined();

        ScopedCallData callData(scope, 0);
        callData->thisObject = Primitive::undefinedValue();
        getter->call(scope, callData);
        return scope.result.asReturnedValue();
    }
    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, engine);
}

ReturnedValue Lookup::globalGetterAccessor2(Lookup *l, ExecutionEngine *engine)
{
    Heap::Object *o = engine->globalObject->d();
    if (l->classList[0] == o->internalClass) {
        o = o->prototype;
        if (l->classList[1] == o->internalClass) {
            o = o->prototype;
            if (l->classList[2] == o->internalClass) {
                Scope scope(o->internalClass->engine);
                ScopedFunctionObject getter(scope, o->propertyData(l->index + Object::GetterOffset));
                if (!getter)
                    return Encode::undefined();

                ScopedCallData callData(scope, 0);
                callData->thisObject = Primitive::undefinedValue();
                getter->call(scope, callData);
                return scope.result.asReturnedValue();
            }
        }
    }
    l->globalGetter = globalGetterGeneric;
    return globalGetterGeneric(l, engine);
}

void Lookup::setterGeneric(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value)
{
    Scope scope(engine);
    ScopedObject o(scope, object);
    if (!o) {
        o = RuntimeHelpers::convertToObject(scope.engine, object);
        if (!o) // type error
            return;
        ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[l->nameIndex]);
        o->put(name, value);
        return;
    }
    o->setLookup(l, value);
}

void Lookup::setterTwoClasses(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value)
{
    Lookup l1 = *l;

    if (Object *o = object.as<Object>()) {
        o->setLookup(l, value);

        if (l->setter == Lookup::setter0) {
            l->setter = setter0setter0;
            l->classList[1] = l1.classList[0];
            l->index2 = l1.index;
            return;
        }
    }

    l->setter = setterFallback;
    setterFallback(l, engine, object, value);
}

void Lookup::setterFallback(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value)
{
    QV4::Scope scope(engine);
    QV4::ScopedObject o(scope, object.toObject(scope.engine));
    if (o) {
        ScopedString name(scope, engine->current->compilationUnit->runtimeStrings[l->nameIndex]);
        o->put(name, value);
    }
}

void Lookup::setter0(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value)
{
    Object *o = object.as<Object>();
    if (o && o->internalClass() == l->classList[0]) {
        *o->propertyData(l->index) = value;
        return;
    }

    setterTwoClasses(l, engine, object, value);
}

void Lookup::setterInsert0(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value)
{
    Object *o = object.as<Object>();
    if (o && o->internalClass() == l->classList[0]) {
        if (!o->prototype()) {
            o->setInternalClass(l->classList[3]);
            *o->propertyData(l->index) = value;
            return;
        }
    }

    l->setter = setterFallback;
    setterFallback(l, engine, object, value);
}

void Lookup::setterInsert1(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value)
{
    Object *o = object.as<Object>();
    if (o && o->internalClass() == l->classList[0]) {
        Heap::Object *p = o->prototype();
        if (p && p->internalClass == l->classList[1]) {
            o->setInternalClass(l->classList[3]);
            *o->propertyData(l->index) = value;
            return;
        }
    }

    l->setter = setterFallback;
    setterFallback(l, engine, object, value);
}

void Lookup::setterInsert2(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value)
{
    Object *o = object.as<Object>();
    if (o && o->internalClass() == l->classList[0]) {
        Heap::Object *p = o->prototype();
        if (p && p->internalClass == l->classList[1]) {
            p = p->prototype;
            if (p && p->internalClass == l->classList[2]) {
                o->setInternalClass(l->classList[3]);
                *o->propertyData(l->index) = value;
                return;
            }
        }
    }

    l->setter = setterFallback;
    setterFallback(l, engine, object, value);
}

void Lookup::setter0setter0(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value)
{
    Object *o = object.as<Object>();
    if (o) {
        if (o->internalClass() == l->classList[0]) {
            *o->propertyData(l->index) = value;
            return;
        }
        if (o->internalClass() == l->classList[1]) {
            *o->propertyData(l->index2) = value;
            return;
        }
    }

    l->setter = setterFallback;
    setterFallback(l, engine, object, value);

}

QT_END_NAMESPACE
