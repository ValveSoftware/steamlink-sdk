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
#ifndef QV4DATEOBJECT_P_H
#define QV4DATEOBJECT_P_H

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
#include <QtCore/private/qnumeric_p.h>

QT_BEGIN_NAMESPACE

class QDateTime;

namespace QV4 {

namespace Heap {

struct DateObject : Object {
    void init()
    {
        Object::init();
        date = qt_qnan();
    }

    void init(const Value &date)
    {
        Object::init();
        this->date = date.toNumber();
    }
    void init(const QDateTime &date);
    void init(const QTime &time);

    double date;
};


struct DateCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope);
};

}

struct DateObject: Object {
    V4_OBJECT2(DateObject, Object)
    Q_MANAGED_TYPE(DateObject)
    V4_PROTOTYPE(datePrototype)


    double date() const { return d()->date; }
    void setDate(double date) { d()->date = date; }

    QDateTime toQDateTime() const;
};

template<>
inline const DateObject *Value::as() const {
    return isManaged() && m()->vtable()->type == Managed::Type_DateObject ? static_cast<const DateObject *>(this) : 0;
}

struct DateCtor: FunctionObject
{
    V4_OBJECT2(DateCtor, FunctionObject)

    static void construct(const Managed *, Scope &scope, CallData *callData);
    static void call(const Managed *that, Scope &scope, CallData *);
};

struct DatePrototype: DateObject
{
    V4_PROTOTYPE(objectPrototype)

    void init(ExecutionEngine *engine, Object *ctor);

    static double getThisDate(Scope &scope, CallData *callData);

    static void method_parse(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_UTC(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_now(const BuiltinFunction *, Scope &scope, CallData *callData);

    static void method_toString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toDateString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toTimeString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toLocaleString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toLocaleDateString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toLocaleTimeString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_valueOf(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getTime(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getYear(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getFullYear(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getUTCFullYear(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getMonth(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getUTCMonth(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getDate(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getUTCDate(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getDay(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getUTCDay(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getHours(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getUTCHours(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getMinutes(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getUTCMinutes(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getSeconds(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getUTCSeconds(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getMilliseconds(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getUTCMilliseconds(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_getTimezoneOffset(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setTime(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setMilliseconds(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setUTCMilliseconds(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setSeconds(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setUTCSeconds(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setMinutes(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setUTCMinutes(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setHours(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setUTCHours(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setDate(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setUTCDate(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setMonth(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setUTCMonth(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setYear(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setFullYear(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_setUTCFullYear(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toUTCString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toISOString(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_toJSON(const BuiltinFunction *, Scope &scope, CallData *callData);

    static void timezoneUpdated();
};

}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
