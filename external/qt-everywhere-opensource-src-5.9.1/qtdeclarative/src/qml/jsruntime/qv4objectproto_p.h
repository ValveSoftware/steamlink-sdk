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

    static void method_getPrototypeOf(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getOwnPropertyDescriptor(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getOwnPropertyNames(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_create(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_defineProperty(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_defineProperties(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_seal(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_freeze(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_preventExtensions(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_isSealed(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_isFrozen(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_isExtensible(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_keys(const BuiltinFunction *, Scope &scope, CallData *callData);

    static void method_toString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toLocaleString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_valueOf(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_hasOwnProperty(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_isPrototypeOf(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_propertyIsEnumerable(const BuiltinFunction *, Scope &scope, CallData *callData);

    static void method_defineGetter(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_defineSetter(const BuiltinFunction *, Scope &scope, CallData *callData);

    static void method_get_proto(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_set_proto(const BuiltinFunction *, Scope &scope, CallData *callData);

    static void toPropertyDescriptor(ExecutionEngine *engine, const Value &v, Property *desc, PropertyAttributes *attrs);
    static ReturnedValue fromPropertyDescriptor(ExecutionEngine *engine, const Property *desc, PropertyAttributes attrs);

    static Heap::ArrayObject *getOwnPropertyNames(ExecutionEngine *v4, const Value &o);
};


}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
