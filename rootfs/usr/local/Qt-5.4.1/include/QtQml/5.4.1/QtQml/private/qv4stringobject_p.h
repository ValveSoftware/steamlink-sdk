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
#ifndef QV4STRINGOBJECT_P_H
#define QV4STRINGOBJECT_P_H

#include "qv4object_p.h"
#include "qv4functionobject_p.h"
#include <QtCore/qnumeric.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct StringObject: Object {
    struct Data : Object::Data {
        Data(ExecutionEngine *engine, const ValueRef value);
        Data(InternalClass *ic);
        Value value;
        // ### get rid of tmpProperty
        mutable Property tmpProperty;
    };
    V4_OBJECT(Object)
    Q_MANAGED_TYPE(StringObject)


    Property *getIndex(uint index) const;

    static bool deleteIndexedProperty(Managed *m, uint index);

protected:
    static void advanceIterator(Managed *m, ObjectIterator *it, String *&name, uint *index, Property *p, PropertyAttributes *attrs);
    static void markObjects(Managed *that, ExecutionEngine *e);
};

struct StringCtor: FunctionObject
{
    struct Data : FunctionObject::Data {
        Data(ExecutionContext *scope);
    };
    V4_OBJECT(FunctionObject)

    static ReturnedValue construct(Managed *m, CallData *callData);
    static ReturnedValue call(Managed *that, CallData *callData);
};

struct StringPrototype: StringObject
{
    void init(ExecutionEngine *engine, Object *ctor);

    static ReturnedValue method_toString(CallContext *context);
    static ReturnedValue method_charAt(CallContext *context);
    static ReturnedValue method_charCodeAt(CallContext *context);
    static ReturnedValue method_concat(CallContext *context);
    static ReturnedValue method_indexOf(CallContext *context);
    static ReturnedValue method_lastIndexOf(CallContext *context);
    static ReturnedValue method_localeCompare(CallContext *context);
    static ReturnedValue method_match(CallContext *context);
    static ReturnedValue method_replace(CallContext *ctx);
    static ReturnedValue method_search(CallContext *ctx);
    static ReturnedValue method_slice(CallContext *ctx);
    static ReturnedValue method_split(CallContext *ctx);
    static ReturnedValue method_substr(CallContext *context);
    static ReturnedValue method_substring(CallContext *context);
    static ReturnedValue method_toLowerCase(CallContext *ctx);
    static ReturnedValue method_toLocaleLowerCase(CallContext *ctx);
    static ReturnedValue method_toUpperCase(CallContext *ctx);
    static ReturnedValue method_toLocaleUpperCase(CallContext *ctx);
    static ReturnedValue method_fromCharCode(CallContext *context);
    static ReturnedValue method_trim(CallContext *ctx);
};

}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
