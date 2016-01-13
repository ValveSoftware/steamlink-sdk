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

#include "qv4object_p.h"
#include "qv4jsir_p.h"
#include "qv4isel_p.h"
#include "qv4objectproto_p.h"
#include "qv4stringobject_p.h"
#include "qv4function_p.h"
#include "qv4mm_p.h"

#include "qv4arrayobject_p.h"
#include "qv4scopedvalue_p.h"

#include <private/qqmljsengine_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljsast_p.h>
#include <private/qqmlcontextwrapper_p.h>
#include <private/qqmlengine_p.h>
#include <qv4jsir_p.h>
#include <qv4codegen_p.h>
#include "private/qlocale_tools_p.h"

#include <QtCore/qmath.h>
#include <QtCore/QDebug>
#include <cassert>
#include <typeinfo>
#include <iostream>
#include <algorithm>
#include "qv4alloca_p.h"
#include "qv4profiling_p.h"

using namespace QV4;


DEFINE_OBJECT_VTABLE(FunctionObject);

FunctionObject::Data::Data(ExecutionContext *scope, String *name, bool createProto)
    : Object::Data(scope->d()->engine->functionClass)
    , scope(scope)
{
    Scope s(scope);
    ScopedFunctionObject f(s, this);
    f->init(name, createProto);
}


FunctionObject::Data::Data(ExecutionContext *scope, const QString &name, bool createProto)
    : Object::Data(scope->d()->engine->functionClass)
    , scope(scope)
{
    Scope s(scope);
    ScopedFunctionObject f(s, this);
    ScopedString n(s, s.engine->newString(name));
    f->init(n.getPointer(), createProto);
}

FunctionObject::Data::Data(ExecutionContext *scope, const ReturnedValue name)
    : Object::Data(scope->d()->engine->functionClass)
    , scope(scope)
{
    Scope s(scope);
    ScopedFunctionObject f(s, this);
    ScopedString n(s, name);
    f->init(n.getPointer(), false);
}

FunctionObject::Data::Data(InternalClass *ic)
    : Object::Data(ic)
    , scope(ic->engine->rootContext)
{
    memberData.ensureIndex(ic->engine, Index_Prototype);
    memberData[Index_Prototype] = Encode::undefined();
}


FunctionObject::Data::~Data()
{
    if (function)
        function->compilationUnit->release();
}

void FunctionObject::init(String *n, bool createProto)
{
    Scope s(internalClass()->engine);
    ScopedValue protectThis(s, this);

    d()->needsActivation = true;
    d()->strictMode = false;

    memberData().ensureIndex(s.engine, Index_Prototype);
    if (createProto) {
        Scoped<Object> proto(s, scope()->d()->engine->newObject(scope()->d()->engine->protoClass));
        proto->memberData()[Index_ProtoConstructor] = this->asReturnedValue();
        memberData()[Index_Prototype] = proto.asReturnedValue();
    } else {
        memberData()[Index_Prototype] = Encode::undefined();
    }

    ScopedValue v(s, n);
    defineReadonlyProperty(s.engine->id_name, v);
}

ReturnedValue FunctionObject::name()
{
    return get(scope()->d()->engine->id_name);
}


ReturnedValue FunctionObject::newInstance()
{
    Scope scope(internalClass()->engine);
    ScopedCallData callData(scope, 0);
    return construct(callData);
}

ReturnedValue FunctionObject::construct(Managed *that, CallData *)
{
    that->internalClass()->engine->currentContext()->throwTypeError();
    return Encode::undefined();
}

ReturnedValue FunctionObject::call(Managed *, CallData *)
{
    return Encode::undefined();
}

void FunctionObject::markObjects(Managed *that, ExecutionEngine *e)
{
    FunctionObject *o = static_cast<FunctionObject *>(that);
    if (o->scope())
        o->scope()->mark(e);

    Object::markObjects(that, e);
}

FunctionObject *FunctionObject::createScriptFunction(ExecutionContext *scope, Function *function, bool createProto)
{
    if (function->needsActivation() ||
        function->compiledFunction->flags & CompiledData::Function::HasCatchOrWith ||
        function->compiledFunction->nFormals > QV4::Global::ReservedArgumentCount ||
        function->isNamedExpression())
        return scope->d()->engine->memoryManager->alloc<ScriptFunction>(scope, function);
    return scope->d()->engine->memoryManager->alloc<SimpleScriptFunction>(scope, function, createProto);
}

DEFINE_OBJECT_VTABLE(FunctionCtor);

