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
#ifndef QV4ARRAYOBJECT_H
#define QV4ARRAYOBJECT_H

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

struct ArrayCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

}

struct ArrayCtor: FunctionObject
{
    V4_OBJECT2(ArrayCtor, FunctionObject)

    static void construct(const Managed *m, Scope &scope, CallData *callData);
    static void call(const Managed *that, Scope &scope, CallData *callData);
};

struct ArrayPrototype: ArrayObject
{
    void init(ExecutionEngine *engine, Object *ctor);

    static void method_isArray(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_toString(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_toLocaleString(const BuiltinFunction *builtin, Scope &, CallData *callData);
    static void method_concat(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_find(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_findIndex(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_join(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_pop(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_push(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_reverse(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_shift(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_slice(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_sort(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_splice(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_unshift(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_indexOf(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_lastIndexOf(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_every(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_some(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_forEach(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_map(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_filter(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_reduce(const BuiltinFunction *, Scope &, CallData *callData);
    static void method_reduceRight(const BuiltinFunction *, Scope &, CallData *callData);
};


} // namespace QV4

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
