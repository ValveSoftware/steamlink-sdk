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

#include "qv4managed_p.h"
#include "qv4mm_p.h"
#include "qv4errorobject_p.h"

using namespace QV4;


const ManagedVTable Managed::static_vtbl =
{
    0,
    Managed::IsExecutionContext,
    Managed::IsString,
    Managed::IsObject,
    Managed::IsFunctionObject,
    Managed::IsErrorObject,
    Managed::IsArrayData,
    0,
    Managed::MyType,
    "Managed",
    0,
    0 /*markObjects*/,
    isEqualTo
};


void *Managed::operator new(size_t size, MemoryManager *mm)
{
    assert(mm);

    return mm->allocManaged(size);
}

ExecutionEngine *Managed::engine() const
{
    return internalClass()->engine;
}

QString Managed::className() const
{
    const char *s = 0;
    switch (Type(internalClass()->vtable->type)) {
    case Type_Invalid:
    case Type_String:
        return QString();
    case Type_Object:
        s = "Object";
        break;
    case Type_ArrayObject:
        s = "Array";
        break;
    case Type_FunctionObject:
        s = "Function";
        break;
    case Type_BooleanObject:
        s = "Boolean";
        break;
    case Type_NumberObject:
        s = "Number";
        break;
    case Type_StringObject:
        s = "String";
        break;
    case Type_DateObject:
        s = "Date";
        break;
    case Type_RegExpObject:
        s = "RegExp";
        break;
    case Type_ErrorObject:
        switch (ErrorObject::ErrorType(subtype())) {
        case ErrorObject::Error:
            s = "Error";
            break;
        case ErrorObject::EvalError:
            s = "EvalError";
            break;
        case ErrorObject::RangeError:
            s = "RangeError";
            break;
        case ErrorObject::ReferenceError:
            s = "ReferenceError";
            break;
        case ErrorObject::SyntaxError:
            s = "SyntaxError";
            break;
        case ErrorObject::TypeError:
            s = "TypeError";
            break;
        case ErrorObject::URIError:
            s = "URIError";
            break;
        }
        break;
    case Type_ArgumentsObject:
        s = "Arguments";
        break;
    case Type_JsonObject:
        s = "JSON";
        break;
    case Type_MathObject:
        s = "Math";
        break;

    case Type_ExecutionContext:
        s = "__ExecutionContext";
        break;
    case Type_ForeachIteratorObject:
        s = "__ForeachIterator";
        break;
    case Type_RegExp:
        s = "__RegExp";
        break;

    case Type_QmlSequence:
        s = "QmlSequence";
        break;
    }
    return QString::fromLatin1(s);
}

void Managed::setVTable(const ManagedVTable *vt)
{
    Q_ASSERT(internalClass());
    d()->internalClass = internalClass()->changeVTable(vt);
}

void Managed::Data::setVTable(const ManagedVTable *vt)
{
    Q_ASSERT(internalClass);
    internalClass = internalClass->changeVTable(vt);
}


bool Managed::isEqualTo(Managed *, Managed *)
{
    return false;
}
