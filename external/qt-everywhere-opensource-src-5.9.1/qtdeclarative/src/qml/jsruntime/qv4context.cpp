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

#include <QString>
#include "qv4debugging_p.h"
#include <qv4context_p.h>
#include <qv4object_p.h>
#include <qv4objectproto_p.h>
#include <private/qv4mm_p.h>
#include <qv4argumentsobject_p.h>
#include "qv4function_p.h"
#include "qv4errorobject_p.h"
#include "qv4string_p.h"
#include "qv4qmlcontext_p.h"
#include "qv4profiling_p.h"
#include <private/qqmljavascriptexpression_p.h>

using namespace QV4;

DEFINE_MANAGED_VTABLE(ExecutionContext);
DEFINE_MANAGED_VTABLE(CallContext);
DEFINE_MANAGED_VTABLE(WithContext);
DEFINE_MANAGED_VTABLE(CatchContext);
DEFINE_MANAGED_VTABLE(GlobalContext);

/* Function *f, int argc */
#define requiredMemoryForExecutionContect(f, argc) \
    ((sizeof(CallContext::Data) + 7) & ~7) + \
    sizeof(Value) * (f->compiledFunction->nLocals + qMax((uint)argc, f->nFormals)) + sizeof(CallData)

Heap::CallContext *ExecutionContext::newCallContext(Function *function, CallData *callData)
{
    Heap::CallContext *c = engine()->memoryManager->allocManaged<CallContext>(
                requiredMemoryForExecutionContect(function, callData->argc));
    c->init(Heap::ExecutionContext::Type_CallContext);

    c->v4Function = function;

    c->strictMode = function->isStrict();
    c->outer = this->d();

    c->activation = 0;

    c->compilationUnit = function->compilationUnit;
    c->lookups = function->compilationUnit->runtimeLookups;
    c->constantTable = function->compilationUnit->constants;
    c->locals = (Value *)((quintptr(c + 1) + 7) & ~7);

    const CompiledData::Function *compiledFunction = function->compiledFunction;
    int nLocals = compiledFunction->nLocals;
    if (nLocals)
        std::fill(c->locals, c->locals + nLocals, Primitive::undefinedValue());

    c->callData = reinterpret_cast<CallData *>(c->locals + nLocals);
    ::memcpy(c->callData, callData, sizeof(CallData) + (callData->argc - 1) * sizeof(Value));
    if (callData->argc < static_cast<int>(compiledFunction->nFormals))
        std::fill(c->callData->args + c->callData->argc, c->callData->args + compiledFunction->nFormals, Primitive::undefinedValue());

    return c;
}

Heap::WithContext *ExecutionContext::newWithContext(Heap::Object *with)
{
    return engine()->memoryManager->alloc<WithContext>(d(), with);
}

Heap::CatchContext *ExecutionContext::newCatchContext(Heap::String *exceptionVarName, ReturnedValue exceptionValue)
{
    Scope scope(this);
    ScopedValue e(scope, exceptionValue);
    return engine()->memoryManager->alloc<CatchContext>(d(), exceptionVarName, e);
}

void ExecutionContext::createMutableBinding(String *name, bool deletable)
{
    Scope scope(this);

    // find the right context to create the binding on
    ScopedObject activation(scope);
    ScopedContext ctx(scope, this);
    while (ctx) {
        switch (ctx->d()->type) {
        case Heap::ExecutionContext::Type_CallContext:
        case Heap::ExecutionContext::Type_SimpleCallContext: {
            Heap::CallContext *c = static_cast<Heap::CallContext *>(ctx->d());
            if (!activation) {
                if (!c->activation)
                    c->activation = scope.engine->newObject();
                activation = c->activation;
            }
            break;
        }
        case Heap::ExecutionContext::Type_QmlContext: {
            // this is ugly, as it overrides the inner callcontext, but has to stay as long
            // as bindings still get their own callcontext
            Heap::QmlContext *qml = static_cast<Heap::QmlContext *>(ctx->d());
            activation = qml->qml;
            break;
        }
        case Heap::ExecutionContext::Type_GlobalContext: {
            if (!activation)
                activation = scope.engine->globalObject;
            break;
        }
        default:
            break;
        }
        ctx = ctx->d()->outer;
    }

    if (activation->hasProperty(name))
        return;
    ScopedProperty desc(scope);
    PropertyAttributes attrs(Attr_Data);
    attrs.setConfigurable(deletable);
    activation->__defineOwnProperty__(scope.engine, name, desc, attrs);
}

