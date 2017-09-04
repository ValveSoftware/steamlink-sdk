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

#include "qv4object_p.h"
#include "qv4jsir_p.h"
#include "qv4isel_p.h"
#include "qv4objectproto_p.h"
#include "qv4stringobject_p.h"
#include "qv4function_p.h"
#include <private/qv4mm_p.h>

#include "qv4arrayobject_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4argumentsobject_p.h"

#include <private/qqmljsengine_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljsast_p.h>
#include <private/qqmljavascriptexpression_p.h>
#include <private/qqmlengine_p.h>
#include <qv4codegen_p.h>
#include "private/qlocale_tools_p.h"
#include "private/qqmlbuiltinfunctions_p.h"

#include <QtCore/QDebug>
#include <algorithm>
#include "qv4alloca_p.h"
#include "qv4profiling_p.h"

using namespace QV4;


DEFINE_OBJECT_VTABLE(FunctionObject);

void Heap::FunctionObject::init(QV4::ExecutionContext *scope, QV4::String *name, bool createProto)
{
    Object::init();
    function = nullptr;
    this->scope = scope->d();
    Scope s(scope->engine());
    ScopedFunctionObject f(s, this);
    f->init(name, createProto);
}

void Heap::FunctionObject::init(QV4::ExecutionContext *scope, Function *function, bool createProto)
{
    Object::init();
    this->function = function;
    function->compilationUnit->addref();
    this->scope = scope->d();
    Scope s(scope->engine());
    ScopedString name(s, function->name());
    ScopedFunctionObject f(s, this);
    f->init(name, createProto);
}

void Heap::FunctionObject::init(QV4::ExecutionContext *scope, const QString &name, bool createProto)
{
    Scope valueScope(scope);
    ScopedString s(valueScope, valueScope.engine->newString(name));
    init(scope, s, createProto);
}

void Heap::FunctionObject::init()
{
    Object::init();
    function = nullptr;
    this->scope = internalClass->engine->rootContext()->d();
    Q_ASSERT(internalClass && internalClass->find(internalClass->engine->id_prototype()) == Index_Prototype);
    *propertyData(Index_Prototype) = Encode::undefined();
}


void Heap::FunctionObject::destroy()
{
    if (function)
        function->compilationUnit->release();
    Object::destroy();
}

void FunctionObject::init(String *n, bool createProto)
{
    Scope s(internalClass()->engine);
    ScopedValue protectThis(s, this);

    Q_ASSERT(internalClass() && internalClass()->find(s.engine->id_prototype()) == Heap::FunctionObject::Index_Prototype);
    if (createProto) {
        ScopedObject proto(s, s.engine->newObject(s.engine->internalClasses[EngineBase::Class_ObjectProto], s.engine->objectPrototype()));
        Q_ASSERT(s.engine->internalClasses[EngineBase::Class_ObjectProto]->find(s.engine->id_constructor()) == Heap::FunctionObject::Index_ProtoConstructor);
        *proto->propertyData(Heap::FunctionObject::Index_ProtoConstructor) = this->asReturnedValue();
        *propertyData(Heap::FunctionObject::Index_Prototype) = proto.asReturnedValue();
    } else {
        *propertyData(Heap::FunctionObject::Index_Prototype) = Encode::undefined();
    }

    if (n)
        defineReadonlyProperty(s.engine->id_name(), *n);
}

ReturnedValue FunctionObject::name() const
{
    return get(scope()->internalClass->engine->id_name());
}

void FunctionObject::construct(const Managed *that, Scope &scope, CallData *)
{
    scope.result = static_cast<const FunctionObject *>(that)->engine()->throwTypeError();
}

void FunctionObject::call(const Managed *, Scope &scope, CallData *)
{
    scope.result = Encode::undefined();
}

void FunctionObject::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    Heap::FunctionObject *o = static_cast<Heap::FunctionObject *>(that);
    if (o->scope)
        o->scope->mark(e);

    Object::markObjects(that, e);
}