FunctionCtor::Data::Data(ExecutionContext *scope)
    : FunctionObject::Data(scope, QStringLiteral("Function"))
{
    setVTable(staticVTable());
}

// 15.3.2
ReturnedValue FunctionCtor::construct(Managed *that, CallData *callData)
{
    FunctionCtor *f = static_cast<FunctionCtor *>(that);
    ExecutionEngine *v4 = f->internalClass()->engine;
    ExecutionContext *ctx = v4->currentContext();
    QString arguments;
    QString body;
    if (callData->argc > 0) {
        for (int i = 0; i < callData->argc - 1; ++i) {
            if (i)
                arguments += QLatin1String(", ");
            arguments += callData->args[i].toString(ctx)->toQString();
        }
        body = callData->args[callData->argc - 1].toString(ctx)->toQString();
    }
    if (ctx->d()->engine->hasException)
        return Encode::undefined();

    QString function = QLatin1String("function(") + arguments + QLatin1String("){") + body + QLatin1String("}");

    QQmlJS::Engine ee, *engine = &ee;
    QQmlJS::Lexer lexer(engine);
    lexer.setCode(function, 1, false);
    QQmlJS::Parser parser(engine);

    const bool parsed = parser.parseExpression();

    if (!parsed)
        return v4->currentContext()->throwSyntaxError(QLatin1String("Parse error"));

    using namespace QQmlJS::AST;
    FunctionExpression *fe = QQmlJS::AST::cast<FunctionExpression *>(parser.rootNode());
    if (!fe)
        return v4->currentContext()->throwSyntaxError(QLatin1String("Parse error"));

    IR::Module module(v4->debugger != 0);

    QQmlJS::RuntimeCodegen cg(v4->currentContext(), f->strictMode());
    cg.generateFromFunctionExpression(QString(), function, fe, &module);

    QV4::Compiler::JSUnitGenerator jsGenerator(&module);
    QScopedPointer<EvalInstructionSelection> isel(v4->iselFactory->create(QQmlEnginePrivate::get(v4), v4->executableAllocator, &module, &jsGenerator));
    QQmlRefPointer<QV4::CompiledData::CompilationUnit> compilationUnit = isel->compile();
    QV4::Function *vmf = compilationUnit->linkToEngine(v4);

    return FunctionObject::createScriptFunction(v4->rootContext, vmf)->asReturnedValue();
}

// 15.3.1: This is equivalent to new Function(...)
ReturnedValue FunctionCtor::call(Managed *that, CallData *callData)
{
    return construct(that, callData);
}

DEFINE_OBJECT_VTABLE(FunctionPrototype);

FunctionPrototype::Data::Data(InternalClass *ic)
    : FunctionObject::Data(ic)
{
}

void FunctionPrototype::init(ExecutionEngine *engine, Object *ctor)
{
    Scope scope(engine);
    ScopedObject o(scope);

    ctor->defineReadonlyProperty(engine->id_length, Primitive::fromInt32(1));
    ctor->defineReadonlyProperty(engine->id_prototype, (o = this));

    defineReadonlyProperty(engine->id_length, Primitive::fromInt32(0));
    defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    defineDefaultProperty(engine->id_toString, method_toString, 0);
    defineDefaultProperty(QStringLiteral("apply"), method_apply, 2);
    defineDefaultProperty(QStringLiteral("call"), method_call, 1);
    defineDefaultProperty(QStringLiteral("bind"), method_bind, 1);

}

ReturnedValue FunctionPrototype::method_toString(CallContext *ctx)
{
    FunctionObject *fun = ctx->d()->callData->thisObject.asFunctionObject();
    if (!fun)
        return ctx->throwTypeError();

    return ctx->d()->engine->newString(QStringLiteral("function() { [code] }"))->asReturnedValue();
}

ReturnedValue FunctionPrototype::method_apply(CallContext *ctx)
{
    Scope scope(ctx);
    FunctionObject *o = ctx->d()->callData->thisObject.asFunctionObject();
    if (!o)
        return ctx->throwTypeError();

    ScopedValue arg(scope, ctx->argument(1));

    ScopedObject arr(scope, arg);

    quint32 len;
    if (!arr) {
        len = 0;
        if (!arg->isNullOrUndefined())
            return ctx->throwTypeError();
    } else {
        len = arr->getLength();
    }

    ScopedCallData callData(scope, len);

    if (len) {
        if (arr->arrayType() != ArrayData::Simple || arr->protoHasArray() || arr->hasAccessorProperty()) {
            for (quint32 i = 0; i < len; ++i)
                callData->args[i] = arr->getIndexed(i);
        } else {
            uint alen = arr->arrayData() ? arr->arrayData()->length() : 0;
            if (alen > len)
                alen = len;
            for (uint i = 0; i < alen; ++i)
                callData->args[i] = static_cast<SimpleArrayData *>(arr->arrayData())->data(i);
            for (quint32 i = alen; i < len; ++i)
                callData->args[i] = Primitive::undefinedValue();
        }
    }

    callData->thisObject = ctx->argument(0);
    return o->call(callData);
}

