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

ReturnedValue MathObject::method_abs(CallContext *context)
{
    if (!context->argc())
        return Encode(qt_qnan());

    if (context->args()[0].isInteger()) {
        int i = context->args()[0].integerValue();
        return Encode(i < 0 ? - i : i);
    }

    double v = context->args()[0].toNumber();
    if (v == 0) // 0 | -0
        return Encode(0);

    return Encode(v < 0 ? -v : v);
}

ReturnedValue MathObject::method_acos(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : 2;
    if (v > 1)
        return Encode(qt_qnan());

    return Encode(std::acos(v));
}

ReturnedValue MathObject::method_asin(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : 2;
    if (v > 1)
        return Encode(qt_qnan());
    else
        return Encode(std::asin(v));
}

ReturnedValue MathObject::method_atan(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    if (v == 0.0)
        return Encode(v);
    else
        return Encode(std::atan(v));
}

ReturnedValue MathObject::method_atan2(CallContext *context)
{
    double v1 = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    double v2 = context->argc() > 1 ? context->args()[1].toNumber() : qt_qnan();

    if ((v1 < 0) && qt_is_finite(v1) && qt_is_inf(v2) && (copySign(1.0, v2) == 1.0))
        return Encode(copySign(0, -1.0));

    if ((v1 == 0.0) && (v2 == 0.0)) {
        if ((copySign(1.0, v1) == 1.0) && (copySign(1.0, v2) == -1.0)) {
            return Encode(M_PI);
        } else if ((copySign(1.0, v1) == -1.0) && (copySign(1.0, v2) == -1.0)) {
            return Encode(-M_PI);
        }
    }
    return Encode(std::atan2(v1, v2));
}

ReturnedValue MathObject::method_ceil(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    if (v < 0.0 && v > -1.0)
        return Encode(copySign(0, -1.0));
    else
        return Encode(std::ceil(v));
}

ReturnedValue MathObject::method_cos(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    return Encode(std::cos(v));
}

ReturnedValue MathObject::method_exp(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    if (qt_is_inf(v)) {
        if (copySign(1.0, v) == -1.0)
            return Encode(0);
        else
            return Encode(qt_inf());
    } else {
        return Encode(std::exp(v));
    }
}

ReturnedValue MathObject::method_floor(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    return Encode(std::floor(v));
}

ReturnedValue MathObject::method_log(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    if (v < 0)
        return Encode(qt_qnan());
    else
        return Encode(std::log(v));
}

ReturnedValue MathObject::method_max(CallContext *context)
{
    double mx = -qt_inf();
    for (int i = 0; i < context->argc(); ++i) {
        double x = context->args()[i].toNumber();
        if (x > mx || std::isnan(x))
            mx = x;
    }
    return Encode(mx);
}

ReturnedValue MathObject::method_min(CallContext *context)
{
    double mx = qt_inf();
    for (int i = 0; i < context->argc(); ++i) {
        double x = context->args()[i].toNumber();
        if ((x == 0 && mx == x && copySign(1.0, x) == -1.0)
                || (x < mx) || std::isnan(x)) {
            mx = x;
        }
    }
    return Encode(mx);
}

ReturnedValue MathObject::method_pow(CallContext *context)
{
    double x = context->argc() > 0 ? context->args()[0].toNumber() : qt_qnan();
    double y = context->argc() > 1 ? context->args()[1].toNumber() : qt_qnan();

    if (std::isnan(y))
        return Encode(qt_qnan());

    if (y == 0) {
        return Encode(1);
    } else if (((x == 1) || (x == -1)) && std::isinf(y)) {
        return Encode(qt_qnan());
    } else if (((x == 0) && copySign(1.0, x) == 1.0) && (y < 0)) {
        return Encode(qInf());
    } else if ((x == 0) && copySign(1.0, x) == -1.0) {
        if (y < 0) {
            if (std::fmod(-y, 2.0) == 1.0)
                return Encode(-qt_inf());
            else
                return Encode(qt_inf());
        } else if (y > 0) {
            if (std::fmod(y, 2.0) == 1.0)
                return Encode(copySign(0, -1.0));
            else
                return Encode(0);
        }
    }

#ifdef Q_OS_AIX
    else if (qt_is_inf(x) && copySign(1.0, x) == -1.0) {
        if (y > 0) {
            if (std::fmod(y, 2.0) == 1.0)
                return Encode(-qt_inf());
            else
                return Encode(qt_inf());
        } else if (y < 0) {
            if (std::fmod(-y, 2.0) == 1.0)
                return Encode(copySign(0, -1.0));
            else
                return Encode(0);
        }
    }
#endif
    else {
        return Encode(std::pow(x, y));
    }
    // ###
    return Encode(qt_qnan());
}

Q_GLOBAL_STATIC(QThreadStorage<bool *>, seedCreatedStorage);

ReturnedValue MathObject::method_random(CallContext *context)
{
    if (!seedCreatedStorage()->hasLocalData()) {
        int msecs = QTime(0,0,0).msecsTo(QTime::currentTime());
        Q_ASSERT(msecs >= 0);
        qsrand(uint(uint(msecs) ^ reinterpret_cast<quintptr>(context)));
        seedCreatedStorage()->setLocalData(new bool(true));
    }
    // rand()/qrand() return a value where the upperbound is RAND_MAX inclusive. So, instead of
    // dividing by RAND_MAX (which would return 0..RAND_MAX inclusive), we divide by RAND_MAX + 1.
    qint64 upperLimit = qint64(RAND_MAX) + 1;
    return Encode(qrand() / double(upperLimit));
}

ReturnedValue MathObject::method_round(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    v = copySign(std::floor(v + 0.5), v);
    return Encode(v);
}

ReturnedValue MathObject::method_sign(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();

    if (std::isnan(v))
        return Encode(qt_qnan());

    if (qIsNull(v))
        return v;

    return Encode(std::signbit(v) ? -1 : 1);
}

ReturnedValue MathObject::method_sin(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    return Encode(std::sin(v));
}

ReturnedValue MathObject::method_sqrt(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    return Encode(std::sqrt(v));
}

ReturnedValue MathObject::method_tan(CallContext *context)
{
    double v = context->argc() ? context->args()[0].toNumber() : qt_qnan();
    if (v == 0.0)
        return Encode(v);
    else
        return Encode(std::tan(v));
}