Heap::FunctionObject *FunctionObject::createScriptFunction(ExecutionContext *scope, Function *function)
{
    return scope->engine()->memoryManager->allocObject<ScriptFunction>(scope, function);
}

bool FunctionObject::isBinding() const
{
    return d()->vtable() == QQmlBindingFunction::staticVTable();
}

bool FunctionObject::isBoundFunction() const
{
    return d()->vtable() == BoundFunction::staticVTable();
}

QQmlSourceLocation FunctionObject::sourceLocation() const
{
    return d()->function->sourceLocation();
}

DEFINE_OBJECT_VTABLE(FunctionCtor);

void Heap::FunctionCtor::init(QV4::ExecutionContext *scope)
{
    Heap::FunctionObject::init(scope, QStringLiteral("Function"));
}

// 15.3.2
void FunctionCtor::construct(const Managed *that, Scope &scope, CallData *callData)
{
    Scoped<FunctionCtor> f(scope, static_cast<const FunctionCtor *>(that));

    QString arguments;
    QString body;
    if (callData->argc > 0) {
        for (int i = 0; i < callData->argc - 1; ++i) {
            if (i)
                arguments += QLatin1String(", ");
            arguments += callData->args[i].toQString();
        }
        body = callData->args[callData->argc - 1].toQString();
    }
    if (scope.engine->hasException) {
        scope.result = Encode::undefined();
        return;
    }

    QString function = QLatin1String("function(") + arguments + QLatin1String("){") + body + QLatin1Char('}');

    QQmlJS::Engine ee, *engine = &ee;
    QQmlJS::Lexer lexer(engine);
    lexer.setCode(function, 1, false);
    QQmlJS::Parser parser(engine);

    const bool parsed = parser.parseExpression();

    if (!parsed) {
        scope.result = scope.engine->throwSyntaxError(QLatin1String("Parse error"));
        return;
    }

    using namespace QQmlJS::AST;
    FunctionExpression *fe = QQmlJS::AST::cast<FunctionExpression *>(parser.rootNode());
    if (!fe) {
        scope.result = scope.engine->throwSyntaxError(QLatin1String("Parse error"));
        return;
    }

    IR::Module module(scope.engine->debugger() != 0);

    QQmlJS::RuntimeCodegen cg(scope.engine, f->strictMode());
    cg.generateFromFunctionExpression(QString(), function, fe, &module);

    Compiler::JSUnitGenerator jsGenerator(&module);
    QScopedPointer<EvalInstructionSelection> isel(scope.engine->iselFactory->create(QQmlEnginePrivate::get(scope.engine), scope.engine->executableAllocator, &module, &jsGenerator));
    QQmlRefPointer<CompiledData::CompilationUnit> compilationUnit = isel->compile();
    Function *vmf = compilationUnit->linkToEngine(scope.engine);

    ExecutionContext *global = scope.engine->rootContext();
    scope.result = FunctionObject::createScriptFunction(global, vmf);
}

// 15.3.1: This is equivalent to new Function(...)
void FunctionCtor::call(const Managed *that, Scope &scope, CallData *callData)
{
    construct(that, scope, callData);
}

DEFINE_OBJECT_VTABLE(FunctionPrototype);

void Heap::FunctionPrototype::init()
{
    Heap::FunctionObject::init();
}

void FunctionPrototype::init(ExecutionEngine *engine, Object *ctor)
{
    Scope scope(engine);
    ScopedObject o(scope);

    ctor->defineReadonlyProperty(engine->id_length(), Primitive::fromInt32(1));
    ctor->defineReadonlyProperty(engine->id_prototype(), (o = this));

    defineReadonlyProperty(engine->id_length(), Primitive::fromInt32(0));
    defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    defineDefaultProperty(engine->id_toString(), method_toString, 0);
    defineDefaultProperty(QStringLiteral("apply"), method_apply, 2);
    defineDefaultProperty(QStringLiteral("call"), method_call, 1);
    defineDefaultProperty(QStringLiteral("bind"), method_bind, 1);

}

