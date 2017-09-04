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
#ifndef QV4MATHOBJECT_H
#define QV4MATHOBJECT_H

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

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

struct MathObject : Object {
    void init();
};

}

struct MathObject: Object
{
    V4_OBJECT2(MathObject, Object)
    Q_MANAGED_TYPE(MathObject)

    static void method_abs(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_acos(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_asin(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_atan(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_atan2(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_ceil(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_cos(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_exp(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_floor(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_log(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_max(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_min(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_pow(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_random(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_round(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_sign(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_sin(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_sqrt(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_tan(const BuiltinFunction *, Scope &scope, CallData *callData);
};

}

QT_END_NAMESPACE

#endif // QMLJS_OBJECTS_H
