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
#ifndef QV4STRINGOBJECT_P_H
#define QV4STRINGOBJECT_P_H

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

struct StringObject : Object {
    enum {
        LengthPropertyIndex = 0
    };

    void init();
    void init(const QV4::String *string);
    String *string;

    Heap::String *getIndex(uint index) const;
    uint length() const;
};

struct StringCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

}

struct StringObject: Object {
    V4_OBJECT2(StringObject, Object)
    Q_MANAGED_TYPE(StringObject)
    V4_INTERNALCLASS(StringObject)
    V4_PROTOTYPE(stringPrototype)

    Heap::String *getIndex(uint index) const {
        return d()->getIndex(index);
    }
    uint length() const {
        return d()->length();
    }

    static bool deleteIndexedProperty(Managed *m, uint index);

protected:
    static void advanceIterator(Managed *m, ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attrs);
    static void markObjects(Heap::Base *that, ExecutionEngine *e);
};

struct StringCtor: FunctionObject
{
    V4_OBJECT2(StringCtor, FunctionObject)

    static void construct(const Managed *m, Scope &scope, CallData *callData);
    static void call(const Managed *, Scope &scope, CallData *callData);
};

struct StringPrototype: StringObject
{
    V4_PROTOTYPE(objectPrototype)
    void init(ExecutionEngine *engine, Object *ctor);

    static void method_toString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_charAt(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_charCodeAt(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_concat(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_endsWith(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_indexOf(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_includes(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_lastIndexOf(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_localeCompare(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_match(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_replace(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_search(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_slice(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_split(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_startsWith(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_substr(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_substring(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toLowerCase(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toLocaleLowerCase(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toUpperCase(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toLocaleUpperCase(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_fromCharCode(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_trim(const BuiltinFunction *, Scope &scope, CallData *callData);
};

}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
