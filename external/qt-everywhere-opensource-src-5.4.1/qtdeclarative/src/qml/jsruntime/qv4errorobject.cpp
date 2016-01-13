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


#include "qv4errorobject_p.h"
#include "qv4mm_p.h"
#include <QtCore/qnumeric.h>
#include <QtCore/qmath.h>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QDebug>
#include <cmath>
#include <qmath.h>
#include <qnumeric.h>

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

ErrorObject::Data::Data(InternalClass *ic)
    : Object::Data(ic)
{
    Scope scope(ic->engine);
    Scoped<ErrorObject> e(scope, this);

    ScopedString s(scope, scope.engine->newString(QStringLiteral("Error")));
    e->defineDefaultProperty(QStringLiteral("name"), s);
}

ErrorObject::Data::Data(InternalClass *ic, const ValueRef message, ErrorType t)
    : Object::Data(ic)
{
    subtype = t;

    Scope scope(ic->engine);
    Scoped<ErrorObject> e(scope, this);

    e->defineAccessorProperty(QStringLiteral("stack"), ErrorObject::method_get_stack, 0);

    if (!message->isUndefined())
        e->defineDefaultProperty(QStringLiteral("message"), message);
    ScopedString s(scope);
    e->defineDefaultProperty(QStringLiteral("name"), (s = scope.engine->newString(e->className())));

    e->d()->stackTrace = scope.engine->stackTrace();
    if (!e->d()->stackTrace.isEmpty()) {
        e->defineDefaultProperty(QStringLiteral("fileName"), (s = scope.engine->newString(e->d()->stackTrace.at(0).source)));
        e->defineDefaultProperty(QStringLiteral("lineNumber"), Primitive::fromInt32(e->d()->stackTrace.at(0).line));
    }
}

ErrorObject::Data::Data(InternalClass *ic, const QString &message, ErrorObject::ErrorType t)
    : Object::Data(ic)
{
    subtype = t;

    Scope scope(ic->engine);
    Scoped<ErrorObject> e(scope, this);
    ScopedString s(scope);

    e->defineAccessorProperty(QStringLiteral("stack"), ErrorObject::method_get_stack, 0);

    ScopedValue v(scope, scope.engine->newString(message));
    e->defineDefaultProperty(QStringLiteral("message"), v);
    e->defineDefaultProperty(QStringLiteral("name"), (s = scope.engine->newString(e->className())));

    e->d()->stackTrace = scope.engine->stackTrace();
    if (!e->d()->stackTrace.isEmpty()) {
        e->defineDefaultProperty(QStringLiteral("fileName"), (s = scope.engine->newString(e->d()->stackTrace.at(0).source)));
        e->defineDefaultProperty(QStringLiteral("lineNumber"), Primitive::fromInt32(e->d()->stackTrace.at(0).line));
    }
}

ErrorObject::Data::Data(InternalClass *ic, const QString &message, const QString &fileName, int line, int column, ErrorObject::ErrorType t)
    : Object::Data(ic)
{
    subtype = t;

    Scope scope(ic->engine);
    Scoped<ErrorObject> e(scope, this);
    ScopedString s(scope);

    e->defineAccessorProperty(QStringLiteral("stack"), ErrorObject::method_get_stack, 0);
    e->defineDefaultProperty(QStringLiteral("name"), (s = scope.engine->newString(e->className())));

    e->d()->stackTrace = scope.engine->stackTrace();
    StackFrame frame;
    frame.source = fileName;
    frame.line = line;
    frame.column = column;
    e->d()->stackTrace.prepend(frame);

    if (!e->d()->stackTrace.isEmpty()) {
        e->defineDefaultProperty(QStringLiteral("fileName"), (s = scope.engine->newString(e->d()->stackTrace.at(0).source)));
        e->defineDefaultProperty(QStringLiteral("lineNumber"), Primitive::fromInt32(e->d()->stackTrace.at(0).line));
    }

    ScopedValue v(scope, scope.engine->newString(message));
    e->defineDefaultProperty(QStringLiteral("message"), v);
}

