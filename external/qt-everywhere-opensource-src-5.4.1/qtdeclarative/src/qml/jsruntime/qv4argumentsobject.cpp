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
#include <qv4argumentsobject_p.h>
#include <qv4alloca_p.h>
#include <qv4scopedvalue_p.h>

using namespace QV4;

DEFINE_OBJECT_VTABLE(ArgumentsObject);

ArgumentsObject::Data::Data(CallContext *context)
    : Object::Data(context->d()->strictMode ? context->d()->engine->strictArgumentsObjectClass : context->d()->engine->argumentsObjectClass)
    , context(context)
    , fullyCreated(false)
{
    Q_ASSERT(internalClass->vtable == staticVTable());

    ExecutionEngine *v4 = context->d()->engine;
    Scope scope(v4);
    Scoped<ArgumentsObject> args(scope, this);

    args->setArrayType(ArrayData::Complex);

    if (context->d()->strictMode) {
        Q_ASSERT(CalleePropertyIndex == args->internalClass()->find(context->d()->engine->id_callee));
        Q_ASSERT(CallerPropertyIndex == args->internalClass()->find(context->d()->engine->id_caller));
        args->propertyAt(CalleePropertyIndex)->value = v4->thrower;
        args->propertyAt(CalleePropertyIndex)->set = v4->thrower;
        args->propertyAt(CallerPropertyIndex)->value = v4->thrower;
        args->propertyAt(CallerPropertyIndex)->set = v4->thrower;

        args->arrayReserve(context->d()->callData->argc);
        args->arrayPut(0, context->d()->callData->args, context->d()->callData->argc);
        args->d()->fullyCreated = true;
    } else {
        args->setHasAccessorProperty();
        Q_ASSERT(CalleePropertyIndex == args->internalClass()->find(context->d()->engine->id_callee));
        args->memberData()[CalleePropertyIndex] = context->d()->function->asReturnedValue();
    }
    Q_ASSERT(LengthPropertyIndex == args->internalClass()->find(context->d()->engine->id_length));
    args->memberData()[LengthPropertyIndex] = Primitive::fromInt32(context->d()->realArgumentCount);
}

void ArgumentsObject::fullyCreate()
{
    if (fullyCreated())
        return;

    uint numAccessors = qMin((int)context()->d()->function->formalParameterCount(), context()->d()->realArgumentCount);
    uint argCount = qMin(context()->d()->realArgumentCount, context()->d()->callData->argc);
    ArrayData::realloc(this, ArrayData::Sparse, argCount, true);
    context()->d()->engine->requireArgumentsAccessors(numAccessors);
    mappedArguments().ensureIndex(engine(), numAccessors);
    for (uint i = 0; i < (uint)numAccessors; ++i) {
        mappedArguments()[i] = context()->d()->callData->args[i];
        arraySet(i, context()->d()->engine->argumentsAccessors[i], Attr_Accessor);
    }
    arrayPut(numAccessors, context()->d()->callData->args + numAccessors, argCount - numAccessors);
    for (uint i = numAccessors; i < argCount; ++i)
        setArrayAttributes(i, Attr_Data);

    d()->fullyCreated = true;
}

bool ArgumentsObject::defineOwnProperty(ExecutionContext *ctx, uint index, const Property &desc, PropertyAttributes attrs)
{
    fullyCreate();

    Scope scope(ctx);
    Property *pd = arrayData() ? arrayData()->getProperty(index) : 0;
    Property map;
    PropertyAttributes mapAttrs;
    bool isMapped = false;
    uint numAccessors = qMin((int)context()->d()->function->formalParameterCount(), context()->d()->realArgumentCount);
    if (pd && index < (uint)numAccessors)
        isMapped = arrayData()->attributes(index).isAccessor() && pd->getter() == context()->d()->engine->argumentsAccessors[index].getter();

    if (isMapped) {
        Q_ASSERT(arrayData());
        mapAttrs = arrayData()->attributes(index);
        map.copy(*pd, mapAttrs);
        setArrayAttributes(index, Attr_Data);
        pd = arrayData()->getProperty(index);
        pd->value = mappedArguments()[index];
    }

    bool strict = ctx->d()->strictMode;
    ctx->d()->strictMode = false;
    bool result = Object::defineOwnProperty2(ctx, index, desc, attrs);
    ctx->d()->strictMode = strict;

    if (isMapped && attrs.isData()) {
        Q_ASSERT(arrayData());
        ScopedCallData callData(scope, 1);
        callData->thisObject = this->asReturnedValue();
        callData->args[0] = desc.value;
        map.setter()->call(callData);

        if (attrs.isWritable()) {
            setArrayAttributes(index, mapAttrs);
            pd = arrayData()->getProperty(index);
            pd->copy(map, mapAttrs);
        }
    }

    if (ctx->d()->strictMode && !result)
        return ctx->throwTypeError();
    return result;
}

