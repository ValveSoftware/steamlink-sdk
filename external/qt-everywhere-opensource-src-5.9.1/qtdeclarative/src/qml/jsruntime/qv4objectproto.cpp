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


#include "qv4objectproto_p.h"
#include "qv4argumentsobject_p.h"
#include <private/qv4mm_p.h>
#include "qv4scopedvalue_p.h"
#include "qv4runtime_p.h"
#include "qv4objectiterator_p.h"
#include "qv4string_p.h"

#include <QtCore/QDateTime>
#include <QtCore/QStringList>

using namespace QV4;


DEFINE_OBJECT_VTABLE(ObjectCtor);

void Heap::ObjectCtor::init(QV4::ExecutionContext *scope)
{
    Heap::FunctionObject::init(scope, QStringLiteral("Object"));
}

void ObjectCtor::construct(const Managed *that, Scope &scope, CallData *callData)
{
    const ObjectCtor *ctor = static_cast<const ObjectCtor *>(that);
    if (!callData->argc || callData->args[0].isUndefined() || callData->args[0].isNull()) {
        ScopedObject obj(scope, scope.engine->newObject());
        ScopedObject proto(scope, ctor->get(scope.engine->id_prototype()));
        if (!!proto)
            obj->setPrototype(proto);
        scope.result = obj.asReturnedValue();
    } else {
        scope.result = callData->args[0].toObject(scope.engine);
    }
}

void ObjectCtor::call(const Managed *, Scope &scope, CallData *callData)
{
    ExecutionEngine *v4 = scope.engine;
    if (!callData->argc || callData->args[0].isUndefined() || callData->args[0].isNull()) {
        scope.result = v4->newObject()->asReturnedValue();
    } else {
        scope.result = callData->args[0].toObject(v4);
    }
}

void ObjectPrototype::init(ExecutionEngine *v4, Object *ctor)
{
    Scope scope(v4);
    ScopedObject o(scope, this);

    ctor->defineReadonlyProperty(v4->id_prototype(), o);
    ctor->defineReadonlyProperty(v4->id_length(), Primitive::fromInt32(1));
    ctor->defineDefaultProperty(QStringLiteral("getPrototypeOf"), method_getPrototypeOf, 1);
    ctor->defineDefaultProperty(QStringLiteral("getOwnPropertyDescriptor"), method_getOwnPropertyDescriptor, 2);
    ctor->defineDefaultProperty(QStringLiteral("getOwnPropertyNames"), method_getOwnPropertyNames, 1);
    ctor->defineDefaultProperty(QStringLiteral("create"), method_create, 2);
    ctor->defineDefaultProperty(QStringLiteral("defineProperty"), method_defineProperty, 3);
    ctor->defineDefaultProperty(QStringLiteral("defineProperties"), method_defineProperties, 2);
    ctor->defineDefaultProperty(QStringLiteral("seal"), method_seal, 1);
    ctor->defineDefaultProperty(QStringLiteral("freeze"), method_freeze, 1);
    ctor->defineDefaultProperty(QStringLiteral("preventExtensions"), method_preventExtensions, 1);
    ctor->defineDefaultProperty(QStringLiteral("isSealed"), method_isSealed, 1);
    ctor->defineDefaultProperty(QStringLiteral("isFrozen"), method_isFrozen, 1);
    ctor->defineDefaultProperty(QStringLiteral("isExtensible"), method_isExtensible, 1);
    ctor->defineDefaultProperty(QStringLiteral("keys"), method_keys, 1);

    defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    defineDefaultProperty(v4->id_toString(), method_toString, 0);
    defineDefaultProperty(QStringLiteral("toLocaleString"), method_toLocaleString, 0);
    defineDefaultProperty(v4->id_valueOf(), method_valueOf, 0);
    defineDefaultProperty(QStringLiteral("hasOwnProperty"), method_hasOwnProperty, 1);
    defineDefaultProperty(QStringLiteral("isPrototypeOf"), method_isPrototypeOf, 1);
    defineDefaultProperty(QStringLiteral("propertyIsEnumerable"), method_propertyIsEnumerable, 1);
    defineDefaultProperty(QStringLiteral("__defineGetter__"), method_defineGetter, 2);
    defineDefaultProperty(QStringLiteral("__defineSetter__"), method_defineSetter, 2);

    ExecutionContext *global = v4->rootContext();
    ScopedProperty p(scope);
    p->value = BuiltinFunction::create(global, v4->id___proto__(), method_get_proto);
    p->set = BuiltinFunction::create(global, v4->id___proto__(), method_set_proto);
    insertMember(v4->id___proto__(), p, Attr_Accessor|Attr_NotEnumerable);
}