void FunctionPrototype::method_toString(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    FunctionObject *fun = callData->thisObject.as<FunctionObject>();
    if (!fun)
        THROW_TYPE_ERROR();

    scope.result = scope.engine->newString(QStringLiteral("function() { [code] }"));
}

void FunctionPrototype::method_apply(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    FunctionObject *o = callData->thisObject.as<FunctionObject>();
    if (!o)
        THROW_TYPE_ERROR();

    ScopedValue arg(scope, callData->argument(1));

    ScopedObject arr(scope, arg);

    quint32 len;
    if (!arr) {
        len = 0;
        if (!arg->isNullOrUndefined())
            THROW_TYPE_ERROR();
    } else {
        len = arr->getLength();
    }

    ScopedCallData cData(scope, len);

    if (len) {
        if (ArgumentsObject::isNonStrictArgumentsObject(arr) && !arr->cast<ArgumentsObject>()->fullyCreated()) {
            QV4::ArgumentsObject *a = arr->cast<ArgumentsObject>();
            int l = qMin(len, (uint)a->d()->context->callData->argc);
            memcpy(cData->args, a->d()->context->callData->args, l*sizeof(Value));
            for (quint32 i = l; i < len; ++i)
                cData->args[i] = Primitive::undefinedValue();
        } else if (arr->arrayType() == Heap::ArrayData::Simple && !arr->protoHasArray()) {
            auto sad = static_cast<Heap::SimpleArrayData *>(arr->arrayData());
            uint alen = sad ? sad->len : 0;
            if (alen > len)
                alen = len;
            for (uint i = 0; i < alen; ++i)
                cData->args[i] = sad->data(i);
            for (quint32 i = alen; i < len; ++i)
                cData->args[i] = Primitive::undefinedValue();
        } else {
            for (quint32 i = 0; i < len; ++i)
                cData->args[i] = arr->getIndexed(i);
        }
    }

    cData->thisObject = callData->argument(0);
    o->call(scope, cData);
}

void FunctionPrototype::method_call(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    FunctionObject *o = callData->thisObject.as<FunctionObject>();
    if (!o)
        THROW_TYPE_ERROR();

    ScopedCallData cData(scope, callData->argc ? callData->argc - 1 : 0);
    if (callData->argc) {
        for (int i = 1; i < callData->argc; ++i)
            cData->args[i - 1] = callData->args[i];
    }
    cData->thisObject = callData->argument(0);

    o->call(scope, cData);
}

void FunctionPrototype::method_bind(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    FunctionObject *target = callData->thisObject.as<FunctionObject>();
    if (!target)
        THROW_TYPE_ERROR();

    ScopedValue boundThis(scope, callData->argument(0));
    Scoped<MemberData> boundArgs(scope, (Heap::MemberData *)0);
    if (callData->argc > 1) {
        boundArgs = MemberData::allocate(scope.engine, callData->argc - 1);
        boundArgs->d()->size = callData->argc - 1;
        memcpy(boundArgs->data(), callData->args + 1, (callData->argc - 1)*sizeof(Value));
    }

    ExecutionContext *global = scope.engine->rootContext();
    scope.result = BoundFunction::create(global, target, boundThis, boundArgs);
}

DEFINE_OBJECT_VTABLE(ScriptFunction);

