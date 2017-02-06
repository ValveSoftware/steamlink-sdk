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
#ifndef QV4ECMAOBJECTS_P_H
#define QV4ECMAOBJECTS_P_H

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
#include <QtCore/qnumeric.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

struct ObjectCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

}

struct ObjectCtor: FunctionObject
{
    V4_OBJECT2(ObjectCtor, FunctionObject)

    static void construct(const Managed *that, Scope &scope, CallData *callData);
    static void call(const Managed *that, Scope &scope, CallData *callData);
};

struct ObjectPrototype: Object
{
    void init(ExecutionEngine *engine, Object *ctor);

    static ReturnedValue method_getPrototypeOf(CallContext *ctx);
    static ReturnedValue method_getOwnPropertyDescriptor(CallContext *ctx);
    static ReturnedValue method_getOwnPropertyNames(CallContext *context);
    static ReturnedValue method_create(CallContext *ctx);
    static ReturnedValue method_defineProperty(CallContext *ctx);
    static ReturnedValue method_defineProperties(CallContext *ctx);
    static ReturnedValue method_seal(CallContext *ctx);
    static ReturnedValue method_freeze(CallContext *ctx);
    static ReturnedValue method_preventExtensions(CallContext *ctx);
    static ReturnedValue method_isSealed(CallContext *ctx);
    static ReturnedValue method_isFrozen(CallContext *ctx);
    static ReturnedValue method_isExtensible(CallContext *ctx);
    static ReturnedValue method_keys(CallContext *ctx);

    static ReturnedValue method_toString(CallContext *ctx);
    static ReturnedValue method_toLocaleString(CallContext *ctx);
    static ReturnedValue method_valueOf(CallContext *ctx);
    static ReturnedValue method_hasOwnProperty(CallContext *ctx);
    static ReturnedValue method_isPrototypeOf(CallContext *ctx);
    static ReturnedValue method_propertyIsEnumerable(CallContext *ctx);

    static ReturnedValue method_defineGetter(CallContext *ctx);
    static ReturnedValue method_defineSetter(CallContext *ctx);

    static ReturnedValue method_get_proto(CallContext *ctx);
    static ReturnedValue method_set_proto(CallContext *ctx);

    static void toPropertyDescriptor(ExecutionEngine *engine, const Value &v, Property *desc, PropertyAttributes *attrs);
    static ReturnedValue fromPropertyDescriptor(ExecutionEngine *engine, const Property *desc, PropertyAttributes attrs);

    static Heap::ArrayObject *getOwnPropertyNames(ExecutionEngine *v4, const Value &o);
};


}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