void ObjectPrototype::method_getPrototypeOf(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->argument(0));
    if (!o)
        THROW_TYPE_ERROR();

    ScopedObject p(scope, o->prototype());
    scope.result = !!p ? p->asReturnedValue() : Encode::null();
}

void ObjectPrototype::method_getOwnPropertyDescriptor(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject O(scope, callData->argument(0));
    if (!O) {
        scope.result = scope.engine->throwTypeError();
        return;
    }

    if (ArgumentsObject::isNonStrictArgumentsObject(O))
        static_cast<ArgumentsObject *>(O.getPointer())->fullyCreate();

    ScopedValue v(scope, callData->argument(1));
    ScopedString name(scope, v->toString(scope.engine));
    CHECK_EXCEPTION();

    PropertyAttributes attrs;
    ScopedProperty desc(scope);
    O->getOwnProperty(name, &attrs, desc);
    scope.result = fromPropertyDescriptor(scope.engine, desc, attrs);
}

void ObjectPrototype::method_getOwnPropertyNames(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject O(scope, callData->argument(0));
    if (!O) {
        scope.result = scope.engine->throwTypeError();
        return;
    }

    scope.result = getOwnPropertyNames(scope.engine, callData->args[0]);
}

void ObjectPrototype::method_create(const BuiltinFunction *builtin, Scope &scope, CallData *callData)
{
    ScopedValue O(scope, callData->argument(0));
    if (!O->isObject() && !O->isNull()) {
        scope.result = scope.engine->throwTypeError();
        return;
    }

    ScopedObject newObject(scope, scope.engine->newObject());
    newObject->setPrototype(O->as<Object>());

    if (callData->argc > 1 && !callData->args[1].isUndefined()) {
        callData->args[0] = newObject;
        method_defineProperties(builtin, scope, callData);
        return;
    }

    scope.result = newObject;
}

void ObjectPrototype::method_defineProperty(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject O(scope, callData->argument(0));
    if (!O) {
        scope.result = scope.engine->throwTypeError();
        return;
    }

    ScopedString name(scope, callData->argument(1), ScopedString::Convert);
    CHECK_EXCEPTION();

    ScopedValue attributes(scope, callData->argument(2));
    ScopedProperty pd(scope);
    PropertyAttributes attrs;
    toPropertyDescriptor(scope.engine, attributes, pd, &attrs);
    CHECK_EXCEPTION();

    if (!O->__defineOwnProperty__(scope.engine, name, pd, attrs))
        THROW_TYPE_ERROR();

    scope.result = O;
}

void ObjectPrototype::method_defineProperties(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject O(scope, callData->argument(0));
    if (!O)
        THROW_TYPE_ERROR();

    ScopedObject o(scope, callData->argument(1), ScopedObject::Convert);
    CHECK_EXCEPTION();

    ScopedValue val(scope);

    ObjectIterator it(scope, o, ObjectIterator::EnumerableOnly);
    ScopedString name(scope);
    ScopedProperty pd(scope);
    ScopedProperty n(scope);
    while (1) {
        uint index;
        PropertyAttributes attrs;
        it.next(name.getRef(), &index, pd, &attrs);
        if (attrs.isEmpty())
            break;
        PropertyAttributes nattrs;
        val = o->getValue(pd->value, attrs);
        toPropertyDescriptor(scope.engine, val, n, &nattrs);
        CHECK_EXCEPTION();
        bool ok;
        if (name)
            ok = O->__defineOwnProperty__(scope.engine, name, n, nattrs);
        else
            ok = O->__defineOwnProperty__(scope.engine, index, n, nattrs);
        if (!ok)
            THROW_TYPE_ERROR();
    }

    scope.result = O;
}

void ObjectPrototype::method_seal(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->argument(0));
    if (!o)
        THROW_TYPE_ERROR();

    o->setInternalClass(o->internalClass()->sealed());

    if (o->arrayData()) {
        ArrayData::ensureAttributes(o);
        for (uint i = 0; i < o->d()->arrayData->alloc; ++i) {
            if (!o->arrayData()->isEmpty(i))
                o->d()->arrayData->attrs[i].setConfigurable(false);
        }
    }

    scope.result = o;
}

