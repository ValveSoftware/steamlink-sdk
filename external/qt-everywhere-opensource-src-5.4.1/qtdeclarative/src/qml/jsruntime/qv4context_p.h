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
#ifndef QMLJS_ENVIRONMENT_H
#define QMLJS_ENVIRONMENT_H

#include "qv4global_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4managed_p.h"
#include "qv4engine_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct Object;
struct ExecutionEngine;
struct DeclarativeEnvironment;
struct Lookup;
struct Function;
struct ValueRef;

namespace CompiledData {
struct CompilationUnit;
struct Function;
}

struct CallContext;
struct CatchContext;
struct WithContext;

struct Q_QML_EXPORT ExecutionContext : public Managed
{
    enum {
        IsExecutionContext = true
    };

    enum ContextType {
        Type_GlobalContext = 0x1,
        Type_CatchContext = 0x2,
        Type_WithContext = 0x3,
        Type_SimpleCallContext = 0x4,
        Type_CallContext = 0x5,
        Type_QmlContext = 0x6
    };

    struct Data : Managed::Data {
        Data(ExecutionEngine *engine, ContextType t)
            : Managed::Data(engine->executionContextClass)
            , type(t)
            , strictMode(false)
            , engine(engine)
            , parent(engine->currentContext())
            , outer(0)
            , lookups(0)
            , compilationUnit(0)
            , lineNumber(-1)
        {
            engine->current = reinterpret_cast<ExecutionContext *>(this);
        }
        ContextType type;
        bool strictMode;

        CallData *callData;

        ExecutionEngine *engine;
        ExecutionContext *parent;
        ExecutionContext *outer;
        Lookup *lookups;
        CompiledData::CompilationUnit *compilationUnit;

        int lineNumber;

    };
    V4_MANAGED(Managed)
    Q_MANAGED_TYPE(ExecutionContext)

    ExecutionContext(ExecutionEngine *engine, ContextType t)
        : Managed(engine->executionContextClass)
    {
        d()->type = t;
        d()->strictMode = false;
        d()->engine = engine;
        d()->parent = engine->currentContext();
        d()->outer = 0;
        d()->lookups = 0;
        d()->compilationUnit = 0;
        d()->lineNumber = -1;
        engine->current = this;
    }

    HeapObject *newCallContext(FunctionObject *f, CallData *callData);
    WithContext *newWithContext(Object *with);
    CatchContext *newCatchContext(String *exceptionVarName, const ValueRef exceptionValue);
    CallContext *newQmlContext(FunctionObject *f, Object *qml);

    void createMutableBinding(String *name, bool deletable);

    ReturnedValue throwError(const QV4::ValueRef value);
    ReturnedValue throwError(const QString &message);
    ReturnedValue throwSyntaxError(const QString &message);
    ReturnedValue throwSyntaxError(const QString &message, const QString &fileName, int lineNumber, int column);
    ReturnedValue throwTypeError();
    ReturnedValue throwTypeError(const QString &message);
    ReturnedValue throwReferenceError(const ValueRef value);
    ReturnedValue throwReferenceError(const QString &value, const QString &fileName, int lineNumber, int column);
    ReturnedValue throwRangeError(const ValueRef value);
    ReturnedValue throwRangeError(const QString &message);
    ReturnedValue throwURIError(const ValueRef msg);
    ReturnedValue throwUnimplemented(const QString &message);

    void setProperty(String *name, const ValueRef value);
    ReturnedValue getProperty(String *name);
    ReturnedValue getPropertyAndBase(String *name, Object *&base);
    bool deleteProperty(String *name);

    // Can only be called from within catch(...), rethrows if no JS exception.
    ReturnedValue catchException(StackTrace *trace = 0);

    inline CallContext *asCallContext();
    inline const CallContext *asCallContext() const;
    inline const CatchContext *asCatchContext() const;
    inline const WithContext *asWithContext() const;

    inline FunctionObject *getFunctionObject() const;

    static void markObjects(Managed *m, ExecutionEngine *e);
};

