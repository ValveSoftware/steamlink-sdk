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
#ifndef QMLJS_ENVIRONMENT_H
#define QMLJS_ENVIRONMENT_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qv4global_p.h"
#include "qv4managed_p.h"

QT_BEGIN_NAMESPACE

class QQmlContextData;
class QObject;

namespace QV4 {

namespace CompiledData {
struct CompilationUnit;
struct Function;
}

struct QmlContextWrapper;
struct Identifier;
struct CallContext;
struct CatchContext;
struct WithContext;

struct CallData
{
    // below is to be compatible with Value. Initialize tag to 0
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
    uint tag;
#endif
    int argc;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    uint tag;
#endif
    inline ReturnedValue argument(int i) const {
        return i < argc ? args[i].asReturnedValue() : Primitive::undefinedValue().asReturnedValue();
    }

    Value thisObject;
    Value args[1];
};

namespace Heap {

struct ExecutionContext : Base {
    enum ContextType {
        Type_GlobalContext = 0x1,
        Type_CatchContext = 0x2,
        Type_WithContext = 0x3,
        Type_QmlContext = 0x4,
        Type_SimpleCallContext = 0x5,
        Type_CallContext = 0x6
    };

    void init(ExecutionEngine *engine, ContextType t)
    {
        Base::init();

        callData = nullptr;
        this->engine = engine;
        outer = nullptr;
        lookups = nullptr;
        constantTable = nullptr;
        compilationUnit = nullptr;
        type = t;
        strictMode = false;
        lineNumber = -1;
    }

    CallData *callData;

    ExecutionEngine *engine;
    Pointer<ExecutionContext> outer;
    Lookup *lookups;
    const QV4::Value *constantTable;
    CompiledData::CompilationUnit *compilationUnit;

    ContextType type : 8;
    bool strictMode : 8;
    int lineNumber;
};
V4_ASSERT_IS_TRIVIAL(ExecutionContext)

struct CallContext : ExecutionContext {
    static CallContext createOnStack(ExecutionEngine *v4);

    void init(ExecutionEngine *engine, ContextType t = Type_SimpleCallContext)
    {
        ExecutionContext::init(engine, t);
        function = 0;
        locals = 0;
        activation = 0;
    }

    Pointer<FunctionObject> function;
    Value *locals;
    Pointer<Object> activation;
};
V4_ASSERT_IS_TRIVIAL(CallContext)

struct GlobalContext : ExecutionContext {
    void init(ExecutionEngine *engine);
    Pointer<Object> global;
};
V4_ASSERT_IS_TRIVIAL(GlobalContext)

struct CatchContext : ExecutionContext {
    void init(ExecutionContext *outerContext, String *exceptionVarName, const Value &exceptionValue);
    Pointer<String> exceptionVarName;
    Value exceptionValue;
};
V4_ASSERT_IS_TRIVIAL(CatchContext)

struct WithContext : ExecutionContext {
    void init(ExecutionContext *outerContext, Object *with)
    {
        Heap::ExecutionContext::init(outerContext->engine, Heap::ExecutionContext::Type_WithContext);
        outer = outerContext;
        callData = outer->callData;
        lookups = outer->lookups;
        constantTable = outer->constantTable;
        compilationUnit = outer->compilationUnit;

        withObject = with;
    }

    Pointer<Object> withObject;
};
V4_ASSERT_IS_TRIVIAL(WithContext)

struct QmlContextWrapper;

struct QmlContext : ExecutionContext {
    void init(QV4::ExecutionContext *outerContext, QV4::QmlContextWrapper *qml);

    Pointer<QmlContextWrapper> qml;
};

}

struct Q_QML_EXPORT ExecutionContext : public Managed
{
    enum {
        IsExecutionContext = true
    };

    V4_MANAGED(ExecutionContext, Managed)
    Q_MANAGED_TYPE(ExecutionContext)

    ExecutionEngine *engine() const { return d()->engine; }