ReturnedValue ArgumentsObject::getIndexed(Managed *m, uint index, bool *hasProperty)
{
    ArgumentsObject *args = static_cast<ArgumentsObject *>(m);
    if (args->fullyCreated())
        return Object::getIndexed(m, index, hasProperty);

    if (index < static_cast<uint>(args->context()->d()->callData->argc)) {
        if (hasProperty)
            *hasProperty = true;
        return args->context()->d()->callData->args[index].asReturnedValue();
    }
    if (hasProperty)
        *hasProperty = false;
    return Encode::undefined();
}

void ArgumentsObject::putIndexed(Managed *m, uint index, const ValueRef value)
{
    ArgumentsObject *args = static_cast<ArgumentsObject *>(m);
    if (!args->fullyCreated() && index >= static_cast<uint>(args->context()->d()->callData->argc))
        args->fullyCreate();

    if (args->fullyCreated()) {
        Object::putIndexed(m, index, value);
        return;
    }

    args->context()->d()->callData->args[index] = value;
}

bool ArgumentsObject::deleteIndexedProperty(Managed *m, uint index)
{
    ArgumentsObject *args = static_cast<ArgumentsObject *>(m);
    if (!args->fullyCreated())
        args->fullyCreate();
    return Object::deleteIndexedProperty(m, index);
}

PropertyAttributes ArgumentsObject::queryIndexed(const Managed *m, uint index)
{
    const ArgumentsObject *args = static_cast<const ArgumentsObject *>(m);
    if (args->fullyCreated())
        return Object::queryIndexed(m, index);

    uint numAccessors = qMin((int)args->context()->d()->function->formalParameterCount(), args->context()->d()->realArgumentCount);
    uint argCount = qMin(args->context()->d()->realArgumentCount, args->context()->d()->callData->argc);
    if (index >= argCount)
        return PropertyAttributes();
    if (index >= numAccessors)
        return Attr_Data;
    return Attr_Accessor;
}

DEFINE_OBJECT_VTABLE(ArgumentsGetterFunction);

ReturnedValue ArgumentsGetterFunction::call(Managed *getter, CallData *callData)
{
    ExecutionEngine *v4 = getter->engine();
    Scope scope(v4);
    Scoped<ArgumentsGetterFunction> g(scope, static_cast<ArgumentsGetterFunction *>(getter));
    Scoped<ArgumentsObject> o(scope, callData->thisObject.as<ArgumentsObject>());
    if (!o)
        return v4->currentContext()->throwTypeError();

    Q_ASSERT(g->index() < static_cast<unsigned>(o->context()->d()->callData->argc));
    return o->context()->argument(g->index());
}

DEFINE_OBJECT_VTABLE(ArgumentsSetterFunction);

ReturnedValue ArgumentsSetterFunction::call(Managed *setter, CallData *callData)
{
    ExecutionEngine *v4 = setter->engine();
    Scope scope(v4);
    Scoped<ArgumentsSetterFunction> s(scope, static_cast<ArgumentsSetterFunction *>(setter));
    Scoped<ArgumentsObject> o(scope, callData->thisObject.as<ArgumentsObject>());
    if (!o)
        return v4->currentContext()->throwTypeError();

    Q_ASSERT(s->index() < static_cast<unsigned>(o->context()->d()->callData->argc));
    o->context()->d()->callData->args[s->index()] = callData->argc ? callData->args[0].asReturnedValue() : Encode::undefined();
    return Encode::undefined();
}

void ArgumentsObject::markObjects(Managed *that, ExecutionEngine *e)
{
    ArgumentsObject *o = static_cast<ArgumentsObject *>(that);
    if (o->context())
        o->context()->mark(e);
    o->mappedArguments().mark(e);

    Object::markObjects(that, e);
}