void ObjectPrototype::method_freeze(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->argument(0));
    if (!o)
        THROW_TYPE_ERROR();

    if (ArgumentsObject::isNonStrictArgumentsObject(o))
        static_cast<ArgumentsObject *>(o.getPointer())->fullyCreate();

    o->setInternalClass(o->internalClass()->frozen());

    if (o->arrayData()) {
        ArrayData::ensureAttributes(o);
        for (uint i = 0; i < o->arrayData()->alloc; ++i) {
            if (!o->arrayData()->isEmpty(i))
                o->arrayData()->attrs[i].setConfigurable(false);
            if (o->arrayData()->attrs[i].isData())
                o->arrayData()->attrs[i].setWritable(false);
        }
    }
    scope.result = o;
}

void ObjectPrototype::method_preventExtensions(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->argument(0));
    if (!o)
        THROW_TYPE_ERROR();

    o->setInternalClass(o->internalClass()->nonExtensible());
    scope.result = o;
}

void ObjectPrototype::method_isSealed(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->argument(0));
    if (!o)
        THROW_TYPE_ERROR();

    if (o->isExtensible()) {
        scope.result = Encode(false);
        return;
    }

    if (o->internalClass() != o->internalClass()->sealed()) {
        scope.result = Encode(false);
        return;
    }

    if (!o->arrayData() || !o->arrayData()->length()) {
        scope.result = Encode(true);
        return;
    }

    Q_ASSERT(o->arrayData() && o->arrayData()->length());
    if (!o->arrayData()->attrs) {
        scope.result = Encode(false);
        return;
    }

    for (uint i = 0; i < o->arrayData()->alloc; ++i) {
        if (!o->arrayData()->isEmpty(i))
            if (o->arrayData()->attributes(i).isConfigurable()) {
                scope.result = Encode(false);
                return;
            }
    }

    scope.result = Encode(true);
}

void ObjectPrototype::method_isFrozen(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->argument(0));
    if (!o)
        THROW_TYPE_ERROR();

    if (o->isExtensible()) {
        scope.result = Encode(false);
        return;
    }

    if (o->internalClass() != o->internalClass()->frozen()) {
        scope.result = Encode(false);
        return;
    }

    if (!o->arrayData() || !o->arrayData()->length()) {
        scope.result = Encode(true);
        return;
    }

    Q_ASSERT(o->arrayData() && o->arrayData()->length());
    if (!o->arrayData()->attrs) {
        scope.result = Encode(false);
        return;
    }

    for (uint i = 0; i < o->arrayData()->alloc; ++i) {
        if (!o->arrayData()->isEmpty(i))
            if (o->arrayData()->attributes(i).isConfigurable() || o->arrayData()->attributes(i).isWritable()) {
                scope.result = Encode(false);
                return;
            }
    }

    scope.result = Encode(true);
}

void ObjectPrototype::method_isExtensible(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->argument(0));
    if (!o)
        THROW_TYPE_ERROR();

    scope.result = Encode((bool)o->isExtensible());
}

void ObjectPrototype::method_keys(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->argument(0));
    if (!o)
        THROW_TYPE_ERROR();

    ScopedArrayObject a(scope, scope.engine->newArrayObject());

    ObjectIterator it(scope, o, ObjectIterator::EnumerableOnly);
    ScopedValue name(scope);
    while (1) {
        name = it.nextPropertyNameAsString();
        if (name->isNull())
            break;
        a->push_back(name);
    }

    scope.result = a;
}

void ObjectPrototype::method_toString(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->thisObject.isUndefined()) {
        scope.result = scope.engine->newString(QStringLiteral("[object Undefined]"));
    } else if (callData->thisObject.isNull()) {
        scope.result = scope.engine->newString(QStringLiteral("[object Null]"));
    } else {
        ScopedObject obj(scope, callData->thisObject.toObject(scope.engine));
        QString className = obj->className();
        scope.result = scope.engine->newString(QStringLiteral("[object %1]").arg(className));
    }
}