ReturnedValue ErrorObject::method_get_stack(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<ErrorObject> This(scope, ctx->d()->callData->thisObject);
    if (!This)
        return ctx->throwTypeError();
    if (!This->d()->stack) {
        QString trace;
        for (int i = 0; i < This->d()->stackTrace.count(); ++i) {
            if (i > 0)
                trace += QLatin1Char('\n');
            const StackFrame &frame = This->d()->stackTrace[i];
            trace += frame.function;
            trace += QLatin1Char('@');
            trace += frame.source;
            if (frame.line >= 0) {
                trace += QLatin1Char(':');
                trace += QString::number(frame.line);
            }
        }
        This->d()->stack = ctx->d()->engine->newString(trace)->getPointer();
    }
    return This->d()->stack->asReturnedValue();
}

void ErrorObject::markObjects(Managed *that, ExecutionEngine *e)
{
    ErrorObject *This = that->asErrorObject();
    if (This->d()->stack)
        This->d()->stack->mark(e);
    Object::markObjects(that, e);
}

DEFINE_OBJECT_VTABLE(ErrorObject);

DEFINE_OBJECT_VTABLE(SyntaxErrorObject);

SyntaxErrorObject::Data::Data(ExecutionEngine *engine, const ValueRef msg)
    : ErrorObject::Data(engine->syntaxErrorClass, msg, SyntaxError)
{
}

SyntaxErrorObject::Data::Data(ExecutionEngine *engine, const QString &msg, const QString &fileName, int lineNumber, int columnNumber)
    : ErrorObject::Data(engine->syntaxErrorClass, msg, fileName, lineNumber, columnNumber, SyntaxError)
{
}

EvalErrorObject::Data::Data(ExecutionEngine *engine, const ValueRef message)
    : ErrorObject::Data(engine->evalErrorClass, message, EvalError)
{
}

RangeErrorObject::Data::Data(ExecutionEngine *engine, const ValueRef message)
    : ErrorObject::Data(engine->rangeErrorClass, message, RangeError)
{
}

RangeErrorObject::Data::Data(ExecutionEngine *engine, const QString &message)
    : ErrorObject::Data(engine->rangeErrorClass, message, RangeError)
{
}

ReferenceErrorObject::Data::Data(ExecutionEngine *engine, const ValueRef message)
    : ErrorObject::Data(engine->referenceErrorClass, message, ReferenceError)
{
}

ReferenceErrorObject::Data::Data(ExecutionEngine *engine, const QString &message)
    : ErrorObject::Data(engine->referenceErrorClass, message, ReferenceError)
{
}

ReferenceErrorObject::Data::Data(ExecutionEngine *engine, const QString &msg, const QString &fileName, int lineNumber, int columnNumber)
    : ErrorObject::Data(engine->referenceErrorClass, msg, fileName, lineNumber, columnNumber, ReferenceError)
{
}

TypeErrorObject::Data::Data(ExecutionEngine *engine, const ValueRef message)
    : ErrorObject::Data(engine->typeErrorClass, message, TypeError)
{
}

TypeErrorObject::Data::Data(ExecutionEngine *engine, const QString &message)
    : ErrorObject::Data(engine->typeErrorClass, message, TypeError)
{
}

URIErrorObject::Data::Data(ExecutionEngine *engine, const ValueRef message)
    : ErrorObject::Data(engine->uriErrorClass, message, URIError)
{
}

DEFINE_OBJECT_VTABLE(ErrorCtor);
DEFINE_OBJECT_VTABLE(EvalErrorCtor);
DEFINE_OBJECT_VTABLE(RangeErrorCtor);
DEFINE_OBJECT_VTABLE(ReferenceErrorCtor);
DEFINE_OBJECT_VTABLE(SyntaxErrorCtor);
DEFINE_OBJECT_VTABLE(TypeErrorCtor);
DEFINE_OBJECT_VTABLE(URIErrorCtor);

ErrorCtor::Data::Data(ExecutionContext *scope)
    : FunctionObject::Data(scope, QStringLiteral("Error"))
{
    setVTable(staticVTable());
}

ErrorCtor::Data::Data(ExecutionContext *scope, const QString &name)
    : FunctionObject::Data(scope, name)
{
    setVTable(staticVTable());
}

ReturnedValue ErrorCtor::construct(Managed *m, CallData *callData)
{
    Scope scope(m->engine());
    ScopedValue v(scope, callData->argument(0));
    return Encode(m->engine()->newErrorObject(v));
}

