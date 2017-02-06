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
    return isManaged() && m() && m()->vtable()->type == Managed::Type_DateObject ? static_cast<const DateObject *>(this) : 0;
}

struct DateCtor: FunctionObject
{
    V4_OBJECT2(DateCtor, FunctionObject)

    static void construct(const Managed *, Scope &scope, CallData *callData);
    static void call(const Managed *that, Scope &scope, CallData *);
};

struct DatePrototype: DateObject
{
    void init(ExecutionEngine *engine, Object *ctor);

    static double getThisDate(ExecutionContext *ctx);

    static ReturnedValue method_parse(CallContext *ctx);
    static ReturnedValue method_UTC(CallContext *ctx);
    static ReturnedValue method_now(CallContext *ctx);

    static ReturnedValue method_toString(CallContext *ctx);
    static ReturnedValue method_toDateString(CallContext *ctx);
    static ReturnedValue method_toTimeString(CallContext *ctx);
    static ReturnedValue method_toLocaleString(CallContext *ctx);
    static ReturnedValue method_toLocaleDateString(CallContext *ctx);
    static ReturnedValue method_toLocaleTimeString(CallContext *ctx);
    static ReturnedValue method_valueOf(CallContext *ctx);
    static ReturnedValue method_getTime(CallContext *ctx);
    static ReturnedValue method_getYear(CallContext *ctx);
    static ReturnedValue method_getFullYear(CallContext *ctx);
    static ReturnedValue method_getUTCFullYear(CallContext *ctx);
    static ReturnedValue method_getMonth(CallContext *ctx);
    static ReturnedValue method_getUTCMonth(CallContext *ctx);
    static ReturnedValue method_getDate(CallContext *ctx);
    static ReturnedValue method_getUTCDate(CallContext *ctx);
    static ReturnedValue method_getDay(CallContext *ctx);
    static ReturnedValue method_getUTCDay(CallContext *ctx);
    static ReturnedValue method_getHours(CallContext *ctx);
    static ReturnedValue method_getUTCHours(CallContext *ctx);
    static ReturnedValue method_getMinutes(CallContext *ctx);
    static ReturnedValue method_getUTCMinutes(CallContext *ctx);
    static ReturnedValue method_getSeconds(CallContext *ctx);
    static ReturnedValue method_getUTCSeconds(CallContext *ctx);
    static ReturnedValue method_getMilliseconds(CallContext *ctx);
    static ReturnedValue method_getUTCMilliseconds(CallContext *ctx);
    static ReturnedValue method_getTimezoneOffset(CallContext *ctx);
    static ReturnedValue method_setTime(CallContext *ctx);
    static ReturnedValue method_setMilliseconds(CallContext *ctx);
    static ReturnedValue method_setUTCMilliseconds(CallContext *ctx);
    static ReturnedValue method_setSeconds(CallContext *ctx);
    static ReturnedValue method_setUTCSeconds(CallContext *ctx);
    static ReturnedValue method_setMinutes(CallContext *ctx);
    static ReturnedValue method_setUTCMinutes(CallContext *ctx);
    static ReturnedValue method_setHours(CallContext *ctx);
    static ReturnedValue method_setUTCHours(CallContext *ctx);
    static ReturnedValue method_setDate(CallContext *ctx);
    static ReturnedValue method_setUTCDate(CallContext *ctx);
    static ReturnedValue method_setMonth(CallContext *ctx);
    static ReturnedValue method_setUTCMonth(CallContext *ctx);
    static ReturnedValue method_setYear(CallContext *ctx);
    static ReturnedValue method_setFullYear(CallContext *ctx);
    static ReturnedValue method_setUTCFullYear(CallContext *ctx);
    static ReturnedValue method_toUTCString(CallContext *ctx);
    static ReturnedValue method_toISOString(CallContext *ctx);
    static ReturnedValue method_toJSON(CallContext *ctx);

    static void timezoneUpdated();
};

}

QT_END_NAMESPACE

#endif // QV4ECMAOBJECTS_P_H
