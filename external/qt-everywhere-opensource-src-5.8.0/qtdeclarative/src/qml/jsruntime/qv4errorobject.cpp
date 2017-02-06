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


#include "qv4errorobject_p.h"
#include <QtCore/qnumeric.h>
#include <QtCore/qmath.h>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QDebug>

#include "qv4string_p.h"
#include <private/qv4mm_p.h>
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

void Heap::ErrorObject::init()
{
    Object::init();
    stackTrace = nullptr;

    Scope scope(internalClass->engine);
    Scoped<QV4::ErrorObject> e(scope, this);

    if (internalClass == scope.engine->errorProtoClass)
        return;

    *propertyData(QV4::ErrorObject::Index_Stack) = scope.engine->getStackFunction();
    *propertyData(QV4::ErrorObject::Index_Stack + QV4::Object::SetterOffset) = Encode::undefined();
    *propertyData(QV4::ErrorObject::Index_FileName) = Encode::undefined();
    *propertyData(QV4::ErrorObject::Index_LineNumber) = Encode::undefined();
}

void Heap::ErrorObject::init(const Value &message, ErrorType t)
{
    Object::init();
    errorType = t;

    Scope scope(internalClass->engine);
    Scoped<QV4::ErrorObject> e(scope, this);

    *propertyData(QV4::ErrorObject::Index_Stack) = scope.engine->getStackFunction();
    *propertyData(QV4::ErrorObject::Index_Stack + QV4::Object::SetterOffset) = Encode::undefined();

    e->d()->stackTrace = new StackTrace(scope.engine->stackTrace());
    if (!e->d()->stackTrace->isEmpty()) {
        *propertyData(QV4::ErrorObject::Index_FileName) = scope.engine->newString(e->d()->stackTrace->at(0).source);
        *propertyData(QV4::ErrorObject::Index_LineNumber) = Primitive::fromInt32(e->d()->stackTrace->at(0).line);
    }

    if (!message.isUndefined())
        *propertyData(QV4::ErrorObject::Index_Message) = message;
}

void Heap::ErrorObject::init(const Value &message, const QString &fileName, int line, int column, ErrorObject::ErrorType t)
{
    Object::init();
    errorType = t;

    Scope scope(internalClass->engine);
    Scoped<QV4::ErrorObject> e(scope, this);

    *propertyData(QV4::ErrorObject::Index_Stack) = scope.engine->getStackFunction();
    *propertyData(QV4::ErrorObject::Index_Stack + QV4::Object::SetterOffset) = Encode::undefined();

    e->d()->stackTrace = new StackTrace(scope.engine->stackTrace());
    StackFrame frame;
    frame.source = fileName;
    frame.line = line;
    frame.column = column;
    e->d()->stackTrace->prepend(frame);

    if (!e->d()->stackTrace->isEmpty()) {
        *propertyData(QV4::ErrorObject::Index_FileName) = scope.engine->newString(e->d()->stackTrace->at(0).source);
        *propertyData(QV4::ErrorObject::Index_LineNumber) = Primitive::fromInt32(e->d()->stackTrace->at(0).line);
    }

    if (!message.isUndefined())
        *propertyData(QV4::ErrorObject::Index_Message) = message;
}

const char *ErrorObject::className(Heap::ErrorObject::ErrorType t)
{
    switch (t) {
    case Heap::ErrorObject::Error:
        return "Error";
    case Heap::ErrorObject::EvalError:
        return "EvalError";
    case Heap::ErrorObject::RangeError:
        return "RangeError";
    case Heap::ErrorObject::ReferenceError:
        return "ReferenceError";
    case Heap::ErrorObject::SyntaxError:
        return "SyntaxError";
    case Heap::ErrorObject::TypeError:
        return "TypeError";
    case Heap::ErrorObject::URIError:
        return "URIError";
    }
    Q_UNREACHABLE();
}