ReturnedValue FunctionPrototype::method_call(CallContext *ctx)
{
    Scope scope(ctx);

    FunctionObject *o = ctx->d()->callData->thisObject.asFunctionObject();
    if (!o)
        return ctx->throwTypeError();

    ScopedCallData callData(scope, ctx->d()->callData->argc ? ctx->d()->callData->argc - 1 : 0);
    if (ctx->d()->callData->argc) {
        for (int i = 1; i < ctx->d()->callData->argc; ++i)
            callData->args[i - 1] = ctx->d()->callData->args[i];
    }
    callData->thisObject = ctx->argument(0);
    return o->call(callData);
}

ReturnedValue FunctionPrototype::method_bind(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<FunctionObject> target(scope, ctx->d()->callData->thisObject);
    if (!target)
        return ctx->throwTypeError();

    ScopedValue boundThis(scope, ctx->argument(0));
    Members boundArgs;
    boundArgs.reset();
    if (ctx->d()->callData->argc > 1) {
        boundArgs.ensureIndex(scope.engine, ctx->d()->callData->argc - 1);
        boundArgs.d()->d()->size = ctx->d()->callData->argc - 1;
        memcpy(boundArgs.data(), ctx->d()->callData->args + 1, (ctx->d()->callData->argc - 1)*sizeof(Value));
    }
    ScopedValue protectBoundArgs(scope, boundArgs.d());

    return BoundFunction::create(ctx->d()->engine->rootContext, target, boundThis, boundArgs)->asReturnedValue();
}

DEFINE_OBJECT_VTABLE(ScriptFunction);

ScriptFunction::Data::Data(ExecutionContext *scope, Function *function)
    : SimpleScriptFunction::Data(scope, function, true)
{
    setVTable(staticVTable());
}

ReturnedValue ScriptFunction::construct(Managed *that, CallData *callData)
{
    ExecutionEngine *v4 = that->engine();
    if (v4->hasException)
        return Encode::undefined();
    CHECK_STACK_LIMITS(v4);

    Scope scope(v4);
    Scoped<ScriptFunction> f(scope, static_cast<ScriptFunction *>(that));

    InternalClass *ic = f->internalClassForConstructor();
    ScopedObject obj(scope, v4->newObject(ic));

    ExecutionContext *context = v4->currentContext();
    callData->thisObject = obj.asReturnedValue();
    ExecutionContext *ctx = reinterpret_cast<ExecutionContext *>(context->newCallContext(f.getPointer(), callData));

    ExecutionContextSaver ctxSaver(context);
    ScopedValue result(scope, Q_V4_PROFILE(v4, ctx, f->function()));

    if (f->function()->compiledFunction->hasQmlDependencies())
        QmlContextWrapper::registerQmlDependencies(v4, f->function()->compiledFunction);

    if (v4->hasException)
        return Encode::undefined();

    if (result->isObject())
        return result.asReturnedValue();
    return obj.asReturnedValue();
}

ReturnedValue ScriptFunction::call(Managed *that, CallData *callData)
{
    ScriptFunction *f = static_cast<ScriptFunction *>(that);
    ExecutionEngine *v4 = f->engine();
    if (v4->hasException)
        return Encode::undefined();
    CHECK_STACK_LIMITS(v4);

    ExecutionContext *context = v4->currentContext();
    Scope scope(context);

    CallContext *ctx = reinterpret_cast<CallContext *>(context->newCallContext(f, callData));

    ExecutionContextSaver ctxSaver(context);
    ScopedValue result(scope, Q_V4_PROFILE(v4, ctx, f->function()));

    if (f->function()->compiledFunction->hasQmlDependencies())
        QmlContextWrapper::registerQmlDependencies(ctx->d()->engine, f->function()->compiledFunction);

    return result.asReturnedValue();
}

DEFINE_OBJECT_VTABLE(SimpleScriptFunction);