void ScriptFunction::construct(const Managed *that, Scope &scope, CallData *callData)
{
    ExecutionEngine *v4 = scope.engine;
    if (Q_UNLIKELY(v4->hasException)) {
        scope.result = Encode::undefined();
        return;
    }
    CHECK_STACK_LIMITS(v4, scope);

    ExecutionContextSaver ctxSaver(scope);

    Scoped<ScriptFunction> f(scope, static_cast<const ScriptFunction *>(that));

    InternalClass *ic = f->classForConstructor();
    ScopedObject proto(scope, ic->prototype);
    ScopedObject obj(scope, v4->newObject(ic, proto));
    callData->thisObject = obj.asReturnedValue();

    QV4::Function *v4Function = f->function();
    Q_ASSERT(v4Function);

    ScopedContext c(scope, f->scope());
    if (v4Function->canUseSimpleCall)
        c->simpleCall(scope, callData, v4Function);
    else
        c->call(scope, callData, v4Function, f);

    if (Q_UNLIKELY(v4->hasException)) {
        scope.result = Encode::undefined();
    } else if (!scope.result.isObject()) {
        scope.result = obj.asReturnedValue();
    }
}

void ScriptFunction::call(const Managed *that, Scope &scope, CallData *callData)
{
    ExecutionEngine *v4 = scope.engine;
    if (Q_UNLIKELY(v4->hasException)) {
        scope.result = Encode::undefined();
        return;
    }
    CHECK_STACK_LIMITS(v4, scope);

    Scoped<ScriptFunction> f(scope, static_cast<const ScriptFunction *>(that));

    QV4::Function *v4Function = f->function();
    Q_ASSERT(v4Function);

    ScopedContext c(scope, f->scope());
    if (v4Function->canUseSimpleCall)
        c->simpleCall(scope, callData, v4Function);
    else
        c->call(scope, callData, v4Function, f);
}

void Heap::ScriptFunction::init(QV4::ExecutionContext *scope, Function *function)
{
    FunctionObject::init();
    this->scope = scope->d();

    this->function = function;
    function->compilationUnit->addref();
    Q_ASSERT(function);
    Q_ASSERT(function->code);

    Scope s(scope);
    ScopedFunctionObject f(s, this);

    ScopedString name(s, function->name());
    f->init(name, true);
    Q_ASSERT(internalClass && internalClass->find(s.engine->id_length()) == Index_Length);
    *propertyData(Index_Length) = Primitive::fromInt32(f->formalParameterCount());

    if (scope->d()->strictMode) {
        ScopedProperty pd(s);
        pd->value = s.engine->thrower();
        pd->set = s.engine->thrower();
        f->insertMember(s.engine->id_caller(), pd, Attr_Accessor|Attr_NotConfigurable|Attr_NotEnumerable);
        f->insertMember(s.engine->id_arguments(), pd, Attr_Accessor|Attr_NotConfigurable|Attr_NotEnumerable);
    }
}

InternalClass *ScriptFunction::classForConstructor() const
{
    const Object *o = d()->protoProperty();
    InternalClass *ic = d()->cachedClassForConstructor;
    if (ic && ic->prototype == o->d())
        return ic;

    ic = engine()->internalClasses[EngineBase::Class_Object];
    if (o)
        ic = ic->changePrototype(o->d());
    d()->cachedClassForConstructor = ic;

    return ic;
}


DEFINE_OBJECT_VTABLE(BuiltinFunction);

void Heap::BuiltinFunction::init(QV4::ExecutionContext *scope, QV4::String *name, void (*code)(const QV4::BuiltinFunction *, Scope &, CallData *))
{
    Heap::FunctionObject::init(scope, name);
    this->code = code;
}

void BuiltinFunction::construct(const Managed *f, Scope &scope, CallData *)
{
    scope.result = static_cast<const BuiltinFunction *>(f)->internalClass()->engine->throwTypeError();
}

void BuiltinFunction::call(const Managed *that, Scope &scope, CallData *callData)
{
    const BuiltinFunction *f = static_cast<const BuiltinFunction *>(that);
    ExecutionEngine *v4 = scope.engine;
    if (v4->hasException) {
        scope.result = Encode::undefined();
        return;
    }
    f->d()->code(f, scope, callData);
}


