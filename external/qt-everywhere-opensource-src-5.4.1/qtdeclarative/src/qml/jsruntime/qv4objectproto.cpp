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


#include "qv4objectproto_p.h"
#include "qv4argumentsobject_p.h"
#include "qv4mm_p.h"
#include "qv4scopedvalue_p.h"
#include <QtCore/qnumeric.h>
#include <QtCore/qmath.h>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QDebug>
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


DEFINE_OBJECT_VTABLE(ObjectCtor);

ObjectCtor::Data::Data(ExecutionContext *scope)
    : FunctionObject::Data(scope, QStringLiteral("Object"))
{
    setVTable(staticVTable());
}

ReturnedValue ObjectCtor::construct(Managed *that, CallData *callData)
{
    ExecutionEngine *v4 = that->engine();
    Scope scope(v4);
    ObjectCtor *ctor = static_cast<ObjectCtor *>(that);
    if (!callData->argc || callData->args[0].isUndefined() || callData->args[0].isNull()) {
        Scoped<Object> obj(scope, v4->newObject());
        Scoped<Object> proto(scope, ctor->get(v4->id_prototype));
        if (!!proto)
            obj->setPrototype(proto.getPointer());
        return obj.asReturnedValue();
    }
    return RuntimeHelpers::toObject(v4->currentContext(), ValueRef(&callData->args[0]));
}

ReturnedValue ObjectCtor::call(Managed *m, CallData *callData)
{
    if (!callData->argc || callData->args[0].isUndefined() || callData->args[0].isNull())
        return m->engine()->newObject()->asReturnedValue();
    return RuntimeHelpers::toObject(m->engine()->currentContext(), ValueRef(&callData->args[0]));
}

void ObjectPrototype::init(ExecutionEngine *v4, Object *ctor)
{
    Scope scope(v4);
    ScopedObject o(scope, this);

    ctor->defineReadonlyProperty(v4->id_prototype, o);
    ctor->defineReadonlyProperty(v4->id_length, Primitive::fromInt32(1));
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
    defineDefaultProperty(v4->id_toString, method_toString, 0);
    defineDefaultProperty(QStringLiteral("toLocaleString"), method_toLocaleString, 0);
    defineDefaultProperty(v4->id_valueOf, method_valueOf, 0);
    defineDefaultProperty(QStringLiteral("hasOwnProperty"), method_hasOwnProperty, 1);
    defineDefaultProperty(QStringLiteral("isPrototypeOf"), method_isPrototypeOf, 1);
    defineDefaultProperty(QStringLiteral("propertyIsEnumerable"), method_propertyIsEnumerable, 1);
    defineDefaultProperty(QStringLiteral("__defineGetter__"), method_defineGetter, 2);
    defineDefaultProperty(QStringLiteral("__defineSetter__"), method_defineSetter, 2);

    Property p(ScopedFunctionObject(scope, BuiltinFunction::create(v4->rootContext, v4->id___proto__, method_get_proto)).getPointer(),
               ScopedFunctionObject(scope, BuiltinFunction::create(v4->rootContext, v4->id___proto__, method_set_proto)).getPointer());
    insertMember(v4->id___proto__, p, Attr_Accessor|Attr_NotEnumerable);
}

ReturnedValue ObjectPrototype::method_getPrototypeOf(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> o(scope, ctx->argument(0));
    if (!o)
        return ctx->throwTypeError();

    Scoped<Object> p(scope, o->prototype());
    return !!p ? p->asReturnedValue() : Encode::null();
}

ReturnedValue ObjectPrototype::method_getOwnPropertyDescriptor(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> O(scope, ctx->argument(0));
    if (!O)
        return ctx->throwTypeError();

    if (ArgumentsObject::isNonStrictArgumentsObject(O.getPointer()))
        Scoped<ArgumentsObject>(scope, O)->fullyCreate();

    ScopedValue v(scope, ctx->argument(1));
    Scoped<String> name(scope, v->toString(ctx));
    if (scope.hasException())
        return Encode::undefined();
    PropertyAttributes attrs;
    Property *desc = O->__getOwnProperty__(name.getPointer(), &attrs);
    return fromPropertyDescriptor(ctx, desc, attrs);
}