SimpleScriptFunction::Data::Data(ExecutionContext *scope, Function *function, bool createProto)
    : FunctionObject::Data(scope, function->name(), createProto)
{
    setVTable(staticVTable());

    this->function = function;
    function->compilationUnit->addref();
    Q_ASSERT(function);
    Q_ASSERT(function->code);

    needsActivation = function->needsActivation();
    strictMode = function->isStrict();

    // global function
    if (!scope)
        return;

    Scope s(scope);
    ScopedFunctionObject f(s, this);

    f->defineReadonlyProperty(scope->d()->engine->id_length, Primitive::fromInt32(f->formalParameterCount()));

    if (scope->d()->strictMode) {
        Property pd(s.engine->thrower, s.engine->thrower);
        f->insertMember(scope->d()->engine->id_caller, pd, Attr_Accessor|Attr_NotConfigurable|Attr_NotEnumerable);
        f->insertMember(scope->d()->engine->id_arguments, pd, Attr_Accessor|Attr_NotConfigurable|Attr_NotEnumerable);
    }
}

ReturnedValue SimpleScriptFunction::construct(Managed *that, CallData *callData)
{
    ExecutionEngine *v4 = that->engine();
    if (v4->hasException)
        return Encode::undefined();
    CHECK_STACK_LIMITS(v4);

    Scope scope(v4);
    Scoped<SimpleScriptFunction> f(scope, static_cast<SimpleScriptFunction *>(that));

    InternalClass *ic = f->internalClassForConstructor();
    callData->thisObject = v4->newObject(ic);

    ExecutionContext *context = v4->currentContext();
    ExecutionContextSaver ctxSaver(context);

    CallContext::Data ctx(v4);
    ctx.strictMode = f->strictMode();
    ctx.callData = callData;
    ctx.function = f.getPointer();
    ctx.compilationUnit = f->function()->compilationUnit;
    ctx.lookups = ctx.compilationUnit->runtimeLookups;
    ctx.outer = f->scope();
    ctx.locals = v4->stackPush(f->varCount());
    while (callData->argc < (int)f->formalParameterCount()) {
        callData->args[callData->argc] = Encode::undefined();
        ++callData->argc;
    }
    Q_ASSERT(v4->currentContext()->d() == &ctx);

    Scoped<Object> result(scope, Q_V4_PROFILE(v4, reinterpret_cast<CallContext *>(&ctx), f->function()));

    if (f->function()->compiledFunction->hasQmlDependencies())
        QmlContextWrapper::registerQmlDependencies(v4, f->function()->compiledFunction);

    if (!result)
        return callData->thisObject.asReturnedValue();
    return result.asReturnedValue();
}

ReturnedValue SimpleScriptFunction::call(Managed *that, CallData *callData)
{
    ExecutionEngine *v4 = that->internalClass()->engine;
    if (v4->hasException)
        return Encode::undefined();
    CHECK_STACK_LIMITS(v4);

    SimpleScriptFunction *f = static_cast<SimpleScriptFunction *>(that);

    Scope scope(v4);
    ExecutionContext *context = v4->currentContext();
    ExecutionContextSaver ctxSaver(context);

    CallContext::Data ctx(v4);
    ctx.strictMode = f->strictMode();
    ctx.callData = callData;
    ctx.function = f;
    ctx.compilationUnit = f->function()->compilationUnit;
    ctx.lookups = ctx.compilationUnit->runtimeLookups;
    ctx.outer = f->scope();
    ctx.locals = v4->stackPush(f->varCount());
    while (callData->argc < (int)f->formalParameterCount()) {
        callData->args[callData->argc] = Encode::undefined();
        ++callData->argc;
    }
    Q_ASSERT(v4->currentContext()->d() == &ctx);

    ScopedValue result(scope, Q_V4_PROFILE(v4, reinterpret_cast<CallContext *>(&ctx), f->function()));

    if (f->function()->compiledFunction->hasQmlDependencies())
        QmlContextWrapper::registerQmlDependencies(v4, f->function()->compiledFunction);

    return result.asReturnedValue();
}

InternalClass *SimpleScriptFunction::internalClassForConstructor()
{
    ReturnedValue proto = protoProperty();
    InternalClass *classForConstructor;
    Scope scope(internalClass()->engine);
    ScopedObject p(scope, proto);
    if (p)
        classForConstructor = internalClass()->engine->constructClass->changePrototype(p.getPointer());
    else
        classForConstructor = scope.engine->objectClass;

    return classForConstructor;
}



DEFINE_OBJECT_VTABLE(BuiltinFunction);

BuiltinFunction::Data::Data(ExecutionContext *scope, String *name, ReturnedValue (*code)(CallContext *))
    : FunctionObject::Data(scope, name)
    , code(code)
{
    setVTable(staticVTable());
}