void IndexedBuiltinFunction::call(const Managed *that, Scope &scope, CallData *callData)
{
    const IndexedBuiltinFunction *f = static_cast<const IndexedBuiltinFunction *>(that);
    ExecutionEngine *v4 = scope.engine;
    if (v4->hasException) {
        scope.result = Encode::undefined();
        return;
    }
    CHECK_STACK_LIMITS(v4, scope);

    ExecutionContextSaver ctxSaver(scope);

    CallContext::Data *ctx = v4->memoryManager->allocSimpleCallContext();
    ctx->strictMode = f->scope()->strictMode; // ### needed? scope or parent context?
    ctx->callData = callData;
    v4->pushContext(ctx);
    Q_ASSERT(v4->current == ctx);

    scope.result = f->d()->code(static_cast<QV4::CallContext *>(v4->currentContext), f->d()->index);
    v4->memoryManager->freeSimpleCallContext();
}

DEFINE_OBJECT_VTABLE(IndexedBuiltinFunction);

DEFINE_OBJECT_VTABLE(BoundFunction);

void Heap::BoundFunction::init(QV4::ExecutionContext *scope, QV4::FunctionObject *target,
                               const Value &boundThis, QV4::MemberData *boundArgs)
{
    Heap::FunctionObject::init(scope, QStringLiteral("__bound function__"));
    this->target = target->d();
    this->boundArgs = boundArgs ? boundArgs->d() : 0;
    this->boundThis = boundThis;

    Scope s(scope);
    ScopedObject f(s, this);

    ScopedValue l(s, target->get(s.engine->id_length()));
    int len = l->toUInt32();
    if (boundArgs)
        len -= boundArgs->size();
    if (len < 0)
        len = 0;
    f->defineReadonlyProperty(s.engine->id_length(), Primitive::fromInt32(len));

    ScopedProperty pd(s);
    pd->value = s.engine->thrower();
    pd->set = s.engine->thrower();
    f->insertMember(s.engine->id_arguments(), pd, Attr_Accessor|Attr_NotConfigurable|Attr_NotEnumerable);
    f->insertMember(s.engine->id_caller(), pd, Attr_Accessor|Attr_NotConfigurable|Attr_NotEnumerable);
}

void BoundFunction::call(const Managed *that, Scope &scope, CallData *dd)
{
    const BoundFunction *f = static_cast<const BoundFunction *>(that);
    if (scope.hasException()) {
        scope.result = Encode::undefined();
        return;
    }

    Scoped<MemberData> boundArgs(scope, f->boundArgs());
    ScopedCallData callData(scope, (boundArgs ? boundArgs->size() : 0) + dd->argc);
    callData->thisObject = f->boundThis();
    Value *argp = callData->args;
    if (boundArgs) {
        memcpy(argp, boundArgs->data(), boundArgs->size()*sizeof(Value));
        argp += boundArgs->size();
    }
    memcpy(argp, dd->args, dd->argc*sizeof(Value));
    ScopedFunctionObject t(scope, f->target());
    t->call(scope, callData);
}

void BoundFunction::construct(const Managed *that, Scope &scope, CallData *dd)
{
    const BoundFunction *f = static_cast<const BoundFunction *>(that);
    if (scope.hasException()) {
        scope.result = Encode::undefined();
        return;
    }

    Scoped<MemberData> boundArgs(scope, f->boundArgs());
    ScopedCallData callData(scope, (boundArgs ? boundArgs->size() : 0) + dd->argc);
    Value *argp = callData->args;
    if (boundArgs) {
        memcpy(argp, boundArgs->data(), boundArgs->size()*sizeof(Value));
        argp += boundArgs->size();
    }
    memcpy(argp, dd->args, dd->argc*sizeof(Value));
    ScopedFunctionObject t(scope, f->target());
    t->construct(scope, callData);
}

void BoundFunction::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    BoundFunction::Data *o = static_cast<BoundFunction::Data *>(that);
    if (o->target)
        o->target->mark(e);
    o->boundThis.mark(e);
    if (o->boundArgs)
        o->boundArgs->mark(e);
    FunctionObject::markObjects(that, e);
}
