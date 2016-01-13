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
#ifndef QV4ERROROBJECT_H
#define QV4ERROROBJECT_H

#include "qv4object_p.h"
#include "qv4functionobject_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct SyntaxErrorObject;

struct ErrorObject: Object {
    enum {
        IsErrorObject = true
    };

    enum ErrorType {
        Error,
        EvalError,
        RangeError,
        ReferenceError,
        SyntaxError,
        TypeError,
        URIError
    };
    struct Data : Object::Data {
        Data(InternalClass *ic);
        Data(InternalClass *ic, const ValueRef message, ErrorType t = Error);
        Data(InternalClass *ic, const QString &message, ErrorType t = Error);
        Data(InternalClass *ic, const QString &message, const QString &fileName, int line, int column, ErrorType t = Error);
        StackTrace stackTrace;
        String *stack;
    };
    V4_OBJECT(Object)
    Q_MANAGED_TYPE(ErrorObject)

    SyntaxErrorObject *asSyntaxError();

    static ReturnedValue method_get_stack(CallContext *ctx);
    static void markObjects(Managed *that, ExecutionEngine *e);
    static void destroy(Managed *that) { static_cast<ErrorObject *>(that)->d()->~Data(); }
};

template<>
inline ErrorObject *value_cast(const Value &v) {
    return v.asErrorObject();
}

struct EvalErrorObject: ErrorObject {
    struct Data : ErrorObject::Data {
        Data(ExecutionEngine *engine, const ValueRef message);
    };
};

struct RangeErrorObject: ErrorObject {
    struct Data : ErrorObject::Data {
        Data(ExecutionEngine *engine, const ValueRef message);
        Data(ExecutionEngine *engine, const QString &msg);
    };
};

struct ReferenceErrorObject: ErrorObject {
    struct Data : ErrorObject::Data {
        Data(ExecutionEngine *engine, const ValueRef message);
        Data(ExecutionEngine *engine, const QString &msg);
        Data(ExecutionEngine *engine, const QString &msg, const QString &fileName, int lineNumber, int columnNumber);
    };
};

struct SyntaxErrorObject: ErrorObject {
    struct Data : ErrorObject::Data {
        Data(ExecutionEngine *engine, const ValueRef message);
        Data(ExecutionEngine *engine, const QString &msg, const QString &fileName, int lineNumber, int columnNumber);
    };
    V4_OBJECT(ErrorObject)
};

struct TypeErrorObject: ErrorObject {
    struct Data : ErrorObject::Data {
        Data(ExecutionEngine *engine, const ValueRef message);
        Data(ExecutionEngine *engine, const QString &msg);
    };
};

struct URIErrorObject: ErrorObject {
    struct Data : ErrorObject::Data {
        Data(ExecutionEngine *engine, const ValueRef message);
    };
};

struct ErrorCtor: FunctionObject
{
    struct Data : FunctionObject::Data {
        Data(ExecutionContext *scope);
        Data(ExecutionContext *scope, const QString &name);
    };

    V4_OBJECT(FunctionObject)

    static ReturnedValue construct(Managed *, CallData *callData);
    static ReturnedValue call(Managed *that, CallData *callData);
};

struct EvalErrorCtor: ErrorCtor
{
    struct Data : ErrorCtor::Data {
        Data(ExecutionContext *scope);
    };
    V4_OBJECT(ErrorCtor)

    static ReturnedValue construct(Managed *m, CallData *callData);
};

struct RangeErrorCtor: ErrorCtor
{
    struct Data : ErrorCtor::Data {
        Data(ExecutionContext *scope);
    };
    V4_OBJECT(ErrorCtor)

    static ReturnedValue construct(Managed *m, CallData *callData);
};

struct ReferenceErrorCtor: ErrorCtor
{
    struct Data : ErrorCtor::Data {
        Data(ExecutionContext *scope);
    };
    V4_OBJECT(ErrorCtor)

    static ReturnedValue construct(Managed *m, CallData *callData);
};

struct SyntaxErrorCtor: ErrorCtor
{
    struct Data : ErrorCtor::Data {
        Data(ExecutionContext *scope);
    };
    V4_OBJECT(ErrorCtor)

    static ReturnedValue construct(Managed *m, CallData *callData);
};

struct TypeErrorCtor: ErrorCtor
{
    struct Data : ErrorCtor::Data {
        Data(ExecutionContext *scope);
    };
    V4_OBJECT(ErrorCtor)

    static ReturnedValue construct(Managed *m, CallData *callData);
};

struct URIErrorCtor: ErrorCtor
{
    struct Data : ErrorCtor::Data {
        Data(ExecutionContext *scope);
    };
    V4_OBJECT(ErrorCtor)

    static ReturnedValue construct(Managed *m, CallData *callData);
};


struct ErrorPrototype : ErrorObject
{
    void init(ExecutionEngine *engine, Object *ctor) { init(engine, ctor, this); }

    static void init(ExecutionEngine *engine, Object *ctor, Object *obj);
    static ReturnedValue method_toString(CallContext *ctx);
};

struct EvalErrorPrototype : ErrorObject
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this); }
};

struct RangeErrorPrototype : ErrorObject
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this); }
};

struct ReferenceErrorPrototype : ErrorObject
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this); }
};

struct SyntaxErrorPrototype : ErrorObject
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this); }
};

struct TypeErrorPrototype : ErrorObject
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this); }
};

struct URIErrorPrototype : ErrorObject
{
    void init(ExecutionEngine *engine, Object *ctor) { ErrorPrototype::init(engine, ctor, this); }
};


inline SyntaxErrorObject *ErrorObject::asSyntaxError()
{
    return subtype() == SyntaxError ? static_cast<SyntaxErrorObject *>(this) : 0;
}

}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