ReturnedValue BuiltinFunction::construct(Managed *f, CallData *)
{
    return f->internalClass()->engine->currentContext()->throwTypeError();
}

ReturnedValue BuiltinFunction::call(Managed *that, CallData *callData)
{
    BuiltinFunction *f = static_cast<BuiltinFunction *>(that);
    ExecutionEngine *v4 = f->internalClass()->engine;
    if (v4->hasException)
        return Encode::undefined();
    CHECK_STACK_LIMITS(v4);

    ExecutionContext *context = v4->currentContext();
    ExecutionContextSaver ctxSaver(context);

    CallContext::Data ctx(v4);
    ctx.strictMode = f->scope()->d()->strictMode; // ### needed? scope or parent context?
    ctx.callData = callData;
    Q_ASSERT(v4->currentContext()->d() == &ctx);

    return f->d()->code(reinterpret_cast<CallContext *>(&ctx));
}

ReturnedValue IndexedBuiltinFunction::call(Managed *that, CallData *callData)
{
    IndexedBuiltinFunction *f = static_cast<IndexedBuiltinFunction *>(that);
    ExecutionEngine *v4 = f->internalClass()->engine;
    if (v4->hasException)
        return Encode::undefined();
    CHECK_STACK_LIMITS(v4);

    ExecutionContext *context = v4->currentContext();
    ExecutionContextSaver ctxSaver(context);

    CallContext::Data ctx(v4);
    ctx.strictMode = f->scope()->d()->strictMode; // ### needed? scope or parent context?
    ctx.callData = callData;
    Q_ASSERT(v4->currentContext()->d() == &ctx);

    return f->d()->code(reinterpret_cast<CallContext *>(&ctx), f->d()->index);
}

DEFINE_OBJECT_VTABLE(IndexedBuiltinFunction);

DEFINE_OBJECT_VTABLE(BoundFunction);

BoundFunction::Data::Data(ExecutionContext *scope, FunctionObject *target, const ValueRef boundThis, const Members &boundArgs)
    : FunctionObject::Data(scope, QStringLiteral("__bound function__"))
    , target(target)
    , boundArgs(boundArgs)
{
    this->boundThis = boundThis;
    setVTable(staticVTable());
    subtype = FunctionObject::BoundFunction;

    Scope s(scope);
    ScopedObject f(s, this);

    ScopedValue l(s, target->get(s.engine->id_length));
    int len = l->toUInt32();
    len -= boundArgs.size();
    if (len < 0)
        len = 0;
    f->defineReadonlyProperty(s.engine->id_length, Primitive::fromInt32(len));

    ExecutionEngine *v4 = s.engine;

    Property pd(v4->thrower, v4->thrower);
    f->insertMember(s.engine->id_arguments, pd, Attr_Accessor|Attr_NotConfigurable|Attr_NotEnumerable);
    f->insertMember(s.engine->id_caller, pd, Attr_Accessor|Attr_NotConfigurable|Attr_NotEnumerable);
}

ReturnedValue BoundFunction::call(Managed *that, CallData *dd)
{
    BoundFunction *f = static_cast<BoundFunction *>(that);
    Scope scope(f->scope()->d()->engine);
    if (scope.hasException())
        return Encode::undefined();

    ScopedCallData callData(scope, f->boundArgs().size() + dd->argc);
    callData->thisObject = f->boundThis();
    memcpy(callData->args, f->boundArgs().data(), f->boundArgs().size()*sizeof(Value));
    memcpy(callData->args + f->boundArgs().size(), dd->args, dd->argc*sizeof(Value));
    return f->target()->call(callData);
}

ReturnedValue BoundFunction::construct(Managed *that, CallData *dd)
{
    BoundFunction *f = static_cast<BoundFunction *>(that);
    Scope scope(f->scope()->d()->engine);
    if (scope.hasException())
        return Encode::undefined();

    ScopedCallData callData(scope, f->boundArgs().size() + dd->argc);
    memcpy(callData->args, f->boundArgs().data(), f->boundArgs().size()*sizeof(Value));
    memcpy(callData->args + f->boundArgs().size(), dd->args, dd->argc*sizeof(Value));
    return f->target()->construct(callData);
}

void BoundFunction::markObjects(Managed *that, ExecutionEngine *e)
{
    BoundFunction *o = static_cast<BoundFunction *>(that);
    o->target()->mark(e);
    o->boundThis().mark(e);
    o->boundArgs().mark(e);
    FunctionObject::markObjects(that, e);
}
