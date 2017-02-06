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

#include "qv4managed_p.h"
#include <private/qv4mm_p.h>
#include "qv4errorobject_p.h"

using namespace QV4;


const VTable Managed::static_vtbl =
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


QString Managed::className() const
{
    const char *s = 0;
    switch (Type(d()->vtable()->type)) {
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
        s = ErrorObject::className(static_cast<Heap::ErrorObject *>(d())->errorType);
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

bool Managed::isEqualTo(Managed *, Managed *)
{
    return false;
}