    Heap::CallContext *newCallContext(const FunctionObject *f, CallData *callData);
    Heap::WithContext *newWithContext(Heap::Object *with);
    Heap::CatchContext *newCatchContext(Heap::String *exceptionVarName, ReturnedValue exceptionValue);
    Heap::QmlContext *newQmlContext(QmlContextWrapper *qml);
    Heap::QmlContext *newQmlContext(QQmlContextData *context, QObject *scopeObject);

    void createMutableBinding(String *name, bool deletable);

    void setProperty(String *name, const Value &value);
    ReturnedValue getProperty(String *name);
    ReturnedValue getPropertyAndBase(String *name, Value *base);
    bool deleteProperty(String *name);

    inline CallContext *asCallContext();
    inline const CallContext *asCallContext() const;
    inline const CatchContext *asCatchContext() const;
    inline const WithContext *asWithContext() const;

    Heap::FunctionObject *getFunctionObject() const;

    static void markObjects(Heap::Base *m, ExecutionEngine *e);

    Value &thisObject() const {
        return d()->callData->thisObject;
    }
    int argc() const {
        return d()->callData->argc;
    }
    const Value *args() const {
        return d()->callData->args;
    }
    ReturnedValue argument(int i) const {
        return d()->callData->argument(i);
    }
};

struct Q_QML_EXPORT CallContext : public ExecutionContext
{
    V4_MANAGED(CallContext, ExecutionContext)

    // formals are in reverse order
    Identifier * const *formals() const;
    unsigned int formalCount() const;
    Identifier * const *variables() const;
    unsigned int variableCount() const;

    inline ReturnedValue argument(int i);
    bool needsOwnArguments() const;
};

inline ReturnedValue CallContext::argument(int i) {
    return i < argc() ? args()[i].asReturnedValue() : Primitive::undefinedValue().asReturnedValue();
}

struct GlobalContext : public ExecutionContext
{
    V4_MANAGED(GlobalContext, ExecutionContext)

};

struct CatchContext : public ExecutionContext
{
    V4_MANAGED(CatchContext, ExecutionContext)
};

struct WithContext : public ExecutionContext
{
    V4_MANAGED(WithContext, ExecutionContext)
};

struct Q_QML_EXPORT QmlContext : public ExecutionContext
{
    V4_MANAGED(QmlContext, ExecutionContext)

    QObject *qmlScope() const;
    QQmlContextData *qmlContext() const;

    void takeContextOwnership();
};

inline CallContext *ExecutionContext::asCallContext()
{
    return d()->type >= Heap::ExecutionContext::Type_SimpleCallContext ? static_cast<CallContext *>(this) : 0;
}

inline const CallContext *ExecutionContext::asCallContext() const
{
    return d()->type >= Heap::ExecutionContext::Type_SimpleCallContext ? static_cast<const CallContext *>(this) : 0;
}

inline const CatchContext *ExecutionContext::asCatchContext() const
{
    return d()->type == Heap::ExecutionContext::Type_CatchContext ? static_cast<const CatchContext *>(this) : 0;
}

inline const WithContext *ExecutionContext::asWithContext() const
{
    return d()->type == Heap::ExecutionContext::Type_WithContext ? static_cast<const WithContext *>(this) : 0;
}

inline Heap::CallContext Heap::CallContext::createOnStack(ExecutionEngine *v4)
{
    Heap::CallContext ctxt;
    memset(&ctxt, 0, sizeof(Heap::CallContext));
    ctxt.mm_data = 0;
    ctxt.setVtable(QV4::CallContext::staticVTable());
    ctxt.init(v4);
    return ctxt;
}

/* Function *f, int argc */
#define requiredMemoryForExecutionContect(f, argc) \
    ((sizeof(CallContext::Data) + 7) & ~7) + sizeof(Value) * (f->varCount() + qMax((uint)argc, f->formalParameterCount())) + sizeof(CallData)

} // namespace QV4

QT_END_NAMESPACE

#endif
