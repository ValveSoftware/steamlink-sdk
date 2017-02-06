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
    ExecutionEngine *v4 = ctor->engine();
    if (!callData->argc || callData->args[0].isUndefined() || callData->args[0].isNull()) {
        ScopedObject obj(scope, v4->newObject());
        ScopedObject proto(scope, ctor->get(v4->id_prototype()));
        if (!!proto)
            obj->setPrototype(proto);
        scope.result = obj.asReturnedValue();
    } else {
        scope.result = RuntimeHelpers::toObject(scope.engine, callData->args[0]);
    }
}

void ObjectCtor::call(const Managed *m, Scope &scope, CallData *callData)
{
    const ObjectCtor *ctor = static_cast<const ObjectCtor *>(m);
    ExecutionEngine *v4 = ctor->engine();
    if (!callData->argc || callData->args[0].isUndefined() || callData->args[0].isNull()) {
        scope.result = v4->newObject()->asReturnedValue();
    } else {
        scope.result = RuntimeHelpers::toObject(v4, callData->args[0]);
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

ReturnedValue ObjectPrototype::method_getPrototypeOf(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->argument(0));
    if (!o)
        return ctx->engine()->throwTypeError();

    ScopedObject p(scope, o->prototype());
    return !!p ? p->asReturnedValue() : Encode::null();
}

ReturnedValue ObjectPrototype::method_getOwnPropertyDescriptor(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject O(scope, ctx->argument(0));
    if (!O)
        return ctx->engine()->throwTypeError();

    if (ArgumentsObject::isNonStrictArgumentsObject(O))
        static_cast<ArgumentsObject *>(O.getPointer())->fullyCreate();

    ScopedValue v(scope, ctx->argument(1));
    ScopedString name(scope, v->toString(scope.engine));
    if (scope.hasException())
        return Encode::undefined();
    PropertyAttributes attrs;
    ScopedProperty desc(scope);
    O->getOwnProperty(name, &attrs, desc);
    return fromPropertyDescriptor(scope.engine, desc, attrs);
}

ReturnedValue ObjectPrototype::method_getOwnPropertyNames(CallContext *context)
{
    Scope scope(context);
    ScopedObject O(scope, context->argument(0));
    if (!O)
        return context->engine()->throwTypeError();

    ScopedArrayObject array(scope, getOwnPropertyNames(context->d()->engine, context->args()[0]));
    return array.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_create(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue O(scope, ctx->argument(0));
    if (!O->isObject() && !O->isNull())
        return ctx->engine()->throwTypeError();

    ScopedObject newObject(scope, ctx->d()->engine->newObject());
    newObject->setPrototype(O->as<Object>());

    if (ctx->argc() > 1 && !ctx->args()[1].isUndefined()) {
        ctx->d()->callData->args[0] = newObject.asReturnedValue();
        return method_defineProperties(ctx);
    }

    return newObject.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_defineProperty(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject O(scope, ctx->argument(0));
    if (!O)
        return ctx->engine()->throwTypeError();

    ScopedString name(scope, ctx->argument(1), ScopedString::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();

    ScopedValue attributes(scope, ctx->argument(2));
    ScopedProperty pd(scope);
    PropertyAttributes attrs;
    toPropertyDescriptor(scope.engine, attributes, pd, &attrs);
    if (scope.engine->hasException)
        return Encode::undefined();

    if (!O->__defineOwnProperty__(scope.engine, name, pd, attrs))
        return ctx->engine()->throwTypeError();

    return O.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_defineProperties(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject O(scope, ctx->argument(0));
    if (!O)
        return ctx->engine()->throwTypeError();

    ScopedObject o(scope, ctx->argument(1), ScopedObject::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();
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
        if (scope.engine->hasException)
            return Encode::undefined();
        bool ok;
        if (name)
            ok = O->__defineOwnProperty__(scope.engine, name, n, nattrs);
        else
            ok = O->__defineOwnProperty__(scope.engine, index, n, nattrs);
        if (!ok)
            return ctx->engine()->throwTypeError();
    }

    return O.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_seal(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->argument(0));
    if (!o)
        return ctx->engine()->throwTypeError();

    o->setInternalClass(o->internalClass()->sealed());

    if (o->arrayData()) {
        ArrayData::ensureAttributes(o);
        for (uint i = 0; i < o->d()->arrayData->alloc; ++i) {
            if (!o->arrayData()->isEmpty(i))
                o->d()->arrayData->attrs[i].setConfigurable(false);
        }
    }

    return o.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_freeze(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->argument(0));
    if (!o)
        return ctx->engine()->throwTypeError();

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
    return o.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_preventExtensions(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->argument(0));
    if (!o)
        return ctx->engine()->throwTypeError();

    o->setInternalClass(o->internalClass()->nonExtensible());
    return o.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_isSealed(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->argument(0));
    if (!o)
        return ctx->engine()->throwTypeError();

    if (o->isExtensible())
        return Encode(false);

    if (o->internalClass() != o->internalClass()->sealed())
        return Encode(false);

    if (!o->arrayData() || !o->arrayData()->length())
        return Encode(true);

    Q_ASSERT(o->arrayData() && o->arrayData()->length());
    if (!o->arrayData()->attrs)
        return Encode(false);

    for (uint i = 0; i < o->arrayData()->alloc; ++i) {
        if (!o->arrayData()->isEmpty(i))
            if (o->arrayData()->attributes(i).isConfigurable())
                return Encode(false);
    }

    return Encode(true);
}

ReturnedValue ObjectPrototype::method_isFrozen(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->argument(0));
    if (!o)
        return ctx->engine()->throwTypeError();

    if (o->isExtensible())
        return Encode(false);

    if (o->internalClass() != o->internalClass()->frozen())
        return Encode(false);

    if (!o->arrayData() || !o->arrayData()->length())
        return Encode(true);

    Q_ASSERT(o->arrayData() && o->arrayData()->length());
    if (!o->arrayData()->attrs)
        return Encode(false);

    for (uint i = 0; i < o->arrayData()->alloc; ++i) {
        if (!o->arrayData()->isEmpty(i))
            if (o->arrayData()->attributes(i).isConfigurable() || o->arrayData()->attributes(i).isWritable())
                return Encode(false);
    }

    return Encode(true);
}

ReturnedValue ObjectPrototype::method_isExtensible(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->argument(0));
    if (!o)
        return ctx->engine()->throwTypeError();

    return Encode((bool)o->isExtensible());
}

ReturnedValue ObjectPrototype::method_keys(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->argument(0));
    if (!o)
        return ctx->engine()->throwTypeError();

    ScopedArrayObject a(scope, ctx->d()->engine->newArrayObject());

    ObjectIterator it(scope, o, ObjectIterator::EnumerableOnly);
    ScopedValue name(scope);
    while (1) {
        name = it.nextPropertyNameAsString();
        if (name->isNull())
            break;
        a->push_back(name);
    }

    return a.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_toString(CallContext *ctx)
{
    Scope scope(ctx);
    if (ctx->thisObject().isUndefined()) {
        return ctx->d()->engine->newString(QStringLiteral("[object Undefined]"))->asReturnedValue();
    } else if (ctx->thisObject().isNull()) {
        return ctx->d()->engine->newString(QStringLiteral("[object Null]"))->asReturnedValue();
    } else {
        ScopedObject obj(scope, RuntimeHelpers::toObject(scope.engine, ctx->thisObject()));
        QString className = obj->className();
        return ctx->d()->engine->newString(QStringLiteral("[object %1]").arg(className))->asReturnedValue();
    }
}

ReturnedValue ObjectPrototype::method_toLocaleString(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->thisObject().toObject(scope.engine));
    if (!o)
        return Encode::undefined();
    ScopedFunctionObject f(scope, o->get(ctx->d()->engine->id_toString()));
    if (!f)
        return ctx->engine()->throwTypeError();
    ScopedCallData callData(scope);
    callData->thisObject = o;
    f->call(scope, callData);
    return scope.result.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_valueOf(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue v(scope, ctx->thisObject().toObject(scope.engine));
    if (ctx->d()->engine->hasException)
        return Encode::undefined();
    return v->asReturnedValue();
}

ReturnedValue ObjectPrototype::method_hasOwnProperty(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedString P(scope, ctx->argument(0), ScopedString::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();
    ScopedObject O(scope, ctx->thisObject(), ScopedObject::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();
    bool r = O->hasOwnProperty(P);
    if (!r)
        r = !O->query(P).isEmpty();
    return Encode(r);
}

ReturnedValue ObjectPrototype::method_isPrototypeOf(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject V(scope, ctx->argument(0));
    if (!V)
        return Encode(false);

    ScopedObject O(scope, ctx->thisObject(), ScopedObject::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();
    ScopedObject proto(scope, V->prototype());
    while (proto) {
        if (O->d() == proto->d())
            return Encode(true);
        proto = proto->prototype();
    }
    return Encode(false);
}

ReturnedValue ObjectPrototype::method_propertyIsEnumerable(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedString p(scope, ctx->argument(0), ScopedString::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();

    ScopedObject o(scope, ctx->thisObject(), ScopedObject::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();
    PropertyAttributes attrs;
    o->getOwnProperty(p, &attrs);
    return Encode(attrs.isEnumerable());
}

ReturnedValue ObjectPrototype::method_defineGetter(CallContext *ctx)
{
    if (ctx->argc() < 2)
        return ctx->engine()->throwTypeError();

    Scope scope(ctx);
    ScopedFunctionObject f(scope, ctx->argument(1));
    if (!f)
        return ctx->engine()->throwTypeError();

    ScopedString prop(scope, ctx->argument(0), ScopedString::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();

    ScopedObject o(scope, ctx->thisObject());
    if (!o) {
        if (!ctx->thisObject().isUndefined())
            return Encode::undefined();
        o = ctx->d()->engine->globalObject;
    }

    ScopedProperty pd(scope);
    pd->value = f;
    pd->set = Primitive::emptyValue();
    o->__defineOwnProperty__(scope.engine, prop, pd, Attr_Accessor);
    return Encode::undefined();
}

ReturnedValue ObjectPrototype::method_defineSetter(CallContext *ctx)
{
    if (ctx->argc() < 2)
        return ctx->engine()->throwTypeError();

    Scope scope(ctx);
    ScopedFunctionObject f(scope, ctx->argument(1));
    if (!f)
        return ctx->engine()->throwTypeError();

    ScopedString prop(scope, ctx->argument(0), ScopedString::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();

    ScopedObject o(scope, ctx->thisObject());
    if (!o) {
        if (!ctx->thisObject().isUndefined())
            return Encode::undefined();
        o = ctx->d()->engine->globalObject;
    }

    ScopedProperty pd(scope);
    pd->value = Primitive::emptyValue();
    pd->set = f;
    o->__defineOwnProperty__(scope.engine, prop, pd, Attr_Accessor);
    return Encode::undefined();
}

ReturnedValue ObjectPrototype::method_get_proto(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->thisObject().as<Object>());
    if (!o)
        return ctx->engine()->throwTypeError();

    return o->prototype()->asReturnedValue();
}

ReturnedValue ObjectPrototype::method_set_proto(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->thisObject());
    if (!o || !ctx->argc())
        return ctx->engine()->throwTypeError();

    if (ctx->args()[0].isNull()) {
        o->setPrototype(0);
        return Encode::undefined();
    }

    ScopedObject p(scope, ctx->args()[0]);
    bool ok = false;
    if (!!p) {
        if (o->prototype() == p->d()) {
            ok = true;
        } else if (o->isExtensible()) {
            ok = o->setPrototype(p);
        }
    }
    if (!ok)
        return ctx->engine()->throwTypeError(QStringLiteral("Cyclic __proto__ value"));
    return Encode::undefined();
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