ReturnedValue ObjectPrototype::method_getOwnPropertyNames(CallContext *context)
{
    Scope scope(context);
    ScopedObject O(scope, context->argument(0));
    if (!O)
        return context->throwTypeError();

    ScopedArrayObject array(scope, getOwnPropertyNames(context->d()->engine, context->d()->callData->args[0]));
    return array.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_create(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue O(scope, ctx->argument(0));
    if (!O->isObject() && !O->isNull())
        return ctx->throwTypeError();

    Scoped<Object> newObject(scope, ctx->d()->engine->newObject());
    newObject->setPrototype(O->asObject());

    if (ctx->d()->callData->argc > 1 && !ctx->d()->callData->args[1].isUndefined()) {
        ctx->d()->callData->args[0] = newObject.asReturnedValue();
        return method_defineProperties(ctx);
    }

    return newObject.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_defineProperty(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> O(scope, ctx->argument(0));
    if (!O)
        return ctx->throwTypeError();

    Scoped<String> name(scope, ctx->argument(1), Scoped<String>::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();

    ScopedValue attributes(scope, ctx->argument(2));
    Property pd;
    PropertyAttributes attrs;
    toPropertyDescriptor(ctx, attributes, &pd, &attrs);
    if (scope.engine->hasException)
        return Encode::undefined();

    if (!O->__defineOwnProperty__(ctx, name.getPointer(), pd, attrs))
        return ctx->throwTypeError();

    return O.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_defineProperties(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> O(scope, ctx->argument(0));
    if (!O)
        return ctx->throwTypeError();

    Scoped<Object> o(scope, ctx->argument(1), Scoped<Object>::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();
    ScopedValue val(scope);

    ObjectIterator it(scope, o, ObjectIterator::EnumerableOnly);
    ScopedString name(scope);
    while (1) {
        uint index;
        PropertyAttributes attrs;
        Property pd;
        String *nm;
        it.next(nm, &index, &pd, &attrs);
        name = nm;
        if (attrs.isEmpty())
            break;
        Property n;
        PropertyAttributes nattrs;
        val = o->getValue(&pd, attrs);
        toPropertyDescriptor(ctx, val, &n, &nattrs);
        if (scope.engine->hasException)
            return Encode::undefined();
        bool ok;
        if (name)
            ok = O->__defineOwnProperty__(ctx, name.getPointer(), n, nattrs);
        else
            ok = O->__defineOwnProperty__(ctx, index, n, nattrs);
        if (!ok)
            return ctx->throwTypeError();
    }

    return O.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_seal(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> o(scope, ctx->argument(0));
    if (!o)
        return ctx->throwTypeError();

    o->setExtensible(false);

    o->setInternalClass(o->internalClass()->sealed());

    if (o->arrayData()) {
        ArrayData::ensureAttributes(o.getPointer());
        for (uint i = 0; i < o->arrayData()->alloc(); ++i) {
            if (!o->arrayData()->isEmpty(i))
                o->arrayData()->attrs()[i].setConfigurable(false);
        }
    }

    return o.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_freeze(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> o(scope, ctx->argument(0));
    if (!o)
        return ctx->throwTypeError();

    if (ArgumentsObject::isNonStrictArgumentsObject(o.getPointer()))
        Scoped<ArgumentsObject>(scope, o)->fullyCreate();

    o->setExtensible(false);

    o->setInternalClass(o->internalClass()->frozen());

    if (o->arrayData()) {
        ArrayData::ensureAttributes(o.getPointer());
        for (uint i = 0; i < o->arrayData()->alloc(); ++i) {
            if (!o->arrayData()->isEmpty(i))
                o->arrayData()->attrs()[i].setConfigurable(false);
            if (o->arrayData()->attrs()[i].isData())
                o->arrayData()->attrs()[i].setWritable(false);
        }
    }
    return o.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_preventExtensions(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> o(scope, ctx->argument(0));
    if (!o)
        return ctx->throwTypeError();

    o->setExtensible(false);
    return o.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_isSealed(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> o(scope, ctx->argument(0));
    if (!o)
        return ctx->throwTypeError();

    if (o->isExtensible())
        return Encode(false);

    if (o->internalClass() != o->internalClass()->sealed())
        return Encode(false);

    if (!o->arrayData() || !o->arrayData()->length())
        return Encode(true);

    Q_ASSERT(o->arrayData() && o->arrayData()->length());
    if (!o->arrayData()->attrs())
        return Encode(false);

    for (uint i = 0; i < o->arrayData()->alloc(); ++i) {
        if (!o->arrayData()->isEmpty(i))
            if (o->arrayData()->attributes(i).isConfigurable())
                return Encode(false);
    }

    return Encode(true);
}

ReturnedValue ObjectPrototype::method_isFrozen(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> o(scope, ctx->argument(0));
    if (!o)
        return ctx->throwTypeError();

    if (o->isExtensible())
        return Encode(false);

    if (o->internalClass() != o->internalClass()->frozen())
        return Encode(false);

    if (!o->arrayData() || !o->arrayData()->length())
        return Encode(true);

    Q_ASSERT(o->arrayData() && o->arrayData()->length());
    if (!o->arrayData()->attrs())
        return Encode(false);

    for (uint i = 0; i < o->arrayData()->alloc(); ++i) {
        if (!o->arrayData()->isEmpty(i))
            if (o->arrayData()->attributes(i).isConfigurable() || o->arrayData()->attributes(i).isWritable())
                return Encode(false);
    }

    return Encode(true);
}

ReturnedValue ObjectPrototype::method_isExtensible(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> o(scope, ctx->argument(0));
    if (!o)
        return ctx->throwTypeError();

    return Encode((bool)o->isExtensible());
}

ReturnedValue ObjectPrototype::method_keys(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> o(scope, ctx->argument(0));
    if (!o)
        return ctx->throwTypeError();

    Scoped<ArrayObject> a(scope, ctx->d()->engine->newArrayObject());

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
    if (ctx->d()->callData->thisObject.isUndefined()) {
        return ctx->d()->engine->newString(QStringLiteral("[object Undefined]"))->asReturnedValue();
    } else if (ctx->d()->callData->thisObject.isNull()) {
        return ctx->d()->engine->newString(QStringLiteral("[object Null]"))->asReturnedValue();
    } else {
        ScopedObject obj(scope, RuntimeHelpers::toObject(ctx, ValueRef(&ctx->d()->callData->thisObject)));
        QString className = obj->className();
        return ctx->d()->engine->newString(QString::fromLatin1("[object %1]").arg(className))->asReturnedValue();
    }
}

ReturnedValue ObjectPrototype::method_toLocaleString(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->d()->callData->thisObject.toObject(ctx));
    if (!o)
        return Encode::undefined();
    Scoped<FunctionObject> f(scope, o->get(ctx->d()->engine->id_toString));
    if (!f)
        return ctx->throwTypeError();
    ScopedCallData callData(scope, 0);
    callData->thisObject = o;
    return f->call(callData);
}

ReturnedValue ObjectPrototype::method_valueOf(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue v(scope, ctx->d()->callData->thisObject.toObject(ctx));
    if (ctx->d()->engine->hasException)
        return Encode::undefined();
    return v.asReturnedValue();
}

ReturnedValue ObjectPrototype::method_hasOwnProperty(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<String> P(scope, ctx->argument(0), Scoped<String>::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();
    Scoped<Object> O(scope, ctx->d()->callData->thisObject, Scoped<Object>::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();
    bool r = O->hasOwnProperty(P.getPointer());
    if (!r)
        r = !O->query(P.getPointer()).isEmpty();
    return Encode(r);
}

ReturnedValue ObjectPrototype::method_isPrototypeOf(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> V(scope, ctx->argument(0));
    if (!V)
        return Encode(false);

    Scoped<Object> O(scope, ctx->d()->callData->thisObject, Scoped<Object>::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();
    Scoped<Object> proto(scope, V->prototype());
    while (proto) {
        if (O.getPointer() == proto.getPointer())
            return Encode(true);
        proto = proto->prototype();
    }
    return Encode(false);
}

ReturnedValue ObjectPrototype::method_propertyIsEnumerable(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<String> p(scope, ctx->argument(0), Scoped<String>::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();

    Scoped<Object> o(scope, ctx->d()->callData->thisObject, Scoped<Object>::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();
    PropertyAttributes attrs;
    o->__getOwnProperty__(p.getPointer(), &attrs);
    return Encode(attrs.isEnumerable());
}

ReturnedValue ObjectPrototype::method_defineGetter(CallContext *ctx)
{
    if (ctx->d()->callData->argc < 2)
        return ctx->throwTypeError();

    Scope scope(ctx);
    Scoped<FunctionObject> f(scope, ctx->argument(1));
    if (!f)
        return ctx->throwTypeError();

    Scoped<String> prop(scope, ctx->argument(0), Scoped<String>::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();

    Scoped<Object> o(scope, ctx->d()->callData->thisObject);
    if (!o) {
        if (!ctx->d()->callData->thisObject.isUndefined())
            return Encode::undefined();
        o = ctx->d()->engine->globalObject;
    }

    Property pd;
    pd.value = f;
    pd.set = Primitive::emptyValue();
    o->__defineOwnProperty__(ctx, prop.getPointer(), pd, Attr_Accessor);
    return Encode::undefined();
}

ReturnedValue ObjectPrototype::method_defineSetter(CallContext *ctx)
{
    if (ctx->d()->callData->argc < 2)
        return ctx->throwTypeError();

    Scope scope(ctx);
    Scoped<FunctionObject> f(scope, ctx->argument(1));
    if (!f)
        return ctx->throwTypeError();

    Scoped<String> prop(scope, ctx->argument(0), Scoped<String>::Convert);
    if (scope.engine->hasException)
        return Encode::undefined();

    Scoped<Object> o(scope, ctx->d()->callData->thisObject);
    if (!o) {
        if (!ctx->d()->callData->thisObject.isUndefined())
            return Encode::undefined();
        o = ctx->d()->engine->globalObject;
    }

    Property pd;
    pd.value = Primitive::emptyValue();
    pd.set = f;
    o->__defineOwnProperty__(ctx, prop.getPointer(), pd, Attr_Accessor);
    return Encode::undefined();
}

ReturnedValue ObjectPrototype::method_get_proto(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedObject o(scope, ctx->d()->callData->thisObject.asObject());
    if (!o)
        return ctx->throwTypeError();

    return o->prototype()->asReturnedValue();
}

ReturnedValue ObjectPrototype::method_set_proto(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<Object> o(scope, ctx->d()->callData->thisObject);
    if (!o || !ctx->d()->callData->argc)
        return ctx->throwTypeError();

    if (ctx->d()->callData->args[0].isNull()) {
        o->setPrototype(0);
        return Encode::undefined();
    }

    Scoped<Object> p(scope, ctx->d()->callData->args[0]);
    bool ok = false;
    if (!!p) {
        if (o->prototype() == p.getPointer()) {
            ok = true;
        } else if (o->isExtensible()) {
            ok = o->setPrototype(p.getPointer());
        }
    }
    if (!ok)
        return ctx->throwTypeError(QStringLiteral("Cyclic __proto__ value"));
    return Encode::undefined();
}

void ObjectPrototype::toPropertyDescriptor(ExecutionContext *ctx, const ValueRef v, Property *desc, PropertyAttributes *attrs)
{
    Scope scope(ctx);
    ScopedObject o(scope, v);
    if (!o) {
        ctx->throwTypeError();
        return;
    }

    attrs->clear();
    desc->value = Primitive::emptyValue();
    desc->set = Primitive::emptyValue();
    ScopedValue tmp(scope);

    if (o->hasProperty(ctx->d()->engine->id_enumerable))
        attrs->setEnumerable((tmp = o->get(ctx->d()->engine->id_enumerable))->toBoolean());

    if (o->hasProperty(ctx->d()->engine->id_configurable))
        attrs->setConfigurable((tmp = o->get(ctx->d()->engine->id_configurable))->toBoolean());

    if (o->hasProperty(ctx->d()->engine->id_get)) {
        ScopedValue get(scope, o->get(ctx->d()->engine->id_get));
        FunctionObject *f = get->asFunctionObject();
        if (f || get->isUndefined()) {
            desc->value = get;
        } else {
            ctx->throwTypeError();
            return;
        }
        attrs->setType(PropertyAttributes::Accessor);
    }

    if (o->hasProperty(ctx->d()->engine->id_set)) {
        ScopedValue set(scope, o->get(ctx->d()->engine->id_set));
        FunctionObject *f = set->asFunctionObject();
        if (f || set->isUndefined()) {
            desc->set = set;
        } else {
            ctx->throwTypeError();
            return;
        }
        attrs->setType(PropertyAttributes::Accessor);
    }

    if (o->hasProperty(ctx->d()->engine->id_writable)) {
        if (attrs->isAccessor()) {
            ctx->throwTypeError();
            return;
        }
        attrs->setWritable((tmp = o->get(ctx->d()->engine->id_writable))->toBoolean());
        // writable forces it to be a data descriptor
        desc->value = Primitive::undefinedValue();
    }

    if (o->hasProperty(ctx->d()->engine->id_value)) {
        if (attrs->isAccessor()) {
            ctx->throwTypeError();
            return;
        }
        desc->value = o->get(ctx->d()->engine->id_value);
        attrs->setType(PropertyAttributes::Data);
    }

    if (attrs->isGeneric())
        desc->value = Primitive::emptyValue();
}


ReturnedValue ObjectPrototype::fromPropertyDescriptor(ExecutionContext *ctx, const Property *desc, PropertyAttributes attrs)
{
    if (!desc)
        return Encode::undefined();

    ExecutionEngine *engine = ctx->d()->engine;
    Scope scope(engine);
    // Let obj be the result of creating a new object as if by the expression new Object() where Object
    // is the standard built-in constructor with that name.
    ScopedObject o(scope, engine->newObject());
    ScopedString s(scope);

    Property pd;
    if (attrs.isData()) {
        pd.value = desc->value;
        s = engine->newString(QStringLiteral("value"));
        o->__defineOwnProperty__(ctx, s.getPointer(), pd, Attr_Data);
        pd.value = Primitive::fromBoolean(attrs.isWritable());
        s = engine->newString(QStringLiteral("writable"));
        o->__defineOwnProperty__(ctx, s.getPointer(), pd, Attr_Data);
    } else {
        pd.value = desc->getter() ? desc->getter()->asReturnedValue() : Encode::undefined();
        s = engine->newString(QStringLiteral("get"));
        o->__defineOwnProperty__(ctx, s.getPointer(), pd, Attr_Data);
        pd.value = desc->setter() ? desc->setter()->asReturnedValue() : Encode::undefined();
        s = engine->newString(QStringLiteral("set"));
        o->__defineOwnProperty__(ctx, s.getPointer(), pd, Attr_Data);
    }
    pd.value = Primitive::fromBoolean(attrs.isEnumerable());
    s = engine->newString(QStringLiteral("enumerable"));
    o->__defineOwnProperty__(ctx, s.getPointer(), pd, Attr_Data);
    pd.value = Primitive::fromBoolean(attrs.isConfigurable());
    s = engine->newString(QStringLiteral("configurable"));
    o->__defineOwnProperty__(ctx, s.getPointer(), pd, Attr_Data);

    return o.asReturnedValue();
}


Returned<ArrayObject> *ObjectPrototype::getOwnPropertyNames(ExecutionEngine *v4, const ValueRef o)
{
    Scope scope(v4);
    Scoped<ArrayObject> array(scope, v4->newArrayObject());
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
    return array->asReturned<ArrayObject>();
}
