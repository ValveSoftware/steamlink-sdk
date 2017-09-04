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

#include "qv4mathobject_p.h"
#include "qv4objectproto_p.h"

#include <QtCore/qdatetime.h>
#include <QtCore/qmath.h>
#include <QtCore/private/qnumeric_p.h>
#include <QtCore/qthreadstorage.h>

#include <cmath>

using namespace QV4;

DEFINE_OBJECT_VTABLE(MathObject);

void Heap::MathObject::init()
{
    Object::init();
    Scope scope(internalClass->engine);
    ScopedObject m(scope, this);

    m->defineReadonlyProperty(QStringLiteral("E"), Primitive::fromDouble(M_E));
    m->defineReadonlyProperty(QStringLiteral("LN2"), Primitive::fromDouble(M_LN2));
    m->defineReadonlyProperty(QStringLiteral("LN10"), Primitive::fromDouble(M_LN10));
    m->defineReadonlyProperty(QStringLiteral("LOG2E"), Primitive::fromDouble(M_LOG2E));
    m->defineReadonlyProperty(QStringLiteral("LOG10E"), Primitive::fromDouble(M_LOG10E));
    m->defineReadonlyProperty(QStringLiteral("PI"), Primitive::fromDouble(M_PI));
    m->defineReadonlyProperty(QStringLiteral("SQRT1_2"), Primitive::fromDouble(M_SQRT1_2));
    m->defineReadonlyProperty(QStringLiteral("SQRT2"), Primitive::fromDouble(M_SQRT2));

    m->defineDefaultProperty(QStringLiteral("abs"), QV4::MathObject::method_abs, 1);
    m->defineDefaultProperty(QStringLiteral("acos"), QV4::MathObject::method_acos, 1);
    m->defineDefaultProperty(QStringLiteral("asin"), QV4::MathObject::method_asin, 0);
    m->defineDefaultProperty(QStringLiteral("atan"), QV4::MathObject::method_atan, 1);
    m->defineDefaultProperty(QStringLiteral("atan2"), QV4::MathObject::method_atan2, 2);
    m->defineDefaultProperty(QStringLiteral("ceil"), QV4::MathObject::method_ceil, 1);
    m->defineDefaultProperty(QStringLiteral("cos"), QV4::MathObject::method_cos, 1);
    m->defineDefaultProperty(QStringLiteral("exp"), QV4::MathObject::method_exp, 1);
    m->defineDefaultProperty(QStringLiteral("floor"), QV4::MathObject::method_floor, 1);
    m->defineDefaultProperty(QStringLiteral("log"), QV4::MathObject::method_log, 1);
    m->defineDefaultProperty(QStringLiteral("max"), QV4::MathObject::method_max, 2);
    m->defineDefaultProperty(QStringLiteral("min"), QV4::MathObject::method_min, 2);
    m->defineDefaultProperty(QStringLiteral("pow"), QV4::MathObject::method_pow, 2);
    m->defineDefaultProperty(QStringLiteral("random"), QV4::MathObject::method_random, 0);
    m->defineDefaultProperty(QStringLiteral("round"), QV4::MathObject::method_round, 1);
    m->defineDefaultProperty(QStringLiteral("sign"), QV4::MathObject::method_sign, 1);
    m->defineDefaultProperty(QStringLiteral("sin"), QV4::MathObject::method_sin, 1);
    m->defineDefaultProperty(QStringLiteral("sqrt"), QV4::MathObject::method_sqrt, 1);
    m->defineDefaultProperty(QStringLiteral("tan"), QV4::MathObject::method_tan, 1);
}

static Q_ALWAYS_INLINE double copySign(double x, double y)
{
    return ::copysign(x, y);
}

void MathObject::method_abs(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    if (!callData->argc)
        RETURN_RESULT(Encode(qt_qnan()));

    if (callData->args[0].isInteger()) {
        int i = callData->args[0].integerValue();
        RETURN_RESULT(Encode(i < 0 ? - i : i));
    }

    double v = callData->args[0].toNumber();
    if (v == 0) // 0 | -0
        RETURN_RESULT(Encode(0));

    RETURN_RESULT(Encode(v < 0 ? -v : v));
}

void MathObject::method_acos(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : 2;
    if (v > 1)
        RETURN_RESULT(Encode(qt_qnan()));

    RETURN_RESULT(Encode(std::acos(v)));
}

void MathObject::method_asin(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : 2;
    if (v > 1)
        RETURN_RESULT(Encode(qt_qnan()));
    else
        RETURN_RESULT(Encode(std::asin(v)));
}

void MathObject::method_atan(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    if (v == 0.0)
        RETURN_RESULT(Encode(v));
    else
        RETURN_RESULT(Encode(std::atan(v)));
}

void MathObject::method_atan2(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v1 = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    double v2 = callData->argc > 1 ? callData->args[1].toNumber() : qt_qnan();

    if ((v1 < 0) && qt_is_finite(v1) && qt_is_inf(v2) && (copySign(1.0, v2) == 1.0))
        RETURN_RESULT(Encode(copySign(0, -1.0)));

    if ((v1 == 0.0) && (v2 == 0.0)) {
        if ((copySign(1.0, v1) == 1.0) && (copySign(1.0, v2) == -1.0)) {
            RETURN_RESULT(Encode(M_PI));
        } else if ((copySign(1.0, v1) == -1.0) && (copySign(1.0, v2) == -1.0)) {
            RETURN_RESULT(Encode(-M_PI));
        }
    }
    RETURN_RESULT(Encode(std::atan2(v1, v2)));
}