ReturnedValue ErrorObject::method_get_stack(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<ErrorObject> This(scope, ctx->thisObject());
    if (!This)
        return ctx->engine()->throwTypeError();
    if (!This->d()->stack) {
        QString trace;
        for (int i = 0; i < This->d()->stackTrace->count(); ++i) {
            if (i > 0)
                trace += QLatin1Char('\n');
            const StackFrame &frame = This->d()->stackTrace->at(i);
            trace += frame.function + QLatin1Char('@') + frame.source;
            if (frame.line >= 0)
                trace += QLatin1Char(':') + QString::number(frame.line);
        }
        This->d()->stack = ctx->d()->engine->newString(trace);
    }
    return This->d()->stack->asReturnedValue();
}

void ErrorObject::markObjects(Heap::Base *that, ExecutionEngine *e)
{
    ErrorObject::Data *This = static_cast<ErrorObject::Data *>(that);
    if (This->stack)
        This->stack->mark(e);
    Object::markObjects(that, e);
}

DEFINE_OBJECT_VTABLE(ErrorObject);

DEFINE_OBJECT_VTABLE(SyntaxErrorObject);

void Heap::SyntaxErrorObject::init(const Value &msg)
{
    Heap::ErrorObject::init(msg, SyntaxError);
}

void Heap::SyntaxErrorObject::init(const Value &msg, const QString &fileName, int lineNumber, int columnNumber)
{
    Heap::ErrorObject::init(msg, fileName, lineNumber, columnNumber, SyntaxError);
}

void Heap::EvalErrorObject::init(const Value &message)
{
    Heap::ErrorObject::init(message, EvalError);
}

void Heap::RangeErrorObject::init(const Value &message)
{
    Heap::ErrorObject::init(message, RangeError);
}

void Heap::ReferenceErrorObject::init(const Value &message)
{
    Heap::ErrorObject::init(message, ReferenceError);
}

void Heap::ReferenceErrorObject::init(const Value &msg, const QString &fileName, int lineNumber, int columnNumber)
{
    Heap::ErrorObject::init(msg, fileName, lineNumber, columnNumber, ReferenceError);
}

void Heap::TypeErrorObject::init(const Value &message)
{
    Heap::ErrorObject::init(message, TypeError);
}

void Heap::URIErrorObject::init(const Value &message)
{
    Heap::ErrorObject::init(message, URIError);
}

DEFINE_OBJECT_VTABLE(ErrorCtor);
DEFINE_OBJECT_VTABLE(EvalErrorCtor);
DEFINE_OBJECT_VTABLE(RangeErrorCtor);
DEFINE_OBJECT_VTABLE(ReferenceErrorCtor);
DEFINE_OBJECT_VTABLE(SyntaxErrorCtor);
DEFINE_OBJECT_VTABLE(TypeErrorCtor);
DEFINE_OBJECT_VTABLE(URIErrorCtor);

void Heap::ErrorCtor::init(QV4::ExecutionContext *scope)
{
    Heap::FunctionObject::init(scope, QStringLiteral("Error"));
}

void Heap::ErrorCtor::init(QV4::ExecutionContext *scope, const QString &name)
{
    Heap::FunctionObject::init(scope, name);
}

void ErrorCtor::construct(const Managed *, Scope &scope, CallData *callData)
{
    ScopedValue v(scope, callData->argument(0));
    scope.result = ErrorObject::create<ErrorObject>(scope.engine, v);
}

void ErrorCtor::call(const Managed *that, Scope &scope, CallData *callData)
{
    static_cast<const Object *>(that)->construct(scope, callData);
}

void Heap::EvalErrorCtor::init(QV4::ExecutionContext *scope)
{
    Heap::ErrorCtor::init(scope, QStringLiteral("EvalError"));
}

void EvalErrorCtor::construct(const Managed *, Scope &scope, CallData *callData)
{
    ScopedValue v(scope, callData->argument(0));
    scope.result = ErrorObject::create<EvalErrorObject>(scope.engine, v);
}