void Heap::GlobalContext::init(ExecutionEngine *eng)
{
    Heap::ExecutionContext::init(Heap::ExecutionContext::Type_GlobalContext);
    global = eng->globalObject->d();
}

void Heap::CatchContext::init(ExecutionContext *outerContext, String *exceptionVarName,
                              const Value &exceptionValue)
{
    Heap::ExecutionContext::init(Heap::ExecutionContext::Type_CatchContext);
    outer = outerContext;
    strictMode = outer->strictMode;
    callData = outer->callData;
    lookups = outer->lookups;
    constantTable = outer->constantTable;
    compilationUnit = outer->compilationUnit;

    this->exceptionVarName = exceptionVarName;
    this->exceptionValue = exceptionValue;
}


Identifier * const *CallContext::formals() const
{
    return d()->v4Function ? d()->v4Function->internalClass->nameMap.constData() : 0;
}

unsigned int CallContext::formalCount() const
{
    return d()->v4Function ? d()->v4Function->nFormals : 0;
}

Identifier * const *CallContext::variables() const
{
    return d()->v4Function ? d()->v4Function->internalClass->nameMap.constData() + d()->v4Function->nFormals : 0;
}

unsigned int CallContext::variableCount() const
{
    return d()->v4Function ? d()->v4Function->compiledFunction->nLocals : 0;
}



bool ExecutionContext::deleteProperty(String *name)
{
    name->makeIdentifier();
    Identifier *id = name->identifier();

    Scope scope(this);
    bool hasWith = false;
    ScopedContext ctx(scope, this);
    for (; ctx; ctx = ctx->d()->outer) {
        switch (ctx->d()->type) {
        case Heap::ExecutionContext::Type_CatchContext: {
            Heap::CatchContext *c = static_cast<Heap::CatchContext *>(ctx->d());
            if (c->exceptionVarName->isEqualTo(name->d()))
                return false;
            break;
        }
        case Heap::ExecutionContext::Type_WithContext: {
            hasWith = true;
            ScopedObject withObject(scope, static_cast<Heap::WithContext *>(ctx->d())->withObject);
            if (withObject->hasProperty(name))
                return withObject->deleteProperty(name);
            break;
        }
        case Heap::ExecutionContext::Type_GlobalContext: {
            ScopedObject global(scope, static_cast<Heap::GlobalContext *>(ctx->d())->global);
            if (global->hasProperty(name))
                return global->deleteProperty(name);
            break;
        }
        case Heap::ExecutionContext::Type_CallContext:
        case Heap::ExecutionContext::Type_SimpleCallContext: {
            Heap::CallContext *c = static_cast<Heap::CallContext *>(ctx->d());
            if (c->v4Function && (c->v4Function->needsActivation() || hasWith)) {
                uint index = c->v4Function->internalClass->find(id);
                if (index < UINT_MAX)
                    // ### throw in strict mode?
                    return false;
            }
            ScopedObject qml(scope, c->activation);
            if (qml && qml->hasProperty(name))
                return qml->deleteProperty(name);
            break;
        }
        case Heap::ExecutionContext::Type_QmlContext:
            // can't delete properties on qml objects
            break;
        }
    }

    if (d()->strictMode)
        engine()->throwSyntaxError(QStringLiteral("Can't delete property %1").arg(name->toQString()));
    return true;
}

bool CallContext::needsOwnArguments() const
{
    QV4::Function *f = d()->v4Function;
    return (f && f->needsActivation()) || (argc() < (f ? static_cast<int>(f->nFormals) : 0));
}