void ObjectPrototype::method_toLocaleString(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->thisObject.toObject(scope.engine));
    if (!o)
        RETURN_UNDEFINED();

    ScopedFunctionObject f(scope, o->get(scope.engine->id_toString()));
    if (!f)
        THROW_TYPE_ERROR();
    ScopedCallData cData(scope);
    cData->thisObject = o;
    f->call(scope, callData);
}

void ObjectPrototype::method_valueOf(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    scope.result = callData->thisObject.toObject(scope.engine);
}

void ObjectPrototype::method_hasOwnProperty(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedString P(scope, callData->argument(0), ScopedString::Convert);
    CHECK_EXCEPTION();
    ScopedObject O(scope, callData->thisObject, ScopedObject::Convert);
    CHECK_EXCEPTION();
    bool r = O->hasOwnProperty(P);
    if (!r)
        r = !O->query(P).isEmpty();
    scope.result = Encode(r);
}

void ObjectPrototype::method_isPrototypeOf(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject V(scope, callData->argument(0));
    if (!V) {
        scope.result = Encode(false);
        return;
    }

    ScopedObject O(scope, callData->thisObject, ScopedObject::Convert);
    CHECK_EXCEPTION();
    ScopedObject proto(scope, V->prototype());
    while (proto) {
        if (O->d() == proto->d()) {
            scope.result = Encode(true);
            return;
        }
        proto = proto->prototype();
    }
    scope.result = Encode(false);
}

void ObjectPrototype::method_propertyIsEnumerable(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedString p(scope, callData->argument(0), ScopedString::Convert);
    CHECK_EXCEPTION();

    ScopedObject o(scope, callData->thisObject, ScopedObject::Convert);
    CHECK_EXCEPTION();
    PropertyAttributes attrs;
    o->getOwnProperty(p, &attrs);
    scope.result = Encode(attrs.isEnumerable());
}

void ObjectPrototype::method_defineGetter(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 2)
        THROW_TYPE_ERROR();

    ScopedFunctionObject f(scope, callData->argument(1));
    if (!f)
        THROW_TYPE_ERROR();

    ScopedString prop(scope, callData->argument(0), ScopedString::Convert);
    CHECK_EXCEPTION();

    ScopedObject o(scope, callData->thisObject);
    if (!o) {
        if (!callData->thisObject.isUndefined())
            RETURN_UNDEFINED();
        o = scope.engine->globalObject;
    }

    ScopedProperty pd(scope);
    pd->value = f;
    pd->set = Primitive::emptyValue();
    o->__defineOwnProperty__(scope.engine, prop, pd, Attr_Accessor);
    RETURN_UNDEFINED();
}

void ObjectPrototype::method_defineSetter(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (callData->argc < 2)
        THROW_TYPE_ERROR();

    ScopedFunctionObject f(scope, callData->argument(1));
    if (!f)
        THROW_TYPE_ERROR();

    ScopedString prop(scope, callData->argument(0), ScopedString::Convert);
    CHECK_EXCEPTION();

    ScopedObject o(scope, callData->thisObject);
    if (!o) {
        if (!callData->thisObject.isUndefined())
            RETURN_UNDEFINED();
        o = scope.engine->globalObject;
    }

    ScopedProperty pd(scope);
    pd->value = Primitive::emptyValue();
    pd->set = f;
    o->__defineOwnProperty__(scope.engine, prop, pd, Attr_Accessor);
    RETURN_UNDEFINED();
}

void ObjectPrototype::method_get_proto(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->thisObject.as<Object>());
    if (!o)
        THROW_TYPE_ERROR();

    scope.result = o->prototype();
}

void ObjectPrototype::method_set_proto(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    ScopedObject o(scope, callData->thisObject);
    if (!o || !callData->argc)
        THROW_TYPE_ERROR();

    if (callData->args[0].isNull()) {
        o->setPrototype(0);
        RETURN_UNDEFINED();
    }

    ScopedObject p(scope, callData->args[0]);
    bool ok = false;
    if (!!p) {
        if (o->prototype() == p->d()) {
            ok = true;
        } else if (o->isExtensible()) {
            ok = o->setPrototype(p);
        }
    }
    if (!ok) {
        scope.result = scope.engine->throwTypeError(QStringLiteral("Cyclic __proto__ value"));
        return;
    }
    RETURN_UNDEFINED();
}

