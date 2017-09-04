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
#ifndef QV4FUNCTIONOBJECT_H
#define QV4FUNCTIONOBJECT_H

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

#include "qv4object_p.h"
#include "qv4function_p.h"
#include "qv4context_p.h"
#include <private/qv4mm_p.h>

QT_BEGIN_NAMESPACE

struct QQmlSourceLocation;

namespace QV4 {

struct BuiltinFunction;

namespace Heap {

struct Q_QML_PRIVATE_EXPORT FunctionObject : Object {
    enum {
        Index_Prototype = 0,
        Index_ProtoConstructor = 0
    };

    void init(QV4::ExecutionContext *scope, QV4::String *name = 0, bool createProto = false);
    void init(QV4::ExecutionContext *scope, QV4::Function *function, bool createProto = false);
    void init(QV4::ExecutionContext *scope, const QString &name, bool createProto = false);
    void init();
    void destroy();

    unsigned int formalParameterCount() { return function ? function->nFormals : 0; }
    unsigned int varCount() { return function ? function->compiledFunction->nLocals : 0; }
    bool needsActivation() const { return function ? function->needsActivation() : false; }

    const QV4::Object *protoProperty() const { return propertyData(Index_Prototype)->as<QV4::Object>(); }

    Pointer<ExecutionContext> scope;
    Function *function;
};

struct FunctionCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

struct FunctionPrototype : FunctionObject {
    void init();
};

struct Q_QML_EXPORT OldBuiltinFunction : FunctionObject {
    void init(QV4::ExecutionContext *scope, QV4::String *name, ReturnedValue (*code)(QV4::CallContext *));
    ReturnedValue (*code)(QV4::CallContext *);
};

struct Q_QML_EXPORT BuiltinFunction : FunctionObject {
    void init(QV4::ExecutionContext *scope, QV4::String *name, void (*code)(const QV4::BuiltinFunction *, Scope &, CallData *));
    void (*code)(const QV4::BuiltinFunction *, Scope &, CallData *);
};

struct IndexedBuiltinFunction : FunctionObject {
    inline void init(QV4::ExecutionContext *scope, uint index, ReturnedValue (*code)(QV4::CallContext *ctx, uint index));
    ReturnedValue (*code)(QV4::CallContext *, uint index);
    uint index;
};

struct ScriptFunction : FunctionObject {
    enum {
        Index_Name = FunctionObject::Index_Prototype + 1,
        Index_Length
    };
    void init(QV4::ExecutionContext *scope, Function *function);

    QV4::InternalClass *cachedClassForConstructor;
};

struct BoundFunction : FunctionObject {
    void init(QV4::ExecutionContext *scope, QV4::FunctionObject *target, const Value &boundThis, QV4::MemberData *boundArgs);
    Pointer<FunctionObject> target;
    Value boundThis;
    Pointer<MemberData> boundArgs;
};

}

struct Q_QML_EXPORT FunctionObject: Object {
    enum {
        IsFunctionObject = true
    };
    V4_OBJECT2(FunctionObject, Object)
    Q_MANAGED_TYPE(FunctionObject)
    V4_INTERNALCLASS(FunctionObject)
    V4_PROTOTYPE(functionPrototype)
    V4_NEEDS_DESTROY

    Heap::ExecutionContext *scope() const { return d()->scope; }
    Function *function() const { return d()->function; }

    ReturnedValue name() const;
    unsigned int formalParameterCount() const { return d()->formalParameterCount(); }
    unsigned int varCount() const { return d()->varCount(); }

    void init(String *name, bool createProto);

    using Object::construct;
    using Object::call;
    static void construct(const Managed *that, Scope &scope, CallData *);
    static void call(const Managed *that, Scope &scope, CallData *d);

    static Heap::FunctionObject *createScriptFunction(ExecutionContext *scope, Function *function);

    bool needsActivation() const { return d()->needsActivation(); }
    bool strictMode() const { return d()->function ? d()->function->isStrict() : false; }
    bool isBinding() const;
    bool isBoundFunction() const;

    QQmlSourceLocation sourceLocation() const;

    static void markObjects(Heap::Base *that, ExecutionEngine *e);
};

template<>
inline const FunctionObject *Value::as() const {
    return isManaged() && m()->vtable()->isFunctionObject ? reinterpret_cast<const FunctionObject *>(this) : 0;
}


struct FunctionCtor: FunctionObject
{
    V4_OBJECT2(FunctionCtor, FunctionObject)

    static void construct(const Managed *that, Scope &scope, CallData *callData);
    static void call(const Managed *that, Scope &scope, CallData *callData);
};

struct FunctionPrototype: FunctionObject
{
    V4_OBJECT2(FunctionPrototype, FunctionObject)

    void init(ExecutionEngine *engine, Object *ctor);

    static void method_toString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_apply(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_call(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_bind(const BuiltinFunction *, Scope &scope, CallData *callData);
};

struct Q_QML_EXPORT BuiltinFunction : FunctionObject {
    V4_OBJECT2(BuiltinFunction, FunctionObject)
    V4_INTERNALCLASS(BuiltinFunction)

    static Heap::BuiltinFunction *create(ExecutionContext *scope, String *name, void (*code)(const BuiltinFunction *, Scope &, CallData *))
    {
        return scope->engine()->memoryManager->allocObject<BuiltinFunction>(scope, name, code);
    }

    static void construct(const Managed *, Scope &scope, CallData *);
    static void call(const Managed *that, Scope &scope, CallData *callData);
};

struct IndexedBuiltinFunction: FunctionObject
{
    V4_OBJECT2(IndexedBuiltinFunction, FunctionObject)

    static void construct(const Managed *m, Scope &scope, CallData *)
    {
        scope.result = static_cast<const IndexedBuiltinFunction *>(m)->engine()->throwTypeError();
    }

    static void call(const Managed *that, Scope &scope, CallData *callData);
};

void Heap::IndexedBuiltinFunction::init(QV4::ExecutionContext *scope, uint index,
                                        ReturnedValue (*code)(QV4::CallContext *ctx, uint index))
{
    Heap::FunctionObject::init(scope);
    this->index = index;
    this->code = code;
}


struct ScriptFunction : FunctionObject {
    V4_OBJECT2(ScriptFunction, FunctionObject)
    V4_INTERNALCLASS(ScriptFunction)

    static void construct(const Managed *, Scope &scope, CallData *callData);
    static void call(const Managed *that, Scope &scope, CallData *callData);

    InternalClass *classForConstructor() const;
};


struct BoundFunction: FunctionObject {
    V4_OBJECT2(BoundFunction, FunctionObject)

    static Heap::BoundFunction *create(ExecutionContext *scope, FunctionObject *target, const Value &boundThis, QV4::MemberData *boundArgs)
    {
        return scope->engine()->memoryManager->allocObject<BoundFunction>(scope, target, boundThis, boundArgs);
    }

    Heap::FunctionObject *target() const { return d()->target; }
    Value boundThis() const { return d()->boundThis; }
    Heap::MemberData *boundArgs() const { return d()->boundArgs; }

    static void construct(const Managed *, Scope &scope, CallData *d);
    static void call(const Managed *that, Scope &scope, CallData *dd);

    static void markObjects(Heap::Base *that, ExecutionEngine *e);
};

}

QT_END_NAMESPACE

#endif // QMLJS_OBJECTS_H