void ExecutionContext::markObjects(Heap::Base *m, ExecutionEngine *engine)
{
    ExecutionContext::Data *ctx = static_cast<ExecutionContext::Data *>(m);

    if (ctx->outer)
        ctx->outer->mark(engine);

    switch (ctx->type) {
    case Heap::ExecutionContext::Type_CatchContext: {
        CatchContext::Data *c = static_cast<CatchContext::Data *>(ctx);
        c->exceptionVarName->mark(engine);
        c->exceptionValue.mark(engine);
        break;
    }
    case Heap::ExecutionContext::Type_WithContext: {
        WithContext::Data *w = static_cast<WithContext::Data *>(ctx);
        if (w->withObject)
            w->withObject->mark(engine);
        break;
    }
    case Heap::ExecutionContext::Type_GlobalContext: {
        GlobalContext::Data *g = static_cast<GlobalContext::Data *>(ctx);
        g->global->mark(engine);
        break;
    }
    case Heap::ExecutionContext::Type_SimpleCallContext:
        break;
    case Heap::ExecutionContext::Type_CallContext: {
        QV4::Heap::CallContext *c = static_cast<Heap::CallContext *>(ctx);
        Q_ASSERT(c->v4Function);
        ctx->callData->thisObject.mark(engine);
        for (int arg = 0; arg < qMax(ctx->callData->argc, (int)c->v4Function->nFormals); ++arg)
            ctx->callData->args[arg].mark(engine);
        for (unsigned local = 0, lastLocal = c->v4Function->compiledFunction->nLocals; local < lastLocal; ++local)
            c->locals[local].mark(engine);
        if (c->activation)
            c->activation->mark(engine);
        if (c->function)
            c->function->mark(engine);
        break;
    }
    case Heap::ExecutionContext::Type_QmlContext: {
        QmlContext::Data *g = static_cast<QmlContext::Data *>(ctx);
        g->qml->mark(engine);
        break;
    }
    }
}

// Do a standard call with this execution context as the outer scope
void ExecutionContext::call(Scope &scope, CallData *callData, Function *function, const FunctionObject *f)
{
    ExecutionContextSaver ctxSaver(scope);

    Scoped<CallContext> ctx(scope, newCallContext(function, callData));
    if (f)
        ctx->d()->function = f->d();
    scope.engine->pushContext(ctx);

    scope.result = Q_V4_PROFILE(scope.engine, function);

    if (function->hasQmlDependencies)
        QQmlPropertyCapture::registerQmlDependencies(function->compiledFunction, scope);
}

// Do a simple, fast call with this execution context as the outer scope
void QV4::ExecutionContext::simpleCall(Scope &scope, CallData *callData, Function *function)
{
    Q_ASSERT(function->canUseSimpleFunction());

    ExecutionContextSaver ctxSaver(scope);

    CallContext::Data *ctx = scope.engine->memoryManager->allocSimpleCallContext();

    ctx->strictMode = function->isStrict();
    ctx->callData = callData;
    ctx->v4Function = function;
    ctx->compilationUnit = function->compilationUnit;
    ctx->lookups = function->compilationUnit->runtimeLookups;
    ctx->constantTable = function->compilationUnit->constants;
    ctx->outer = this->d();
    ctx->locals = scope.alloc(function->compiledFunction->nLocals);
    for (int i = callData->argc; i < (int)function->nFormals; ++i)
        callData->args[i] = Encode::undefined();

    scope.engine->pushContext(ctx);
    Q_ASSERT(scope.engine->current == ctx);

    scope.result = Q_V4_PROFILE(scope.engine, function);

    if (function->hasQmlDependencies)
        QQmlPropertyCapture::registerQmlDependencies(function->compiledFunction, scope);
    scope.engine->memoryManager->freeSimpleCallContext();
}