void ObjectPrototype::toPropertyDescriptor(ExecutionEngine *engine, const Value &v, Property *desc, PropertyAttributes *attrs)
{
    Scope scope(engine);
    ScopedObject o(scope, v);
    if (!o) {
        engine->throwTypeError();
        return;
    }

    attrs->clear();
    desc->value = Primitive::emptyValue();
    desc->set = Primitive::emptyValue();
    ScopedValue tmp(scope);

    if (o->hasProperty(engine->id_enumerable()))
        attrs->setEnumerable((tmp = o->get(engine->id_enumerable()))->toBoolean());

    if (o->hasProperty(engine->id_configurable()))
        attrs->setConfigurable((tmp = o->get(engine->id_configurable()))->toBoolean());

    if (o->hasProperty(engine->id_get())) {
        ScopedValue get(scope, o->get(engine->id_get()));
        FunctionObject *f = get->as<FunctionObject>();
        if (f || get->isUndefined()) {
            desc->value = get;
        } else {
            engine->throwTypeError();
            return;
        }
        attrs->setType(PropertyAttributes::Accessor);
    }

    if (o->hasProperty(engine->id_set())) {
        ScopedValue set(scope, o->get(engine->id_set()));
        FunctionObject *f = set->as<FunctionObject>();
        if (f || set->isUndefined()) {
            desc->set = set;
        } else {
            engine->throwTypeError();
            return;
        }
        attrs->setType(PropertyAttributes::Accessor);
    }

    if (o->hasProperty(engine->id_writable())) {
        if (attrs->isAccessor()) {
            engine->throwTypeError();
            return;
        }
        attrs->setWritable((tmp = o->get(engine->id_writable()))->toBoolean());
        // writable forces it to be a data descriptor
        desc->value = Primitive::undefinedValue();
    }

    if (o->hasProperty(engine->id_value())) {
        if (attrs->isAccessor()) {
            engine->throwTypeError();
            return;
        }
        desc->value = o->get(engine->id_value());
        attrs->setType(PropertyAttributes::Data);
    }

    if (attrs->isGeneric())
        desc->value = Primitive::emptyValue();
}


ReturnedValue ObjectPrototype::fromPropertyDescriptor(ExecutionEngine *engine, const Property *desc, PropertyAttributes attrs)
{
    if (attrs.isEmpty())
        return Encode::undefined();

    Scope scope(engine);
    // Let obj be the result of creating a new object as if by the expression new Object() where Object
    // is the standard built-in constructor with that name.
    ScopedObject o(scope, engine->newObject());
    ScopedString s(scope);

    ScopedProperty pd(scope);
    if (attrs.isData()) {
        pd->value = desc->value;
        s = engine->newString(QStringLiteral("value"));
        o->__defineOwnProperty__(scope.engine, s, pd, Attr_Data);
        pd->value = Primitive::fromBoolean(attrs.isWritable());
        s = engine->newString(QStringLiteral("writable"));
        o->__defineOwnProperty__(scope.engine, s, pd, Attr_Data);
    } else {
        pd->value = desc->getter() ? desc->getter()->asReturnedValue() : Encode::undefined();
        s = engine->newString(QStringLiteral("get"));
        o->__defineOwnProperty__(scope.engine, s, pd, Attr_Data);
        pd->value = desc->setter() ? desc->setter()->asReturnedValue() : Encode::undefined();
        s = engine->newString(QStringLiteral("set"));
        o->__defineOwnProperty__(scope.engine, s, pd, Attr_Data);
    }
    pd->value = Primitive::fromBoolean(attrs.isEnumerable());
    s = engine->newString(QStringLiteral("enumerable"));
    o->__defineOwnProperty__(scope.engine, s, pd, Attr_Data);
    pd->value = Primitive::fromBoolean(attrs.isConfigurable());
    s = engine->newString(QStringLiteral("configurable"));
    o->__defineOwnProperty__(scope.engine, s, pd, Attr_Data);

    return o.asReturnedValue();
}


Heap::ArrayObject *ObjectPrototype::getOwnPropertyNames(ExecutionEngine *v4, const Value &o)
{
    Scope scope(v4);
    ScopedArrayObject array(scope, v4->newArrayObject());
    ScopedObject O(scope, o);
    if (O) {
        ObjectIterator it(scope, O, ObjectIterator::NoFlags);
        ScopedValue name(scope);
        while (1) {
            name = it.nextPropertyNameAsString();
            if (name->isNull())
                break;
            array->push_back(name);
        }
    }
    return array->d();
}