struct CallContext : public ExecutionContext
{
    struct Data : ExecutionContext::Data {
        Data(ExecutionEngine *engine, ContextType t = Type_SimpleCallContext)
            : ExecutionContext::Data(engine, t)
        {
            function = 0;
            locals = 0;
            activation = 0;
        }
        Data(ExecutionEngine *engine, Object *qml, QV4::FunctionObject *function);

        FunctionObject *function;
        int realArgumentCount;
        Value *locals;
        Object *activation;
    };
    V4_MANAGED(ExecutionContext)

    // formals are in reverse order
    String * const *formals() const;
    unsigned int formalCount() const;
    String * const *variables() const;
    unsigned int variableCount() const;

    inline ReturnedValue argument(int i);
    bool needsOwnArguments() const;
};

inline ReturnedValue CallContext::argument(int i) {
    return i < d()->callData->argc ? d()->callData->args[i].asReturnedValue() : Primitive::undefinedValue().asReturnedValue();
}

struct GlobalContext : public ExecutionContext
{
    struct Data : ExecutionContext::Data {
        Data(ExecutionEngine *engine);
        Object *global;
    };
    V4_MANAGED(ExecutionContext)

};

struct CatchContext : public ExecutionContext
{
    struct Data : ExecutionContext::Data {
        Data(ExecutionEngine *engine, String *exceptionVarName, const ValueRef exceptionValue);
        StringValue exceptionVarName;
        Value exceptionValue;
    };
    V4_MANAGED(ExecutionContext)
};

struct WithContext : public ExecutionContext
{
    struct Data : ExecutionContext::Data {
        Data(ExecutionEngine *engine, Object *with);
        Object *withObject;
    };
    V4_MANAGED(ExecutionContext)
};

inline CallContext *ExecutionContext::asCallContext()
{
    return d()->type >= Type_SimpleCallContext ? static_cast<CallContext *>(this) : 0;
}

inline const CallContext *ExecutionContext::asCallContext() const
{
    return d()->type >= Type_SimpleCallContext ? static_cast<const CallContext *>(this) : 0;
}

inline const CatchContext *ExecutionContext::asCatchContext() const
{
    return d()->type == Type_CatchContext ? static_cast<const CatchContext *>(this) : 0;
}

inline const WithContext *ExecutionContext::asWithContext() const
{
    return d()->type == Type_WithContext ? static_cast<const WithContext *>(this) : 0;
}

inline FunctionObject *ExecutionContext::getFunctionObject() const
{
    for (const ExecutionContext *it = this; it; it = it->d()->parent) {
        if (const CallContext *callCtx = it->asCallContext())
            return callCtx->d()->function;
        else if (it->asCatchContext() || it->asWithContext())
            continue; // look in the parent context for a FunctionObject
        else
            break;
    }

    return 0;
}

inline void ExecutionEngine::pushContext(CallContext *context)
{
    context->d()->parent = current;
    current = context;
}

inline ExecutionContext *ExecutionEngine::popContext()
{
    Q_ASSERT(current->d()->parent);
    current = current->d()->parent;
    return current;
}

struct ExecutionContextSaver
{
    ExecutionEngine *engine;
    ExecutionContext *savedContext;

    ExecutionContextSaver(ExecutionContext *context)
        : engine(context->d()->engine)
        , savedContext(context)
    {
    }
    ~ExecutionContextSaver()
    {
        engine->current = savedContext;
    }
};

inline Scope::Scope(ExecutionContext *ctx)
    : engine(ctx->d()->engine)
#ifndef QT_NO_DEBUG
    , size(0)
#endif
{
    mark = engine->jsStackTop;
}

/* Function *f, int argc */
#define requiredMemoryForExecutionContect(f, argc) \
    ((sizeof(CallContext::Data) + 7) & ~7) + sizeof(Value) * (f->varCount() + qMax((uint)argc, f->formalParameterCount())) + sizeof(CallData)

} // namespace QV4

QT_END_NAMESPACE

#endif
