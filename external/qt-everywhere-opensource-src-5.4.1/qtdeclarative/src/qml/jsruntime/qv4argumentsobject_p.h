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
#ifndef QV4ARGUMENTSOBJECTS_H
#define QV4ARGUMENTSOBJECTS_H

#include "qv4object_p.h"
#include "qv4functionobject_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct ArgumentsGetterFunction: FunctionObject
{
    struct Data : FunctionObject::Data {
        Data(ExecutionContext *scope, uint index)
            : FunctionObject::Data(scope)
            , index(index)
        {
            setVTable(staticVTable());
        }
        uint index;
    };
    V4_OBJECT(FunctionObject)

    uint index() const { return d()->index; }

    static ReturnedValue call(Managed *that, CallData *d);
};

struct ArgumentsSetterFunction: FunctionObject
{
    struct Data : FunctionObject::Data {
        Data(ExecutionContext *scope, uint index)
            : FunctionObject::Data(scope)
            , index(index)
        {
            setVTable(staticVTable());
        }
        uint index;
    };
    V4_OBJECT(FunctionObject)

    uint index() const { return d()->index; }

    static ReturnedValue call(Managed *that, CallData *callData);
};


struct ArgumentsObject: Object {
    struct Data : Object::Data {
        Data(CallContext *context);
        CallContext *context;
        bool fullyCreated;
        Members mappedArguments;
    };
    V4_OBJECT(Object)
    Q_MANAGED_TYPE(ArgumentsObject)

    CallContext *context() const { return d()->context; }
    bool fullyCreated() const { return d()->fullyCreated; }
    Members &mappedArguments() { return d()->mappedArguments; }

    static bool isNonStrictArgumentsObject(Managed *m) {
        return m->internalClass()->vtable->type == Type_ArgumentsObject &&
                !static_cast<ArgumentsObject *>(m)->context()->d()->strictMode;
    }

    enum {
        LengthPropertyIndex = 0,
        CalleePropertyIndex = 1,
        CallerPropertyIndex = 3
    };
    bool defineOwnProperty(ExecutionContext *ctx, uint index, const Property &desc, PropertyAttributes attrs);
    static ReturnedValue getIndexed(Managed *m, uint index, bool *hasProperty);
    static void putIndexed(Managed *m, uint index, const ValueRef value);
    static bool deleteIndexedProperty(Managed *m, uint index);
    static PropertyAttributes queryIndexed(const Managed *m, uint index);
    static void markObjects(Managed *that, ExecutionEngine *e);

    void fullyCreate();
};

}

QT_END_NAMESPACE

#endif