ReturnedValue ErrorCtor::call(Managed *that, CallData *callData)
{
    return static_cast<Object *>(that)->construct(callData);
}

EvalErrorCtor::Data::Data(ExecutionContext *scope)
    : ErrorCtor::Data(scope, QStringLiteral("EvalError"))
{
    setVTable(staticVTable());
}

ReturnedValue EvalErrorCtor::construct(Managed *m, CallData *callData)
{
    Scope scope(m->engine());
    ScopedValue v(scope, callData->argument(0));
    return (m->engine()->memoryManager->alloc<EvalErrorObject>(m->engine(), v))->asReturnedValue();
}

RangeErrorCtor::Data::Data(ExecutionContext *scope)
    : ErrorCtor::Data(scope, QStringLiteral("RangeError"))
{
    setVTable(staticVTable());
}

ReturnedValue RangeErrorCtor::construct(Managed *m, CallData *callData)
{
    Scope scope(m->engine());
    ScopedValue v(scope, callData->argument(0));
    return (m->engine()->memoryManager->alloc<RangeErrorObject>(scope.engine, v))->asReturnedValue();
}

ReferenceErrorCtor::Data::Data(ExecutionContext *scope)
    : ErrorCtor::Data(scope, QStringLiteral("ReferenceError"))
{
    setVTable(staticVTable());
}

ReturnedValue ReferenceErrorCtor::construct(Managed *m, CallData *callData)
{
    Scope scope(m->engine());
    ScopedValue v(scope, callData->argument(0));
    return (m->engine()->memoryManager->alloc<ReferenceErrorObject>(scope.engine, v))->asReturnedValue();
}

SyntaxErrorCtor::Data::Data(ExecutionContext *scope)
    : ErrorCtor::Data(scope, QStringLiteral("SyntaxError"))
{
    setVTable(staticVTable());
}

ReturnedValue SyntaxErrorCtor::construct(Managed *m, CallData *callData)
{
    Scope scope(m->engine());
    ScopedValue v(scope, callData->argument(0));
    return (m->engine()->memoryManager->alloc<SyntaxErrorObject>(scope.engine, v))->asReturnedValue();
}

TypeErrorCtor::Data::Data(ExecutionContext *scope)
    : ErrorCtor::Data(scope, QStringLiteral("TypeError"))
{
    setVTable(staticVTable());
}

ReturnedValue TypeErrorCtor::construct(Managed *m, CallData *callData)
{
    Scope scope(m->engine());
    ScopedValue v(scope, callData->argument(0));
    return (m->engine()->memoryManager->alloc<TypeErrorObject>(scope.engine, v))->asReturnedValue();
}

URIErrorCtor::Data::Data(ExecutionContext *scope)
    : ErrorCtor::Data(scope, QStringLiteral("URIError"))
{
    setVTable(staticVTable());
}

ReturnedValue URIErrorCtor::construct(Managed *m, CallData *callData)
{
    Scope scope(m->engine());
    ScopedValue v(scope, callData->argument(0));
    return (m->engine()->memoryManager->alloc<URIErrorObject>(scope.engine, v))->asReturnedValue();
}

void ErrorPrototype::init(ExecutionEngine *engine, Object *ctor, Object *obj)
{
    Scope scope(engine);
    ScopedString s(scope);
    ScopedObject o(scope);
    ctor->defineReadonlyProperty(engine->id_prototype, (o = obj));
    ctor->defineReadonlyProperty(engine->id_length, Primitive::fromInt32(1));
    obj->defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    obj->defineDefaultProperty(engine->id_toString, method_toString, 0);
    obj->defineDefaultProperty(QStringLiteral("message"), (s = engine->newString(QString())));
}

ReturnedValue ErrorPrototype::method_toString(CallContext *ctx)
{
    Scope scope(ctx);

    Object *o = ctx->d()->callData->thisObject.asObject();
    if (!o)
        return ctx->throwTypeError();

    ScopedValue name(scope, o->get(ctx->d()->engine->id_name));
    QString qname;
    if (name->isUndefined())
        qname = QString::fromLatin1("Error");
    else
        qname = name->toQString();

    ScopedString s(scope, ctx->d()->engine->newString(QString::fromLatin1("message")));
    ScopedValue message(scope, o->get(s.getPointer()));
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
