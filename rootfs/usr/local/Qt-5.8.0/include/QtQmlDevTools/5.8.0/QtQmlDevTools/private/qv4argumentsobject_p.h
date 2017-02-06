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
#ifndef QV4ARGUMENTSOBJECTS_H
#define QV4ARGUMENTSOBJECTS_H

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
#include "qv4functionobject_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

struct ArgumentsGetterFunction : FunctionObject {
    inline void init(QV4::ExecutionContext *scope, uint index);
    uint index;
};

struct ArgumentsSetterFunction : FunctionObject {
    inline void init(QV4::ExecutionContext *scope, uint index);
    uint index;
};

struct ArgumentsObject : Object {
    enum {
        LengthPropertyIndex = 0,
        CalleePropertyIndex = 1,
        CallerPropertyIndex = 3
    };
    void init(QV4::CallContext *context);
    Pointer<CallContext> context;
    bool fullyCreated;
    Pointer<MemberData> mappedArguments;
};

}

struct ArgumentsGetterFunction: FunctionObject
{
    V4_OBJECT2(ArgumentsGetterFunction, FunctionObject)

    uint index() const { return d()->index; }
    static void call(const Managed *that, Scope &scope, CallData *d);
};

inline void
Heap::ArgumentsGetterFunction::init(QV4::ExecutionContext *scope, uint index)
{
    Heap::FunctionObject::init(scope);
    this->index = index;
}

struct ArgumentsSetterFunction: FunctionObject
{
    V4_OBJECT2(ArgumentsSetterFunction, FunctionObject)

    uint index() const { return d()->index; }
    static void call(const Managed *that, Scope &scope, CallData *callData);
};

inline void
Heap::ArgumentsSetterFunction::init(QV4::ExecutionContext *scope, uint index)
{
    Heap::FunctionObject::init(scope);
    this->index = index;
}


struct ArgumentsObject: Object {
    V4_OBJECT2(ArgumentsObject, Object)
    Q_MANAGED_TYPE(ArgumentsObject)

    Heap::CallContext *context() const { return d()->context; }
    bool fullyCreated() const { return d()->fullyCreated; }

    static bool isNonStrictArgumentsObject(Managed *m) {
        return m->d()->vtable()->type == Type_ArgumentsObject &&
                !static_cast<ArgumentsObject *>(m)->context()->strictMode;
    }

    bool defineOwnProperty(ExecutionEngine *engine, uint index, const Property *desc, PropertyAttributes attrs);
    static ReturnedValue getIndexed(const Managed *m, uint index, bool *hasProperty);
    static void putIndexed(Managed *m, uint index, const Value &value);
    static bool deleteIndexedProperty(Managed *m, uint index);
    static PropertyAttributes queryIndexed(const Managed *m, uint index);
    static void markObjects(Heap::Base *that, ExecutionEngine *e);

    void fullyCreate();
};

}

QT_END_NAMESPACE

#endif