void MathObject::method_ceil(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    if (v < 0.0 && v > -1.0)
        RETURN_RESULT(Encode(copySign(0, -1.0)));
    else
        RETURN_RESULT(Encode(std::ceil(v)));
}

void MathObject::method_cos(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    RETURN_RESULT(Encode(std::cos(v)));
}

void MathObject::method_exp(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    if (qt_is_inf(v)) {
        if (copySign(1.0, v) == -1.0)
            RETURN_RESULT(Encode(0));
        else
            RETURN_RESULT(Encode(qt_inf()));
    } else {
        RETURN_RESULT(Encode(std::exp(v)));
    }
}

void MathObject::method_floor(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    RETURN_RESULT(Encode(std::floor(v)));
}

void MathObject::method_log(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    if (v < 0)
        RETURN_RESULT(Encode(qt_qnan()));
    else
        RETURN_RESULT(Encode(std::log(v)));
}

void MathObject::method_max(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double mx = -qt_inf();
    for (int i = 0; i < callData->argc; ++i) {
        double x = callData->args[i].toNumber();
        if (x > mx || std::isnan(x))
            mx = x;
    }
    RETURN_RESULT(Encode(mx));
}

void MathObject::method_min(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double mx = qt_inf();
    for (int i = 0; i < callData->argc; ++i) {
        double x = callData->args[i].toNumber();
        if ((x == 0 && mx == x && copySign(1.0, x) == -1.0)
                || (x < mx) || std::isnan(x)) {
            mx = x;
        }
    }
    RETURN_RESULT(Encode(mx));
}

void MathObject::method_pow(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double x = callData->argc > 0 ? callData->args[0].toNumber() : qt_qnan();
    double y = callData->argc > 1 ? callData->args[1].toNumber() : qt_qnan();

    if (std::isnan(y))
        RETURN_RESULT(Encode(qt_qnan()));

    if (y == 0) {
        RETURN_RESULT(Encode(1));
    } else if (((x == 1) || (x == -1)) && std::isinf(y)) {
        RETURN_RESULT(Encode(qt_qnan()));
    } else if (((x == 0) && copySign(1.0, x) == 1.0) && (y < 0)) {
        RETURN_RESULT(Encode(qInf()));
    } else if ((x == 0) && copySign(1.0, x) == -1.0) {
        if (y < 0) {
            if (std::fmod(-y, 2.0) == 1.0)
                RETURN_RESULT(Encode(-qt_inf()));
            else
                RETURN_RESULT(Encode(qt_inf()));
        } else if (y > 0) {
            if (std::fmod(y, 2.0) == 1.0)
                RETURN_RESULT(Encode(copySign(0, -1.0)));
            else
                RETURN_RESULT(Encode(0));
        }
    }

#ifdef Q_OS_AIX
    else if (qt_is_inf(x) && copySign(1.0, x) == -1.0) {
        if (y > 0) {
            if (std::fmod(y, 2.0) == 1.0)
                RETURN_RESULT(Encode(-qt_inf()));
            else
                RETURN_RESULT(Encode(qt_inf()));
        } else if (y < 0) {
            if (std::fmod(-y, 2.0) == 1.0)
                RETURN_RESULT(Encode(copySign(0, -1.0)));
            else
                RETURN_RESULT(Encode(0));
        }
    }
#endif
    else {
        RETURN_RESULT(Encode(std::pow(x, y)));
    }
    // ###
    RETURN_RESULT(Encode(qt_qnan()));
}

Q_GLOBAL_STATIC(QThreadStorage<bool *>, seedCreatedStorage);

void MathObject::method_random(const BuiltinFunction *, Scope &scope, CallData *)
{
    if (!seedCreatedStorage()->hasLocalData()) {
        int msecs = QTime(0,0,0).msecsTo(QTime::currentTime());
        Q_ASSERT(msecs >= 0);
        qsrand(uint(uint(msecs) ^ reinterpret_cast<quintptr>(scope.engine)));
        seedCreatedStorage()->setLocalData(new bool(true));
    }
    // rand()/qrand() return a value where the upperbound is RAND_MAX inclusive. So, instead of
    // dividing by RAND_MAX (which would return 0..RAND_MAX inclusive), we divide by RAND_MAX + 1.
    qint64 upperLimit = qint64(RAND_MAX) + 1;
    RETURN_RESULT(Encode(qrand() / double(upperLimit)));
}

void MathObject::method_round(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    v = copySign(std::floor(v + 0.5), v);
    RETURN_RESULT(Encode(v));
}

void MathObject::method_sign(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();

    if (std::isnan(v))
        RETURN_RESULT(Encode(qt_qnan()));

    if (qIsNull(v))
        RETURN_RESULT(Encode(v));

    RETURN_RESULT(Encode(std::signbit(v) ? -1 : 1));
}

void MathObject::method_sin(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    RETURN_RESULT(Encode(std::sin(v)));
}

void MathObject::method_sqrt(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    RETURN_RESULT(Encode(std::sqrt(v)));
}

void MathObject::method_tan(const BuiltinFunction *, Scope &scope, CallData *callData)
{
    double v = callData->argc ? callData->args[0].toNumber() : qt_qnan();
    if (v == 0.0)
        RETURN_RESULT(Encode(v));
    else
        RETURN_RESULT(Encode(std::tan(v)));
}