void Heap::RangeErrorCtor::init(QV4::ExecutionContext *scope)
{
    Heap::ErrorCtor::init(scope, QStringLiteral("RangeError"));
}

void RangeErrorCtor::construct(const Managed *, Scope &scope, CallData *callData)
{
    ScopedValue v(scope, callData->argument(0));
    scope.result = ErrorObject::create<RangeErrorObject>(scope.engine, v);
}

void Heap::ReferenceErrorCtor::init(QV4::ExecutionContext *scope)
{
    Heap::ErrorCtor::init(scope, QStringLiteral("ReferenceError"));
}

void ReferenceErrorCtor::construct(const Managed *, Scope &scope, CallData *callData)
{
    ScopedValue v(scope, callData->argument(0));
    scope.result = ErrorObject::create<ReferenceErrorObject>(scope.engine, v);
}

void Heap::SyntaxErrorCtor::init(QV4::ExecutionContext *scope)
{
    Heap::ErrorCtor::init(scope, QStringLiteral("SyntaxError"));
}

void SyntaxErrorCtor::construct(const Managed *, Scope &scope, CallData *callData)
{
    ScopedValue v(scope, callData->argument(0));
    scope.result = ErrorObject::create<SyntaxErrorObject>(scope.engine, v);
}

void Heap::TypeErrorCtor::init(QV4::ExecutionContext *scope)
{
    Heap::ErrorCtor::init(scope, QStringLiteral("TypeError"));
}

void TypeErrorCtor::construct(const Managed *, Scope &scope, CallData *callData)
{
    ScopedValue v(scope, callData->argument(0));
    scope.result = ErrorObject::create<TypeErrorObject>(scope.engine, v);
}

void Heap::URIErrorCtor::init(QV4::ExecutionContext *scope)
{
    Heap::ErrorCtor::init(scope, QStringLiteral("URIError"));
}

void URIErrorCtor::construct(const Managed *, Scope &scope, CallData *callData)
{
    ScopedValue v(scope, callData->argument(0));
    scope.result = ErrorObject::create<URIErrorObject>(scope.engine, v);
}

void ErrorPrototype::init(ExecutionEngine *engine, Object *ctor, Object *obj, Heap::ErrorObject::ErrorType t)
{
    Scope scope(engine);
    ScopedString s(scope);
    ScopedObject o(scope);
    ctor->defineReadonlyProperty(engine->id_prototype(), (o = obj));
    ctor->defineReadonlyProperty(engine->id_length(), Primitive::fromInt32(1));
    *obj->propertyData(Index_Constructor) = ctor;
    *obj->propertyData(Index_Message) = engine->id_empty();
    *obj->propertyData(Index_Name) = engine->newString(QString::fromLatin1(ErrorObject::className(t)));
    if (t == Heap::ErrorObject::Error)
        obj->defineDefaultProperty(engine->id_toString(), method_toString, 0);
}

ReturnedValue ErrorPrototype::method_toString(CallContext *ctx)
{
    Scope scope(ctx);

    Object *o = ctx->thisObject().as<Object>();
    if (!o)
        return ctx->engine()->throwTypeError();

    ScopedValue name(scope, o->get(ctx->d()->engine->id_name()));
    QString qname;
    if (name->isUndefined())
        qname = QStringLiteral("Error");
    else
        qname = name->toQString();

    ScopedString s(scope, ctx->d()->engine->newString(QStringLiteral("message")));
    ScopedValue message(scope, o->get(s));
    QString qmessage;
    if (!message->isUndefined())
        qmessage = message->toQString();

    QString str;
    if (qname.isEmpty()) {
        str = qmessage;
    } else if (qmessage.isEmpty()) {
        str = qname;
    } else {
        str = qname + QLatin1String(": ") + qmessage;
    }

    return ctx->d()->engine->newString(str)->asReturnedValue();
}