void ExecutionContext::setProperty(String *name, const Value &value)
{
    name->makeIdentifier();
    Identifier *id = name->identifier();

    Scope scope(this);
    ScopedContext ctx(scope, this);
    ScopedObject activation(scope);

    for (; ctx; ctx = ctx->d()->outer) {
        activation = (Object *)0;
        switch (ctx->d()->type) {
        case Heap::ExecutionContext::Type_CatchContext: {
            Heap::CatchContext *c = static_cast<Heap::CatchContext *>(ctx->d());
            if (c->exceptionVarName->isEqualTo(name->d())) {
                    c->exceptionValue = value;
                    return;
            }
            break;
        }
        case Heap::ExecutionContext::Type_WithContext: {
            ScopedObject w(scope, static_cast<Heap::WithContext *>(ctx->d())->withObject);
            if (w->hasProperty(name)) {
                w->put(name, value);
                return;
            }
            break;
        }
        case Heap::ExecutionContext::Type_GlobalContext: {
            activation = static_cast<Heap::GlobalContext *>(ctx->d())->global;
            break;
        }
        case Heap::ExecutionContext::Type_CallContext:
        case Heap::ExecutionContext::Type_SimpleCallContext: {
            Heap::CallContext *c = static_cast<Heap::CallContext *>(ctx->d());
            if (c->v4Function) {
                uint index = c->v4Function->internalClass->find(id);
                if (index < UINT_MAX) {
                    if (index < c->v4Function->nFormals) {
                        c->callData->args[c->v4Function->nFormals - index - 1] = value;
                    } else {
                        index -= c->v4Function->nFormals;
                        c->locals[index] = value;
                    }
                    return;
                }
            }
            activation = c->activation;
            break;
        }
        case Heap::ExecutionContext::Type_QmlContext: {
            activation = static_cast<Heap::QmlContext *>(ctx->d())->qml;
            activation->put(name, value);
            return;
        }
        }

        if (activation) {
            uint member = activation->internalClass()->find(id);
            if (member < UINT_MAX) {
                activation->putValue(member, value);
                return;
            }
        }
    }

    if (d()->strictMode || name->equals(engine()->id_this())) {
        ScopedValue n(scope, name->asReturnedValue());
        engine()->throwReferenceError(n);
        return;
    }
    engine()->globalObject->put(name, value);
}

ReturnedValue ExecutionContext::getProperty(String *name)
{
    Scope scope(this);
    ScopedValue v(scope);
    name->makeIdentifier();

    if (name->equals(engine()->id_this()))
        return thisObject().asReturnedValue();

    bool hasWith = false;
    bool hasCatchScope = false;
    ScopedContext ctx(scope, this);
    for (; ctx; ctx = ctx->d()->outer) {
        switch (ctx->d()->type) {
        case Heap::ExecutionContext::Type_CatchContext: {
            hasCatchScope = true;
            Heap::CatchContext *c = static_cast<Heap::CatchContext *>(ctx->d());
            if (c->exceptionVarName->isEqualTo(name->d()))
                return c->exceptionValue.asReturnedValue();
            break;
        }
        case Heap::ExecutionContext::Type_WithContext: {
            ScopedObject w(scope, static_cast<Heap::WithContext *>(ctx->d())->withObject);
            hasWith = true;
            bool hasProperty = false;
            v = w->get(name, &hasProperty);
            if (hasProperty) {
                return v->asReturnedValue();
            }
            break;
        }
        case Heap::ExecutionContext::Type_GlobalContext: {
            ScopedObject global(scope, static_cast<Heap::GlobalContext *>(ctx->d())->global);
            bool hasProperty = false;
            v = global->get(name, &hasProperty);
            if (hasProperty)
                return v->asReturnedValue();
            break;
        }
        case Heap::ExecutionContext::Type_CallContext:
        case Heap::ExecutionContext::Type_SimpleCallContext: {
            Heap::CallContext *c = static_cast<Heap::CallContext *>(ctx->d());
            if (c->v4Function && (c->v4Function->needsActivation() || hasWith || hasCatchScope)) {
                name->makeIdentifier();
                Identifier *id = name->identifier();

                uint index = c->v4Function->internalClass->find(id);
                if (index < UINT_MAX) {
                    if (index < c->v4Function->nFormals)
                        return c->callData->args[c->v4Function->nFormals - index - 1].asReturnedValue();
                    return c->locals[index - c->v4Function->nFormals].asReturnedValue();
                }
            }
            ScopedObject activation(scope, c->activation);
            if (activation) {
                bool hasProperty = false;
                v = activation->get(name, &hasProperty);
                if (hasProperty)
                    return v->asReturnedValue();
            }
            if (c->function && c->v4Function->isNamedExpression()
                && name->equals(ScopedString(scope, c->v4Function->name())))
                return c->function->asReturnedValue();
            break;
        }
        case Heap::ExecutionContext::Type_QmlContext: {
            ScopedObject qml(scope, static_cast<Heap::QmlContext *>(ctx->d())->qml);
            bool hasProperty = false;
            v = qml->get(name, &hasProperty);
            if (hasProperty)
                return v->asReturnedValue();
            break;
        }
        }
    }
    ScopedValue n(scope, name);
    return engine()->throwReferenceError(n);
}

ReturnedValue ExecutionContext::getPropertyAndBase(String *name, Value *base)
{
    Scope scope(this);
    ScopedValue v(scope);
    base->setM(0);
    name->makeIdentifier();

    if (name->equals(engine()->id_this()))
        return thisObject().asReturnedValue();

    bool hasWith = false;
    bool hasCatchScope = false;
    ScopedContext ctx(scope, this);
    for (; ctx; ctx = ctx->d()->outer) {
        switch (ctx->d()->type) {
        case Heap::ExecutionContext::Type_CatchContext: {
            hasCatchScope = true;
            Heap::CatchContext *c = static_cast<Heap::CatchContext *>(ctx->d());
            if (c->exceptionVarName->isEqualTo(name->d()))
                return c->exceptionValue.asReturnedValue();
            break;
        }
        case Heap::ExecutionContext::Type_WithContext: {
            ScopedObject w(scope, static_cast<Heap::WithContext *>(ctx->d())->withObject);
            hasWith = true;
            bool hasProperty = false;
            v = w->get(name, &hasProperty);
            if (hasProperty) {
                base->setM(w->d());
                return v->asReturnedValue();
            }
            break;
        }
        case Heap::ExecutionContext::Type_GlobalContext: {
            ScopedObject global(scope, static_cast<Heap::GlobalContext *>(ctx->d())->global);
            bool hasProperty = false;
            v = global->get(name, &hasProperty);
            if (hasProperty)
                return v->asReturnedValue();
            break;
        }
        case Heap::ExecutionContext::Type_CallContext:
        case Heap::ExecutionContext::Type_SimpleCallContext: {
            Heap::CallContext *c = static_cast<Heap::CallContext *>(ctx->d());
            if (c->v4Function && (c->v4Function->needsActivation() || hasWith || hasCatchScope)) {
                name->makeIdentifier();
                Identifier *id = name->identifier();

                uint index = c->v4Function->internalClass->find(id);
                if (index < UINT_MAX) {
                    if (index < c->v4Function->nFormals)
                        return c->callData->args[c->v4Function->nFormals - index - 1].asReturnedValue();
                    return c->locals[index - c->v4Function->nFormals].asReturnedValue();
                }
            }
            ScopedObject activation(scope, c->activation);
            if (activation) {
                bool hasProperty = false;
                v = activation->get(name, &hasProperty);
                if (hasProperty)
                    return v->asReturnedValue();
            }
            if (c->function && c->v4Function->isNamedExpression()
                && name->equals(ScopedString(scope, c->v4Function->name())))
                return c->function->asReturnedValue();
            break;
        }
        case Heap::ExecutionContext::Type_QmlContext: {
            ScopedObject qml(scope, static_cast<Heap::QmlContext *>(ctx->d())->qml);
            bool hasProperty = false;
            v = qml->get(name, &hasProperty);
            if (hasProperty) {
                base->setM(qml->d());
                return v->asReturnedValue();
            }
            break;
        }
        }
    }
    ScopedValue n(scope, name);
    return engine()->throwReferenceError(n);
}

Function *ExecutionContext::getFunction() const
{
    Scope scope(engine());
    ScopedContext it(scope, this->d());
    for (; it; it = it->d()->outer) {
        if (const CallContext *callCtx = it->asCallContext())
            return callCtx->d()->v4Function;
        else if (it->asCatchContext() || it->asWithContext())
            continue; // look in the parent context for a FunctionObject
        else
            break;
    }

    return 0;
}
