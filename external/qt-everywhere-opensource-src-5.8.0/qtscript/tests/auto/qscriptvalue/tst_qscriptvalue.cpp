/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "tst_qscriptvalue.h"
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE
extern bool qt_script_isJITEnabled();
QT_END_NAMESPACE

tst_QScriptValue::tst_QScriptValue()
    : engine(0)
{
}

tst_QScriptValue::~tst_QScriptValue()
{
    if (engine)
        delete engine;
}

void tst_QScriptValue::ctor_invalid()
{
    QScriptEngine eng;
    {
        QScriptValue v;
        QCOMPARE(v.isValid(), false);
        QCOMPARE(v.engine(), (QScriptEngine *)0);
    }
}

void tst_QScriptValue::ctor_undefinedWithEngine()
{
    QScriptEngine eng;
    {
        QScriptValue v(&eng, QScriptValue::UndefinedValue);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isUndefined(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QScriptValue::ctor_nullWithEngine()
{
    QScriptEngine eng;
    {
        QScriptValue v(&eng, QScriptValue::NullValue);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNull(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QScriptValue::ctor_boolWithEngine()
{
    QScriptEngine eng;
    {
        QScriptValue v(&eng, false);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isBoolean(), true);
        QCOMPARE(v.isBool(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toBoolean(), false);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QScriptValue::ctor_intWithEngine()
{
    QScriptEngine eng;
    {
        QScriptValue v(&eng, int(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QScriptValue::ctor_int()
{
    {
        QScriptValue v(int(0x43211234));
        QVERIFY(v.isNumber());
        QCOMPARE(v.toInt32(), 0x43211234);
    }
    {
        QScriptValue v(int(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QScriptEngine *)0);
    }
}

void tst_QScriptValue::ctor_uintWithEngine()
{
    QScriptEngine eng;
    {
        QScriptValue v(&eng, uint(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QScriptValue::ctor_uint()
{
    {
        QScriptValue v(uint(0x43211234));
        QVERIFY(v.isNumber());
        QCOMPARE(v.toUInt32(), uint(0x43211234));
    }
    {
        QScriptValue v(uint(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QScriptEngine *)0);
    }
}

void tst_QScriptValue::ctor_floatWithEngine()
{
    QScriptEngine eng;
    {
        QScriptValue v(&eng, 1.0);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QScriptValue::ctor_float()
{
    {
        QScriptValue v(12345678910.5);
        QVERIFY(v.isNumber());
        QCOMPARE(v.toNumber(), 12345678910.5);
    }
    {
        QScriptValue v(1.0);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QScriptEngine *)0);
    }
}

void tst_QScriptValue::ctor_stringWithEngine()
{
    QScriptEngine eng;
    {
        QScriptValue v(&eng, "ciao");
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QScriptValue::ctor_string()
{
    {
        QScriptValue v(QString("ciao"));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), (QScriptEngine *)0);
    }
    {
        QScriptValue v("ciao");
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), (QScriptEngine *)0);
    }
}

void tst_QScriptValue::ctor_copyAndAssignWithEngine()
{
    QScriptEngine eng;
    // copy constructor, operator=
    {
        QScriptValue v(&eng, 1.0);
        QScriptValue v2(v);
        QCOMPARE(v2.strictlyEquals(v), true);
        QCOMPARE(v2.engine(), &eng);

        QScriptValue v3(v);
        QCOMPARE(v3.strictlyEquals(v), true);
        QCOMPARE(v3.strictlyEquals(v2), true);
        QCOMPARE(v3.engine(), &eng);

        QScriptValue v4(&eng, 2.0);
        QCOMPARE(v4.strictlyEquals(v), false);
        v3 = v4;
        QCOMPARE(v3.strictlyEquals(v), false);
        QCOMPARE(v3.strictlyEquals(v4), true);

        v2 = QScriptValue();
        QCOMPARE(v2.strictlyEquals(v), false);
        QCOMPARE(v.toNumber(), 1.0);

        QScriptValue v5(v);
        QCOMPARE(v5.strictlyEquals(v), true);
        v = QScriptValue();
        QCOMPARE(v5.strictlyEquals(v), false);
        QCOMPARE(v5.toNumber(), 1.0);
    }
}

void tst_QScriptValue::ctor_undefined()
{
    QScriptValue v(QScriptValue::UndefinedValue);
    QCOMPARE(v.isValid(), true);
    QCOMPARE(v.isUndefined(), true);
    QCOMPARE(v.isObject(), false);
    QCOMPARE(v.engine(), (QScriptEngine *)0);
}

void tst_QScriptValue::ctor_null()
{
    QScriptValue v(QScriptValue::NullValue);
    QCOMPARE(v.isValid(), true);
    QCOMPARE(v.isNull(), true);
    QCOMPARE(v.isObject(), false);
    QCOMPARE(v.engine(), (QScriptEngine *)0);
}

void tst_QScriptValue::ctor_bool()
{
    QScriptValue v(false);
    QCOMPARE(v.isValid(), true);
    QCOMPARE(v.isBoolean(), true);
    QCOMPARE(v.isBool(), true);
    QCOMPARE(v.isObject(), false);
    QCOMPARE(v.toBoolean(), false);
    QCOMPARE(v.engine(), (QScriptEngine *)0);
}

void tst_QScriptValue::ctor_copyAndAssign()
{
    QScriptValue v(1.0);
    QScriptValue v2(v);
    QCOMPARE(v2.strictlyEquals(v), true);
    QCOMPARE(v2.engine(), (QScriptEngine *)0);

    QScriptValue v3(v);
    QCOMPARE(v3.strictlyEquals(v), true);
    QCOMPARE(v3.strictlyEquals(v2), true);
    QCOMPARE(v3.engine(), (QScriptEngine *)0);

    QScriptValue v4(2.0);
    QCOMPARE(v4.strictlyEquals(v), false);
    v3 = v4;
    QCOMPARE(v3.strictlyEquals(v), false);
    QCOMPARE(v3.strictlyEquals(v4), true);

    v2 = QScriptValue();
    QCOMPARE(v2.strictlyEquals(v), false);
    QCOMPARE(v.toNumber(), 1.0);

    QScriptValue v5(v);
    QCOMPARE(v5.strictlyEquals(v), true);
    v = QScriptValue();
    QCOMPARE(v5.strictlyEquals(v), false);
    QCOMPARE(v5.toNumber(), 1.0);
}

void tst_QScriptValue::ctor_nullEngine()
{
    // 0 engine
    QVERIFY(QScriptValue(0, QScriptValue::UndefinedValue).isUndefined());
    QVERIFY(QScriptValue(0, QScriptValue::NullValue).isNull());
    QVERIFY(QScriptValue(0, false).isBool());
    QVERIFY(QScriptValue(0, int(1)).isNumber());
    QVERIFY(QScriptValue(0, uint(1)).isNumber());
    QVERIFY(QScriptValue(0, 1.0).isNumber());
    QVERIFY(QScriptValue(0, "ciao").isString());
    QVERIFY(QScriptValue(0, QString("ciao")).isString());
}

static QScriptValue myFunction(QScriptContext *, QScriptEngine *eng)
{
    return eng->undefinedValue();
}

void tst_QScriptValue::toString()
{
    QScriptEngine eng;

    QScriptValue undefined = eng.undefinedValue();
    QCOMPARE(undefined.toString(), QString("undefined"));
    QCOMPARE(qscriptvalue_cast<QString>(undefined), QString());

    QScriptValue null = eng.nullValue();
    QCOMPARE(null.toString(), QString("null"));
    QCOMPARE(qscriptvalue_cast<QString>(null), QString());

    {
        QScriptValue falskt = QScriptValue(&eng, false);
        QCOMPARE(falskt.toString(), QString("false"));
        QCOMPARE(qscriptvalue_cast<QString>(falskt), QString("false"));

        QScriptValue sant = QScriptValue(&eng, true);
        QCOMPARE(sant.toString(), QString("true"));
        QCOMPARE(qscriptvalue_cast<QString>(sant), QString("true"));
    }
    {
        QScriptValue number = QScriptValue(&eng, 123);
        QCOMPARE(number.toString(), QString("123"));
        QCOMPARE(qscriptvalue_cast<QString>(number), QString("123"));
    }
    {
        QScriptValue number = QScriptValue(&eng, 6.37e-8);
        QCOMPARE(number.toString(), QString("6.37e-8"));
    }
    {
        QScriptValue number = QScriptValue(&eng, -6.37e-8);
        QCOMPARE(number.toString(), QString("-6.37e-8"));

        QScriptValue str = QScriptValue(&eng, QString("ciao"));
        QCOMPARE(str.toString(), QString("ciao"));
        QCOMPARE(qscriptvalue_cast<QString>(str), QString("ciao"));
    }

    QScriptValue object = eng.newObject();
    QCOMPARE(object.toString(), QString("[object Object]"));
    QCOMPARE(qscriptvalue_cast<QString>(object), QString("[object Object]"));

    QScriptValue fun = eng.newFunction(myFunction);
    QCOMPARE(fun.toString(), QString("function () {\n    [native code]\n}"));
    QCOMPARE(qscriptvalue_cast<QString>(fun), QString("function () {\n    [native code]\n}"));

    // toString() that throws exception
    {
        QScriptValue objectObject = eng.evaluate(
            "(function(){"
            "  o = { };"
            "  o.toString = function() { throw new Error('toString'); };"
            "  return o;"
            "})()");
        QCOMPARE(objectObject.toString(), QLatin1String("Error: toString"));
        QVERIFY(eng.hasUncaughtException());
        QCOMPARE(eng.uncaughtException().toString(), QLatin1String("Error: toString"));
    }
    {
        eng.clearExceptions();
        QScriptValue objectObject = eng.evaluate(
            "(function(){"
            "  var f = function() {};"
            "  f.prototype = Date;"
            "  return new f;"
            "})()");
        QVERIFY(!eng.hasUncaughtException());
        QVERIFY(objectObject.isObject());
        QCOMPARE(objectObject.toString(), QString::fromLatin1("TypeError: Function.prototype.toString called on incompatible object"));
        QVERIFY(eng.hasUncaughtException());
        eng.clearExceptions();
    }

    QScriptValue inv = QScriptValue();
    QCOMPARE(inv.toString(), QString());

    // V2 constructors
    {
        QScriptValue falskt = QScriptValue(false);
        QCOMPARE(falskt.toString(), QString("false"));
        QCOMPARE(qscriptvalue_cast<QString>(falskt), QString("false"));

        QScriptValue sant = QScriptValue(true);
        QCOMPARE(sant.toString(), QString("true"));
        QCOMPARE(qscriptvalue_cast<QString>(sant), QString("true"));

        QScriptValue number = QScriptValue(123);
        QCOMPARE(number.toString(), QString("123"));
        QCOMPARE(qscriptvalue_cast<QString>(number), QString("123"));

        QScriptValue number2(int(0x43211234));
        QCOMPARE(number2.toString(), QString("1126240820"));

        QScriptValue str = QScriptValue(QString("ciao"));
        QCOMPARE(str.toString(), QString("ciao"));
        QCOMPARE(qscriptvalue_cast<QString>(str), QString("ciao"));
    }

    // variant should use internal valueOf(), then fall back to QVariant::toString(),
    // then fall back to "QVariant(typename)"
    QScriptValue variant = eng.newVariant(123);
    QVERIFY(variant.isVariant());
    QCOMPARE(variant.toString(), QString::fromLatin1("123"));
    variant = eng.newVariant(QByteArray("hello"));
    QVERIFY(variant.isVariant());
    QCOMPARE(variant.toString(), QString::fromLatin1("hello"));
    variant = eng.newVariant(QVariant(QPoint(10, 20)));
    QVERIFY(variant.isVariant());
    QCOMPARE(variant.toString(), QString::fromLatin1("QVariant(QPoint)"));
    variant = eng.newVariant(QUrl());
    QVERIFY(variant.toString().isEmpty());
}

void tst_QScriptValue::toNumber()
{
    QScriptEngine eng;

    QScriptValue undefined = eng.undefinedValue();
    QCOMPARE(qIsNaN(undefined.toNumber()), true);
    QCOMPARE(qIsNaN(qscriptvalue_cast<qsreal>(undefined)), true);

    QScriptValue null = eng.nullValue();
    QCOMPARE(null.toNumber(), 0.0);
    QCOMPARE(qscriptvalue_cast<qsreal>(null), 0.0);

    {
        QScriptValue falskt = QScriptValue(&eng, false);
        QCOMPARE(falskt.toNumber(), 0.0);
        QCOMPARE(qscriptvalue_cast<qsreal>(falskt), 0.0);

        QScriptValue sant = QScriptValue(&eng, true);
        QCOMPARE(sant.toNumber(), 1.0);
        QCOMPARE(qscriptvalue_cast<qsreal>(sant), 1.0);

        QScriptValue number = QScriptValue(&eng, 123.0);
        QCOMPARE(number.toNumber(), 123.0);
        QCOMPARE(qscriptvalue_cast<qsreal>(number), 123.0);

        QScriptValue str = QScriptValue(&eng, QString("ciao"));
        QCOMPARE(qIsNaN(str.toNumber()), true);
        QCOMPARE(qIsNaN(qscriptvalue_cast<qsreal>(str)), true);

        QScriptValue str2 = QScriptValue(&eng, QString("123"));
        QCOMPARE(str2.toNumber(), 123.0);
        QCOMPARE(qscriptvalue_cast<qsreal>(str2), 123.0);
    }

    QScriptValue object = eng.newObject();
    QCOMPARE(qIsNaN(object.toNumber()), true);
    QCOMPARE(qIsNaN(qscriptvalue_cast<qsreal>(object)), true);

    QScriptValue fun = eng.newFunction(myFunction);
    QCOMPARE(qIsNaN(fun.toNumber()), true);
    QCOMPARE(qIsNaN(qscriptvalue_cast<qsreal>(fun)), true);

    QScriptValue inv = QScriptValue();
    QCOMPARE(inv.toNumber(), 0.0);
    QCOMPARE(qscriptvalue_cast<qsreal>(inv), 0.0);

    // V2 constructors
    {
        QScriptValue falskt = QScriptValue(false);
        QCOMPARE(falskt.toNumber(), 0.0);
        QCOMPARE(qscriptvalue_cast<qsreal>(falskt), 0.0);

        QScriptValue sant = QScriptValue(true);
        QCOMPARE(sant.toNumber(), 1.0);
        QCOMPARE(qscriptvalue_cast<qsreal>(sant), 1.0);

        QScriptValue number = QScriptValue(123.0);
        QCOMPARE(number.toNumber(), 123.0);
        QCOMPARE(qscriptvalue_cast<qsreal>(number), 123.0);

        QScriptValue number2(int(0x43211234));
        QCOMPARE(number2.toNumber(), 1126240820.0);

        QScriptValue str = QScriptValue(QString("ciao"));
        QCOMPARE(qIsNaN(str.toNumber()), true);
        QCOMPARE(qIsNaN(qscriptvalue_cast<qsreal>(str)), true);

        QScriptValue str2 = QScriptValue(QString("123"));
        QCOMPARE(str2.toNumber(), 123.0);
        QCOMPARE(qscriptvalue_cast<qsreal>(str2), 123.0);
    }
}

void tst_QScriptValue::toBoolean() // deprecated
{
    QScriptEngine eng;

    QScriptValue undefined = eng.undefinedValue();
    QCOMPARE(undefined.toBoolean(), false);
    QCOMPARE(qscriptvalue_cast<bool>(undefined), false);

    QScriptValue null = eng.nullValue();
    QCOMPARE(null.toBoolean(), false);
    QCOMPARE(qscriptvalue_cast<bool>(null), false);

    {
        QScriptValue falskt = QScriptValue(&eng, false);
        QCOMPARE(falskt.toBoolean(), false);
        QCOMPARE(qscriptvalue_cast<bool>(falskt), false);

        QScriptValue sant = QScriptValue(&eng, true);
        QCOMPARE(sant.toBoolean(), true);
        QCOMPARE(qscriptvalue_cast<bool>(sant), true);

        QScriptValue number = QScriptValue(&eng, 0.0);
        QCOMPARE(number.toBoolean(), false);
        QCOMPARE(qscriptvalue_cast<bool>(number), false);

        QScriptValue number2 = QScriptValue(&eng, qSNaN());
        QCOMPARE(number2.toBoolean(), false);
        QCOMPARE(qscriptvalue_cast<bool>(number2), false);

        QScriptValue number3 = QScriptValue(&eng, 123.0);
        QCOMPARE(number3.toBoolean(), true);
        QCOMPARE(qscriptvalue_cast<bool>(number3), true);

        QScriptValue number4 = QScriptValue(&eng, -456.0);
        QCOMPARE(number4.toBoolean(), true);
        QCOMPARE(qscriptvalue_cast<bool>(number4), true);

        QScriptValue str = QScriptValue(&eng, QString(""));
        QCOMPARE(str.toBoolean(), false);
        QCOMPARE(qscriptvalue_cast<bool>(str), false);

        QScriptValue str2 = QScriptValue(&eng, QString("123"));
        QCOMPARE(str2.toBoolean(), true);
        QCOMPARE(qscriptvalue_cast<bool>(str2), true);
    }

    QScriptValue object = eng.newObject();
    QCOMPARE(object.toBoolean(), true);
    QCOMPARE(qscriptvalue_cast<bool>(object), true);

    QScriptValue fun = eng.newFunction(myFunction);
    QCOMPARE(fun.toBoolean(), true);
    QCOMPARE(qscriptvalue_cast<bool>(fun), true);

    QScriptValue inv = QScriptValue();
    QCOMPARE(inv.toBoolean(), false);
    QCOMPARE(qscriptvalue_cast<bool>(inv), false);

    // V2 constructors
    {
        QScriptValue falskt = QScriptValue(false);
        QCOMPARE(falskt.toBoolean(), false);
        QCOMPARE(qscriptvalue_cast<bool>(falskt), false);

        QScriptValue sant = QScriptValue(true);
        QCOMPARE(sant.toBoolean(), true);
        QCOMPARE(qscriptvalue_cast<bool>(sant), true);

        QScriptValue number = QScriptValue(0.0);
        QCOMPARE(number.toBoolean(), false);
        QCOMPARE(qscriptvalue_cast<bool>(number), false);

        QScriptValue number2 = QScriptValue(qSNaN());
        QCOMPARE(number2.toBoolean(), false);
        QCOMPARE(qscriptvalue_cast<bool>(number2), false);

        QScriptValue number3 = QScriptValue(123.0);
        QCOMPARE(number3.toBoolean(), true);
        QCOMPARE(qscriptvalue_cast<bool>(number3), true);

        QScriptValue number4 = QScriptValue(-456.0);
        QCOMPARE(number4.toBoolean(), true);
        QCOMPARE(qscriptvalue_cast<bool>(number4), true);

        QScriptValue number5 = QScriptValue(0x43211234);
        QCOMPARE(number5.toBoolean(), true);

        QScriptValue str = QScriptValue(QString(""));
        QCOMPARE(str.toBoolean(), false);
        QCOMPARE(qscriptvalue_cast<bool>(str), false);

        QScriptValue str2 = QScriptValue(QString("123"));
        QCOMPARE(str2.toBoolean(), true);
        QCOMPARE(qscriptvalue_cast<bool>(str2), true);
    }
}

void tst_QScriptValue::toBool()
{
    QScriptEngine eng;

    QScriptValue undefined = eng.undefinedValue();
    QCOMPARE(undefined.toBool(), false);
    QCOMPARE(qscriptvalue_cast<bool>(undefined), false);

    QScriptValue null = eng.nullValue();
    QCOMPARE(null.toBool(), false);
    QCOMPARE(qscriptvalue_cast<bool>(null), false);

    {
        QScriptValue falskt = QScriptValue(&eng, false);
        QCOMPARE(falskt.toBool(), false);
        QCOMPARE(qscriptvalue_cast<bool>(falskt), false);

        QScriptValue sant = QScriptValue(&eng, true);
        QCOMPARE(sant.toBool(), true);
        QCOMPARE(qscriptvalue_cast<bool>(sant), true);

        QScriptValue number = QScriptValue(&eng, 0.0);
        QCOMPARE(number.toBool(), false);
        QCOMPARE(qscriptvalue_cast<bool>(number), false);

        QScriptValue number2 = QScriptValue(&eng, qSNaN());
        QCOMPARE(number2.toBool(), false);
        QCOMPARE(qscriptvalue_cast<bool>(number2), false);

        QScriptValue number3 = QScriptValue(&eng, 123.0);
        QCOMPARE(number3.toBool(), true);
        QCOMPARE(qscriptvalue_cast<bool>(number3), true);

        QScriptValue number4 = QScriptValue(&eng, -456.0);
        QCOMPARE(number4.toBool(), true);
        QCOMPARE(qscriptvalue_cast<bool>(number4), true);

        QScriptValue str = QScriptValue(&eng, QString(""));
        QCOMPARE(str.toBool(), false);
        QCOMPARE(qscriptvalue_cast<bool>(str), false);

        QScriptValue str2 = QScriptValue(&eng, QString("123"));
        QCOMPARE(str2.toBool(), true);
        QCOMPARE(qscriptvalue_cast<bool>(str2), true);
    }

    QScriptValue object = eng.newObject();
    QCOMPARE(object.toBool(), true);
    QCOMPARE(qscriptvalue_cast<bool>(object), true);

    QScriptValue fun = eng.newFunction(myFunction);
    QCOMPARE(fun.toBool(), true);
    QCOMPARE(qscriptvalue_cast<bool>(fun), true);

    QScriptValue inv = QScriptValue();
    QCOMPARE(inv.toBool(), false);
    QCOMPARE(qscriptvalue_cast<bool>(inv), false);

    // V2 constructors
    {
        QScriptValue falskt = QScriptValue(false);
        QCOMPARE(falskt.toBool(), false);
        QCOMPARE(qscriptvalue_cast<bool>(falskt), false);

        QScriptValue sant = QScriptValue(true);
        QCOMPARE(sant.toBool(), true);
        QCOMPARE(qscriptvalue_cast<bool>(sant), true);

        QScriptValue number = QScriptValue(0.0);
        QCOMPARE(number.toBool(), false);
        QCOMPARE(qscriptvalue_cast<bool>(number), false);

        QScriptValue number2 = QScriptValue(qSNaN());
        QCOMPARE(number2.toBool(), false);
        QCOMPARE(qscriptvalue_cast<bool>(number2), false);

        QScriptValue number3 = QScriptValue(123.0);
        QCOMPARE(number3.toBool(), true);
        QCOMPARE(qscriptvalue_cast<bool>(number3), true);

        QScriptValue number4 = QScriptValue(-456.0);
        QCOMPARE(number4.toBool(), true);
        QCOMPARE(qscriptvalue_cast<bool>(number4), true);

        QScriptValue number5 = QScriptValue(0x43211234);
        QCOMPARE(number5.toBool(), true);

        QScriptValue str = QScriptValue(QString(""));
        QCOMPARE(str.toBool(), false);
        QCOMPARE(qscriptvalue_cast<bool>(str), false);

        QScriptValue str2 = QScriptValue(QString("123"));
        QCOMPARE(str2.toBool(), true);
        QCOMPARE(qscriptvalue_cast<bool>(str2), true);
    }
}

void tst_QScriptValue::toInteger()
{
    QScriptEngine eng;

    {
        QScriptValue number = QScriptValue(&eng, 123.0);
        QCOMPARE(number.toInteger(), 123.0);

        QScriptValue number2 = QScriptValue(&eng, qSNaN());
        QCOMPARE(number2.toInteger(), 0.0);

        QScriptValue number3 = QScriptValue(&eng, qInf());
        QCOMPARE(qIsInf(number3.toInteger()), true);

        QScriptValue number4 = QScriptValue(&eng, 0.5);
        QCOMPARE(number4.toInteger(), 0.0);

        QScriptValue number5 = QScriptValue(&eng, 123.5);
        QCOMPARE(number5.toInteger(), 123.0);

        QScriptValue number6 = QScriptValue(&eng, -456.5);
        QCOMPARE(number6.toInteger(), -456.0);

        QScriptValue str = QScriptValue(&eng, "123.0");
        QCOMPARE(str.toInteger(), 123.0);

        QScriptValue str2 = QScriptValue(&eng, "NaN");
        QCOMPARE(str2.toInteger(), 0.0);

        QScriptValue str3 = QScriptValue(&eng, "Infinity");
        QCOMPARE(qIsInf(str3.toInteger()), true);

        QScriptValue str4 = QScriptValue(&eng, "0.5");
        QCOMPARE(str4.toInteger(), 0.0);

        QScriptValue str5 = QScriptValue(&eng, "123.5");
        QCOMPARE(str5.toInteger(), 123.0);

        QScriptValue str6 = QScriptValue(&eng, "-456.5");
        QCOMPARE(str6.toInteger(), -456.0);
    }
    // V2 constructors
    {
        QScriptValue number = QScriptValue(123.0);
        QCOMPARE(number.toInteger(), 123.0);

        QScriptValue number2 = QScriptValue(qSNaN());
        QCOMPARE(number2.toInteger(), 0.0);

        QScriptValue number3 = QScriptValue(qInf());
        QCOMPARE(qIsInf(number3.toInteger()), true);

        QScriptValue number4 = QScriptValue(0.5);
        QCOMPARE(number4.toInteger(), 0.0);

        QScriptValue number5 = QScriptValue(123.5);
        QCOMPARE(number5.toInteger(), 123.0);

        QScriptValue number6 = QScriptValue(-456.5);
        QCOMPARE(number6.toInteger(), -456.0);

        QScriptValue number7 = QScriptValue(0x43211234);
        QCOMPARE(number7.toInteger(), qsreal(0x43211234));

        QScriptValue str = QScriptValue("123.0");
        QCOMPARE(str.toInteger(), 123.0);

        QScriptValue str2 = QScriptValue("NaN");
        QCOMPARE(str2.toInteger(), 0.0);

        QScriptValue str3 = QScriptValue("Infinity");
        QCOMPARE(qIsInf(str3.toInteger()), true);

        QScriptValue str4 = QScriptValue("0.5");
        QCOMPARE(str4.toInteger(), 0.0);

        QScriptValue str5 = QScriptValue("123.5");
        QCOMPARE(str5.toInteger(), 123.0);

        QScriptValue str6 = QScriptValue("-456.5");
        QCOMPARE(str6.toInteger(), -456.0);
    }

    QScriptValue inv;
    QCOMPARE(inv.toInteger(), 0.0);
}

void tst_QScriptValue::toInt32()
{
    QScriptEngine eng;

    {
        QScriptValue zer0 = QScriptValue(&eng, 0.0);
        QCOMPARE(zer0.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(zer0), 0);

        QScriptValue number = QScriptValue(&eng, 123.0);
        QCOMPARE(number.toInt32(), 123);
        QCOMPARE(qscriptvalue_cast<qint32>(number), 123);

        QScriptValue number2 = QScriptValue(&eng, qSNaN());
        QCOMPARE(number2.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(number2), 0);

        QScriptValue number3 = QScriptValue(&eng, +qInf());
        QCOMPARE(number3.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(number3), 0);

        QScriptValue number3_2 = QScriptValue(&eng, -qInf());
        QCOMPARE(number3_2.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(number3_2), 0);

        QScriptValue number4 = QScriptValue(&eng, 0.5);
        QCOMPARE(number4.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(number4), 0);

        QScriptValue number5 = QScriptValue(&eng, 123.5);
        QCOMPARE(number5.toInt32(), 123);
        QCOMPARE(qscriptvalue_cast<qint32>(number5), 123);

        QScriptValue number6 = QScriptValue(&eng, -456.5);
        QCOMPARE(number6.toInt32(), -456);
        QCOMPARE(qscriptvalue_cast<qint32>(number6), -456);

        QScriptValue str = QScriptValue(&eng, "123.0");
        QCOMPARE(str.toInt32(), 123);
        QCOMPARE(qscriptvalue_cast<qint32>(str), 123);

        QScriptValue str2 = QScriptValue(&eng, "NaN");
        QCOMPARE(str2.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(str2), 0);

        QScriptValue str3 = QScriptValue(&eng, "Infinity");
        QCOMPARE(str3.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(str3), 0);

        QScriptValue str3_2 = QScriptValue(&eng, "-Infinity");
        QCOMPARE(str3_2.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(str3_2), 0);

        QScriptValue str4 = QScriptValue(&eng, "0.5");
        QCOMPARE(str4.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(str4), 0);

        QScriptValue str5 = QScriptValue(&eng, "123.5");
        QCOMPARE(str5.toInt32(), 123);
        QCOMPARE(qscriptvalue_cast<qint32>(str5), 123);

        QScriptValue str6 = QScriptValue(&eng, "-456.5");
        QCOMPARE(str6.toInt32(), -456);
        QCOMPARE(qscriptvalue_cast<qint32>(str6), -456);
    }
    // V2 constructors
    {
        QScriptValue zer0 = QScriptValue(0.0);
        QCOMPARE(zer0.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(zer0), 0);

        QScriptValue number = QScriptValue(123.0);
        QCOMPARE(number.toInt32(), 123);
        QCOMPARE(qscriptvalue_cast<qint32>(number), 123);

        QScriptValue number2 = QScriptValue(qSNaN());
        QCOMPARE(number2.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(number2), 0);

        QScriptValue number3 = QScriptValue(+qInf());
        QCOMPARE(number3.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(number3), 0);

        QScriptValue number3_2 = QScriptValue(-qInf());
        QCOMPARE(number3_2.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(number3_2), 0);

        QScriptValue number4 = QScriptValue(0.5);
        QCOMPARE(number4.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(number4), 0);

        QScriptValue number5 = QScriptValue(123.5);
        QCOMPARE(number5.toInt32(), 123);
        QCOMPARE(qscriptvalue_cast<qint32>(number5), 123);

        QScriptValue number6 = QScriptValue(-456.5);
        QCOMPARE(number6.toInt32(), -456);
        QCOMPARE(qscriptvalue_cast<qint32>(number6), -456);

        QScriptValue number7 = QScriptValue(0x43211234);
        QCOMPARE(number7.toInt32(), 0x43211234);

        QScriptValue str = QScriptValue("123.0");
        QCOMPARE(str.toInt32(), 123);
        QCOMPARE(qscriptvalue_cast<qint32>(str), 123);

        QScriptValue str2 = QScriptValue("NaN");
        QCOMPARE(str2.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(str2), 0);

        QScriptValue str3 = QScriptValue("Infinity");
        QCOMPARE(str3.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(str3), 0);

        QScriptValue str3_2 = QScriptValue("-Infinity");
        QCOMPARE(str3_2.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(str3_2), 0);

        QScriptValue str4 = QScriptValue("0.5");
        QCOMPARE(str4.toInt32(), 0);
        QCOMPARE(qscriptvalue_cast<qint32>(str4), 0);

        QScriptValue str5 = QScriptValue("123.5");
        QCOMPARE(str5.toInt32(), 123);
        QCOMPARE(qscriptvalue_cast<qint32>(str5), 123);

        QScriptValue str6 = QScriptValue("-456.5");
        QCOMPARE(str6.toInt32(), -456);
        QCOMPARE(qscriptvalue_cast<qint32>(str6), -456);
    }

    QScriptValue inv;
    QCOMPARE(inv.toInt32(), 0);
    QCOMPARE(qscriptvalue_cast<qint32>(inv), 0);
}

void tst_QScriptValue::toUInt32()
{
    QScriptEngine eng;

    {
        QScriptValue zer0 = QScriptValue(&eng, 0.0);
        QCOMPARE(zer0.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(zer0), quint32(0));

        QScriptValue number = QScriptValue(&eng, 123.0);
        QCOMPARE(number.toUInt32(), quint32(123));
        QCOMPARE(qscriptvalue_cast<quint32>(number), quint32(123));

        QScriptValue number2 = QScriptValue(&eng, qSNaN());
        QCOMPARE(number2.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(number2), quint32(0));

        QScriptValue number3 = QScriptValue(&eng, +qInf());
        QCOMPARE(number3.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(number3), quint32(0));

        QScriptValue number3_2 = QScriptValue(&eng, -qInf());
        QCOMPARE(number3_2.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(number3_2), quint32(0));

        QScriptValue number4 = QScriptValue(&eng, 0.5);
        QCOMPARE(number4.toUInt32(), quint32(0));

        QScriptValue number5 = QScriptValue(&eng, 123.5);
        QCOMPARE(number5.toUInt32(), quint32(123));

        QScriptValue number6 = QScriptValue(&eng, -456.5);
        QCOMPARE(number6.toUInt32(), quint32(-456));
        QCOMPARE(qscriptvalue_cast<quint32>(number6), quint32(-456));

        QScriptValue str = QScriptValue(&eng, "123.0");
        QCOMPARE(str.toUInt32(), quint32(123));
        QCOMPARE(qscriptvalue_cast<quint32>(str), quint32(123));

        QScriptValue str2 = QScriptValue(&eng, "NaN");
        QCOMPARE(str2.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(str2), quint32(0));

        QScriptValue str3 = QScriptValue(&eng, "Infinity");
        QCOMPARE(str3.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(str3), quint32(0));

        QScriptValue str3_2 = QScriptValue(&eng, "-Infinity");
        QCOMPARE(str3_2.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(str3_2), quint32(0));

        QScriptValue str4 = QScriptValue(&eng, "0.5");
        QCOMPARE(str4.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(str4), quint32(0));

        QScriptValue str5 = QScriptValue(&eng, "123.5");
        QCOMPARE(str5.toUInt32(), quint32(123));
        QCOMPARE(qscriptvalue_cast<quint32>(str5), quint32(123));

        QScriptValue str6 = QScriptValue(&eng, "-456.5");
        QCOMPARE(str6.toUInt32(), quint32(-456));
        QCOMPARE(qscriptvalue_cast<quint32>(str6), quint32(-456));
    }
    // V2 constructors
    {
        QScriptValue zer0 = QScriptValue(0.0);
        QCOMPARE(zer0.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(zer0), quint32(0));

        QScriptValue number = QScriptValue(123.0);
        QCOMPARE(number.toUInt32(), quint32(123));
        QCOMPARE(qscriptvalue_cast<quint32>(number), quint32(123));

        QScriptValue number2 = QScriptValue(qSNaN());
        QCOMPARE(number2.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(number2), quint32(0));

        QScriptValue number3 = QScriptValue(+qInf());
        QCOMPARE(number3.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(number3), quint32(0));

        QScriptValue number3_2 = QScriptValue(-qInf());
        QCOMPARE(number3_2.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(number3_2), quint32(0));

        QScriptValue number4 = QScriptValue(0.5);
        QCOMPARE(number4.toUInt32(), quint32(0));

        QScriptValue number5 = QScriptValue(123.5);
        QCOMPARE(number5.toUInt32(), quint32(123));

        QScriptValue number6 = QScriptValue(-456.5);
        QCOMPARE(number6.toUInt32(), quint32(-456));
        QCOMPARE(qscriptvalue_cast<quint32>(number6), quint32(-456));

        QScriptValue number7 = QScriptValue(0x43211234);
        QCOMPARE(number7.toUInt32(), quint32(0x43211234));

        QScriptValue str = QScriptValue("123.0");
        QCOMPARE(str.toUInt32(), quint32(123));
        QCOMPARE(qscriptvalue_cast<quint32>(str), quint32(123));

        QScriptValue str2 = QScriptValue("NaN");
        QCOMPARE(str2.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(str2), quint32(0));

        QScriptValue str3 = QScriptValue("Infinity");
        QCOMPARE(str3.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(str3), quint32(0));

        QScriptValue str3_2 = QScriptValue("-Infinity");
        QCOMPARE(str3_2.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(str3_2), quint32(0));

        QScriptValue str4 = QScriptValue("0.5");
        QCOMPARE(str4.toUInt32(), quint32(0));
        QCOMPARE(qscriptvalue_cast<quint32>(str4), quint32(0));

        QScriptValue str5 = QScriptValue("123.5");
        QCOMPARE(str5.toUInt32(), quint32(123));
        QCOMPARE(qscriptvalue_cast<quint32>(str5), quint32(123));

        QScriptValue str6 = QScriptValue("-456.5");
        QCOMPARE(str6.toUInt32(), quint32(-456));
        QCOMPARE(qscriptvalue_cast<quint32>(str6), quint32(-456));
    }

    QScriptValue inv;
    QCOMPARE(inv.toUInt32(), quint32(0));
    QCOMPARE(qscriptvalue_cast<quint32>(inv), quint32(0));
}

void tst_QScriptValue::toUInt16()
{
    QScriptEngine eng;

    {
        QScriptValue zer0 = QScriptValue(&eng, 0.0);
        QCOMPARE(zer0.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(zer0), quint16(0));

        QScriptValue number = QScriptValue(&eng, 123.0);
        QCOMPARE(number.toUInt16(), quint16(123));
        QCOMPARE(qscriptvalue_cast<quint16>(number), quint16(123));

        QScriptValue number2 = QScriptValue(&eng, qSNaN());
        QCOMPARE(number2.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(number2), quint16(0));

        QScriptValue number3 = QScriptValue(&eng, +qInf());
        QCOMPARE(number3.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(number3), quint16(0));

        QScriptValue number3_2 = QScriptValue(&eng, -qInf());
        QCOMPARE(number3_2.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(number3_2), quint16(0));

        QScriptValue number4 = QScriptValue(&eng, 0.5);
        QCOMPARE(number4.toUInt16(), quint16(0));

        QScriptValue number5 = QScriptValue(&eng, 123.5);
        QCOMPARE(number5.toUInt16(), quint16(123));

        QScriptValue number6 = QScriptValue(&eng, -456.5);
        QCOMPARE(number6.toUInt16(), quint16(-456));
        QCOMPARE(qscriptvalue_cast<quint16>(number6), quint16(-456));

        QScriptValue number7 = QScriptValue(&eng, 0x10000);
        QCOMPARE(number7.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(number7), quint16(0));

        QScriptValue number8 = QScriptValue(&eng, 0x10001);
        QCOMPARE(number8.toUInt16(), quint16(1));
        QCOMPARE(qscriptvalue_cast<quint16>(number8), quint16(1));

        QScriptValue str = QScriptValue(&eng, "123.0");
        QCOMPARE(str.toUInt16(), quint16(123));
        QCOMPARE(qscriptvalue_cast<quint16>(str), quint16(123));

        QScriptValue str2 = QScriptValue(&eng, "NaN");
        QCOMPARE(str2.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(str2), quint16(0));

        QScriptValue str3 = QScriptValue(&eng, "Infinity");
        QCOMPARE(str3.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(str3), quint16(0));

        QScriptValue str3_2 = QScriptValue(&eng, "-Infinity");
        QCOMPARE(str3_2.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(str3_2), quint16(0));

        QScriptValue str4 = QScriptValue(&eng, "0.5");
        QCOMPARE(str4.toUInt16(), quint16(0));

        QScriptValue str5 = QScriptValue(&eng, "123.5");
        QCOMPARE(str5.toUInt16(), quint16(123));

        QScriptValue str6 = QScriptValue(&eng, "-456.5");
        QCOMPARE(str6.toUInt16(), quint16(-456));
        QCOMPARE(qscriptvalue_cast<quint16>(str6), quint16(-456));

        QScriptValue str7 = QScriptValue(&eng, "0x10000");
        QCOMPARE(str7.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(str7), quint16(0));

        QScriptValue str8 = QScriptValue(&eng, "0x10001");
        QCOMPARE(str8.toUInt16(), quint16(1));
        QCOMPARE(qscriptvalue_cast<quint16>(str8), quint16(1));
    }
    // V2 constructors
    {
        QScriptValue zer0 = QScriptValue(0.0);
        QCOMPARE(zer0.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(zer0), quint16(0));

        QScriptValue number = QScriptValue(123.0);
        QCOMPARE(number.toUInt16(), quint16(123));
        QCOMPARE(qscriptvalue_cast<quint16>(number), quint16(123));

        QScriptValue number2 = QScriptValue(qSNaN());
        QCOMPARE(number2.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(number2), quint16(0));

        QScriptValue number3 = QScriptValue(+qInf());
        QCOMPARE(number3.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(number3), quint16(0));

        QScriptValue number3_2 = QScriptValue(-qInf());
        QCOMPARE(number3_2.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(number3_2), quint16(0));

        QScriptValue number4 = QScriptValue(0.5);
        QCOMPARE(number4.toUInt16(), quint16(0));

        QScriptValue number5 = QScriptValue(123.5);
        QCOMPARE(number5.toUInt16(), quint16(123));

        QScriptValue number6 = QScriptValue(-456.5);
        QCOMPARE(number6.toUInt16(), quint16(-456));
        QCOMPARE(qscriptvalue_cast<quint16>(number6), quint16(-456));

        QScriptValue number7 = QScriptValue(0x10000);
        QCOMPARE(number7.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(number7), quint16(0));

        QScriptValue number8 = QScriptValue(0x10001);
        QCOMPARE(number8.toUInt16(), quint16(1));
        QCOMPARE(qscriptvalue_cast<quint16>(number8), quint16(1));

        QScriptValue str = QScriptValue("123.0");
        QCOMPARE(str.toUInt16(), quint16(123));
        QCOMPARE(qscriptvalue_cast<quint16>(str), quint16(123));

        QScriptValue str2 = QScriptValue("NaN");
        QCOMPARE(str2.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(str2), quint16(0));

        QScriptValue str3 = QScriptValue("Infinity");
        QCOMPARE(str3.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(str3), quint16(0));

        QScriptValue str3_2 = QScriptValue("-Infinity");
        QCOMPARE(str3_2.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(str3_2), quint16(0));

        QScriptValue str4 = QScriptValue("0.5");
        QCOMPARE(str4.toUInt16(), quint16(0));

        QScriptValue str5 = QScriptValue("123.5");
        QCOMPARE(str5.toUInt16(), quint16(123));

        QScriptValue str6 = QScriptValue("-456.5");
        QCOMPARE(str6.toUInt16(), quint16(-456));
        QCOMPARE(qscriptvalue_cast<quint16>(str6), quint16(-456));

        QScriptValue str7 = QScriptValue("0x10000");
        QCOMPARE(str7.toUInt16(), quint16(0));
        QCOMPARE(qscriptvalue_cast<quint16>(str7), quint16(0));

        QScriptValue str8 = QScriptValue("0x10001");
        QCOMPARE(str8.toUInt16(), quint16(1));
        QCOMPARE(qscriptvalue_cast<quint16>(str8), quint16(1));
    }

    QScriptValue inv;
    QCOMPARE(inv.toUInt16(), quint16(0));
    QCOMPARE(qscriptvalue_cast<quint16>(inv), quint16(0));
}

#if defined Q_CC_MSVC && _MSC_VER < 1300
Q_DECLARE_METATYPE(QVariant)
#endif

void tst_QScriptValue::toVariant()
{
    QScriptEngine eng;

    QScriptValue undefined = eng.undefinedValue();
    QCOMPARE(undefined.toVariant(), QVariant());
    QCOMPARE(qscriptvalue_cast<QVariant>(undefined), QVariant());

    QScriptValue null = eng.nullValue();
    QCOMPARE(null.toVariant(), QVariant());
    QCOMPARE(qscriptvalue_cast<QVariant>(null), QVariant());

    {
        QScriptValue number = QScriptValue(&eng, 123.0);
        QCOMPARE(number.toVariant(), QVariant(123.0));
        QCOMPARE(qscriptvalue_cast<QVariant>(number), QVariant(123.0));

        QScriptValue intNumber = QScriptValue(&eng, (qint32)123);
        QCOMPARE(intNumber.toVariant().type(), QVariant((qint32)123).type());
        QCOMPARE((qscriptvalue_cast<QVariant>(number)).type(), QVariant((qint32)123).type());

        QScriptValue falskt = QScriptValue(&eng, false);
        QCOMPARE(falskt.toVariant(), QVariant(false));
        QCOMPARE(qscriptvalue_cast<QVariant>(falskt), QVariant(false));

        QScriptValue sant = QScriptValue(&eng, true);
        QCOMPARE(sant.toVariant(), QVariant(true));
        QCOMPARE(qscriptvalue_cast<QVariant>(sant), QVariant(true));

        QScriptValue str = QScriptValue(&eng, QString("ciao"));
        QCOMPARE(str.toVariant(), QVariant(QString("ciao")));
        QCOMPARE(qscriptvalue_cast<QVariant>(str), QVariant(QString("ciao")));
    }

    QVariant var(QChar(0x007A));
    QScriptValue opaque = eng.newVariant(var);
    QVERIFY(opaque.isVariant());
    QCOMPARE(opaque.toVariant(), var);

    QScriptValue object = eng.newObject();
    QCOMPARE(object.toVariant(), QVariant(QVariantMap()));

    QScriptValue qobject = eng.newQObject(this);
    {
        QVariant var = qobject.toVariant();
        QCOMPARE(var.userType(), int(QMetaType::QObjectStar));
        QCOMPARE(qVariantValue<QObject*>(var), (QObject *)this);
    }

    {
        QDateTime dateTime = QDateTime(QDate(1980, 10, 4));
        QScriptValue dateObject = eng.newDate(dateTime);
        QVariant var = dateObject.toVariant();
        QCOMPARE(var, QVariant(dateTime));
    }

    {
        QRegExp rx = QRegExp("[0-9a-z]+", Qt::CaseSensitive, QRegExp::RegExp2);
        QScriptValue rxObject = eng.newRegExp(rx);
        QVariant var = rxObject.toVariant();
        QCOMPARE(var, QVariant(rx));
    }

    QScriptValue inv;
    QCOMPARE(inv.toVariant(), QVariant());
    QCOMPARE(qscriptvalue_cast<QVariant>(inv), QVariant());

    // V2 constructors
    {
        QScriptValue number = QScriptValue(123.0);
        QCOMPARE(number.toVariant(), QVariant(123.0));
        QCOMPARE(qscriptvalue_cast<QVariant>(number), QVariant(123.0));

        QScriptValue falskt = QScriptValue(false);
        QCOMPARE(falskt.toVariant(), QVariant(false));
        QCOMPARE(qscriptvalue_cast<QVariant>(falskt), QVariant(false));

        QScriptValue sant = QScriptValue(true);
        QCOMPARE(sant.toVariant(), QVariant(true));
        QCOMPARE(qscriptvalue_cast<QVariant>(sant), QVariant(true));

        QScriptValue str = QScriptValue(QString("ciao"));
        QCOMPARE(str.toVariant(), QVariant(QString("ciao")));
        QCOMPARE(qscriptvalue_cast<QVariant>(str), QVariant(QString("ciao")));
    }

    // array
    {
        QVariantList listIn;
        listIn << 123 << "hello";
        QScriptValue array = qScriptValueFromValue(&eng, listIn);
        QVERIFY(array.isArray());
        QCOMPARE(array.property("length").toInt32(), 2);
        QVariant ret = array.toVariant();
        QCOMPARE(ret.type(), QVariant::List);
        QVariantList listOut = ret.toList();
        QCOMPARE(listOut.size(), listIn.size());
        for (int i = 0; i < listIn.size(); ++i)
            QVERIFY(listOut.at(i) == listIn.at(i));
        // round-trip conversion
        QScriptValue array2 = qScriptValueFromValue(&eng, ret);
        QVERIFY(array2.isArray());
        QCOMPARE(array2.property("length").toInt32(), array.property("length").toInt32());
        for (int i = 0; i < array.property("length").toInt32(); ++i)
            QVERIFY(array2.property(i).strictlyEquals(array.property(i)));
    }
}

void tst_QScriptValue::toQObject_nonQObject_data()
{
    newEngine();
    QTest::addColumn<QScriptValue>("value");

    QTest::newRow("invalid") << QScriptValue();
    QTest::newRow("bool(false)") << QScriptValue(false);
    QTest::newRow("bool(true)") << QScriptValue(true);
    QTest::newRow("int") << QScriptValue(123);
    QTest::newRow("string") << QScriptValue(QString::fromLatin1("ciao"));
    QTest::newRow("undefined") << QScriptValue(QScriptValue::UndefinedValue);
    QTest::newRow("null") << QScriptValue(QScriptValue::NullValue);

    QTest::newRow("bool bound(false)") << QScriptValue(engine, false);
    QTest::newRow("bool bound(true)") << QScriptValue(engine, true);
    QTest::newRow("int bound") << QScriptValue(engine, 123);
    QTest::newRow("string bound") << QScriptValue(engine, QString::fromLatin1("ciao"));
    QTest::newRow("undefined bound") << engine->undefinedValue();
    QTest::newRow("null bound") << engine->nullValue();
    QTest::newRow("object") << engine->newObject();
    QTest::newRow("array") << engine->newArray();
    QTest::newRow("date") << engine->newDate(124);
    QTest::newRow("variant(12345)") << engine->newVariant(12345);
    QTest::newRow("variant((QObject*)0)") << engine->newVariant(qVariantFromValue((QObject*)0));
    QTest::newRow("newQObject(0)") << engine->newQObject(0);
}


void tst_QScriptValue::toQObject_nonQObject()
{
    QFETCH(QScriptValue, value);
    QCOMPARE(value.toQObject(), (QObject *)0);
    QCOMPARE(qscriptvalue_cast<QObject*>(value), (QObject *)0);
}

// unfortunately, this is necessary in order to do qscriptvalue_cast<QPushButton*>(...)
Q_DECLARE_METATYPE(QPushButton*);

void tst_QScriptValue::toQObject()
{
    QScriptEngine eng;

    QScriptValue qobject = eng.newQObject(this);
    QCOMPARE(qobject.toQObject(), (QObject *)this);
    QCOMPARE(qscriptvalue_cast<QObject*>(qobject), (QObject *)this);
    QCOMPARE(qscriptvalue_cast<QWidget*>(qobject), (QWidget *)0);

    QWidget widget;
    QScriptValue qwidget = eng.newQObject(&widget);
    QCOMPARE(qwidget.toQObject(), (QObject *)&widget);
    QCOMPARE(qscriptvalue_cast<QObject*>(qwidget), (QObject *)&widget);
    QCOMPARE(qscriptvalue_cast<QWidget*>(qwidget), &widget);

    QPushButton button;
    QScriptValue qbutton = eng.newQObject(&button);
    QCOMPARE(qbutton.toQObject(), (QObject *)&button);
    QCOMPARE(qscriptvalue_cast<QObject*>(qbutton), (QObject *)&button);
    QCOMPARE(qscriptvalue_cast<QWidget*>(qbutton), (QWidget *)&button);
    QCOMPARE(qscriptvalue_cast<QPushButton*>(qbutton), &button);

    // wrapping a QObject* as variant
    QScriptValue variant = eng.newVariant(qVariantFromValue((QObject*)&button));
    QCOMPARE(variant.toQObject(), (QObject*)&button);
    QCOMPARE(qscriptvalue_cast<QObject*>(variant), (QObject*)&button);
    QCOMPARE(qscriptvalue_cast<QWidget*>(variant), (QWidget*)&button);
    QCOMPARE(qscriptvalue_cast<QPushButton*>(variant), &button);

    QScriptValue variant2 = eng.newVariant(qVariantFromValue((QWidget*)&button));
    QCOMPARE(variant2.toQObject(), (QObject*)&button);
    QCOMPARE(qscriptvalue_cast<QObject*>(variant2), (QObject*)&button);
    QCOMPARE(qscriptvalue_cast<QWidget*>(variant2), (QWidget*)&button);
    QCOMPARE(qscriptvalue_cast<QPushButton*>(variant2), &button);

    QScriptValue variant3 = eng.newVariant(qVariantFromValue(&button));
    QVERIFY(variant3.isQObject());
    QCOMPARE(variant3.toQObject(), (QObject*)&button);
    QCOMPARE(qscriptvalue_cast<QObject*>(variant3), (QObject*)&button);
    QCOMPARE(qscriptvalue_cast<QWidget*>(variant3), (QWidget*)&button);
    QCOMPARE(qscriptvalue_cast<QPushButton*>(variant3), &button);
}

void tst_QScriptValue::toObject()
{
    QScriptEngine eng;

    QScriptValue undefined = eng.undefinedValue();
    QCOMPARE(undefined.toObject().isValid(), false);
    QVERIFY(undefined.isUndefined());

    QScriptValue null = eng.nullValue();
    QCOMPARE(null.toObject().isValid(), false);
    QVERIFY(null.isNull());

    {
        QScriptValue falskt = QScriptValue(&eng, false);
        {
            QScriptValue tmp = falskt.toObject();
            QCOMPARE(tmp.isObject(), true);
            QCOMPARE(tmp.toNumber(), falskt.toNumber());
        }
        QVERIFY(falskt.isBool());

        QScriptValue sant = QScriptValue(&eng, true);
        {
            QScriptValue tmp = sant.toObject();
            QCOMPARE(tmp.isObject(), true);
            QCOMPARE(tmp.toNumber(), sant.toNumber());
        }
        QVERIFY(sant.isBool());

        QScriptValue number = QScriptValue(&eng, 123.0);
        {
            QScriptValue tmp = number.toObject();
            QCOMPARE(tmp.isObject(), true);
            QCOMPARE(tmp.toNumber(), number.toNumber());
        }
        QVERIFY(number.isNumber());

        QScriptValue str = QScriptValue(&eng, QString("ciao"));
        {
            QScriptValue tmp = str.toObject();
            QCOMPARE(tmp.isObject(), true);
            QCOMPARE(tmp.toString(), str.toString());
        }
        QVERIFY(str.isString());
    }

    QScriptValue object = eng.newObject();
    {
        QScriptValue tmp = object.toObject();
        QCOMPARE(tmp.isObject(), true);
    }

    QScriptValue qobject = eng.newQObject(this);
    QCOMPARE(qobject.toObject().isValid(), true);

    QScriptValue inv;
    QCOMPARE(inv.toObject().isValid(), false);

    // V2 constructors: in this case, you have to use QScriptEngine::toObject()
    {
        QScriptValue undefined = QScriptValue(QScriptValue::UndefinedValue);
        QVERIFY(!undefined.toObject().isValid());
        QVERIFY(!eng.toObject(undefined).isValid());
        QVERIFY(undefined.isUndefined());

        QScriptValue null = QScriptValue(QScriptValue::NullValue);
        QVERIFY(!null.toObject().isValid());
        QVERIFY(!eng.toObject(null).isValid());
        QVERIFY(null.isNull());

        QScriptValue falskt = QScriptValue(false);
        QVERIFY(!falskt.toObject().isValid());
        {
            QScriptValue tmp = eng.toObject(falskt);
            QVERIFY(tmp.isObject());
            QVERIFY(tmp.toBool());
        }
        QVERIFY(falskt.isBool());

        QScriptValue sant = QScriptValue(true);
        QVERIFY(!sant.toObject().isValid());
        {
            QScriptValue tmp = eng.toObject(sant);
            QVERIFY(tmp.isObject());
            QVERIFY(tmp.toBool());
        }
        QVERIFY(sant.isBool());

        QScriptValue number = QScriptValue(123.0);
        QVERIFY(!number.toObject().isValid());
        {
            QScriptValue tmp = eng.toObject(number);
            QVERIFY(tmp.isObject());
            QCOMPARE(tmp.toInt32(), number.toInt32());
        }
        QVERIFY(number.isNumber());

        QScriptValue str = QScriptValue(QString::fromLatin1("ciao"));
        QVERIFY(!str.toObject().isValid());
        {
            QScriptValue tmp = eng.toObject(str);
            QVERIFY(tmp.isObject());
            QCOMPARE(tmp.toString(), QString::fromLatin1("ciao"));
        }
        QVERIFY(str.isString());
    }
}

void tst_QScriptValue::toDateTime()
{
    QScriptEngine eng;
    QDateTime dt = eng.evaluate("new Date(0)").toDateTime();
    QVERIFY(dt.isValid());
    QCOMPARE(dt.timeSpec(), Qt::LocalTime);
    QCOMPARE(dt.toUTC(), QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0), Qt::UTC));

    QVERIFY(!eng.evaluate("[]").toDateTime().isValid());
    QVERIFY(!eng.evaluate("{}").toDateTime().isValid());
    QVERIFY(!eng.globalObject().toDateTime().isValid());
    QVERIFY(!QScriptValue().toDateTime().isValid());
    QVERIFY(!QScriptValue(123).toDateTime().isValid());
    QVERIFY(!QScriptValue(false).toDateTime().isValid());
    QVERIFY(!eng.nullValue().toDateTime().isValid());
    QVERIFY(!eng.undefinedValue().toDateTime().isValid());
}

void tst_QScriptValue::toRegExp()
{
    QScriptEngine eng;
    {
        QRegExp rx = eng.evaluate("/foo/").toRegExp();
        QVERIFY(rx.isValid());
        QCOMPARE(rx.patternSyntax(), QRegExp::RegExp2);
        QCOMPARE(rx.pattern(), QString::fromLatin1("foo"));
        QCOMPARE(rx.caseSensitivity(), Qt::CaseSensitive);
        QVERIFY(!rx.isMinimal());
    }
    {
        QRegExp rx = eng.evaluate("/bar/gi").toRegExp();
        QVERIFY(rx.isValid());
        QCOMPARE(rx.patternSyntax(), QRegExp::RegExp2);
        QCOMPARE(rx.pattern(), QString::fromLatin1("bar"));
        QCOMPARE(rx.caseSensitivity(), Qt::CaseInsensitive);
        QVERIFY(!rx.isMinimal());
    }

    QVERIFY(eng.evaluate("[]").toRegExp().isEmpty());
    QVERIFY(eng.evaluate("{}").toRegExp().isEmpty());
    QVERIFY(eng.globalObject().toRegExp().isEmpty());
    QVERIFY(QScriptValue().toRegExp().isEmpty());
    QVERIFY(QScriptValue(123).toRegExp().isEmpty());
    QVERIFY(QScriptValue(false).toRegExp().isEmpty());
    QVERIFY(eng.nullValue().toRegExp().isEmpty());
    QVERIFY(eng.undefinedValue().toRegExp().isEmpty());
}

void tst_QScriptValue::instanceOf_twoEngines()
{
    QScriptEngine eng;
    QScriptValue obj = eng.newObject();
    QScriptEngine otherEngine;
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::instanceof: cannot perform operation on a value created in a different engine");
    QCOMPARE(obj.instanceOf(otherEngine.globalObject().property("Object")), false);
}

void tst_QScriptValue::instanceOf()
{
    QScriptEngine eng;
    QScriptValue obj = eng.newObject();
    QCOMPARE(obj.instanceOf(eng.evaluate("Object.prototype")), false);
    QCOMPARE(obj.instanceOf(eng.evaluate("Array.prototype")), false);
    QCOMPARE(obj.instanceOf(eng.evaluate("Function.prototype")), false);
    QCOMPARE(obj.instanceOf(eng.evaluate("QObject.prototype")), false);
    QCOMPARE(obj.instanceOf(QScriptValue(&eng, 123)), false);
    QCOMPARE(obj.instanceOf(eng.undefinedValue()), false);
    QCOMPARE(obj.instanceOf(eng.nullValue()), false);
    QCOMPARE(obj.instanceOf(QScriptValue()), false);

    QCOMPARE(obj.instanceOf(eng.evaluate("Object")), true);
    QCOMPARE(obj.instanceOf(eng.evaluate("Array")), false);
    QCOMPARE(obj.instanceOf(eng.evaluate("Function")), false);
    QCOMPARE(obj.instanceOf(eng.evaluate("QObject")), false);

    QScriptValue arr = eng.newArray();
    QVERIFY(arr.isArray());
    QCOMPARE(arr.instanceOf(eng.evaluate("Object.prototype")), false);
    QCOMPARE(arr.instanceOf(eng.evaluate("Array.prototype")), false);
    QCOMPARE(arr.instanceOf(eng.evaluate("Function.prototype")), false);
    QCOMPARE(arr.instanceOf(eng.evaluate("QObject.prototype")), false);
    QCOMPARE(arr.instanceOf(eng.evaluate("Object")), true);
    QCOMPARE(arr.instanceOf(eng.evaluate("Array")), true);
    QCOMPARE(arr.instanceOf(eng.evaluate("Function")), false);
    QCOMPARE(arr.instanceOf(eng.evaluate("QObject")), false);

    QCOMPARE(QScriptValue().instanceOf(arr), false);
}

void tst_QScriptValue::isArray_data()
{
    newEngine();

    QTest::addColumn<QScriptValue>("value");
    QTest::addColumn<bool>("array");

    QTest::newRow("[]") << engine->evaluate("[]") << true;
    QTest::newRow("{}") << engine->evaluate("{}") << false;
    QTest::newRow("globalObject") << engine->globalObject() << false;
    QTest::newRow("invalid") << QScriptValue() << false;
    QTest::newRow("number") << QScriptValue(123) << false;
    QTest::newRow("bool") << QScriptValue(false) << false;
    QTest::newRow("null") << engine->nullValue() << false;
    QTest::newRow("undefined") << engine->undefinedValue() << false;
}

void tst_QScriptValue::isArray()
{
    QFETCH(QScriptValue, value);
    QFETCH(bool, array);

    QCOMPARE(value.isArray(), array);
}

void tst_QScriptValue::isDate_data()
{
    newEngine();

    QTest::addColumn<QScriptValue>("value");
    QTest::addColumn<bool>("date");

    QTest::newRow("date") << engine->evaluate("new Date()") << true;
    QTest::newRow("[]") << engine->evaluate("[]") << false;
    QTest::newRow("{}") << engine->evaluate("{}") << false;
    QTest::newRow("globalObject") << engine->globalObject() << false;
    QTest::newRow("invalid") << QScriptValue() << false;
    QTest::newRow("number") << QScriptValue(123) << false;
    QTest::newRow("bool") << QScriptValue(false) << false;
    QTest::newRow("null") << engine->nullValue() << false;
    QTest::newRow("undefined") << engine->undefinedValue() << false;
}

void tst_QScriptValue::isDate()
{
    QFETCH(QScriptValue, value);
    QFETCH(bool, date);

    QCOMPARE(value.isDate(), date);
}

void tst_QScriptValue::isError_propertiesOfGlobalObject()
{
    QStringList errors;
    errors << "Error"
           << "EvalError"
           << "RangeError"
           << "ReferenceError"
           << "SyntaxError"
           << "TypeError"
           << "URIError";
    QScriptEngine eng;
    for (int i = 0; i < errors.size(); ++i) {
        QScriptValue ctor = eng.globalObject().property(errors.at(i));
        QVERIFY(ctor.isFunction());
        QVERIFY(ctor.property("prototype").isError());
    }
}

void tst_QScriptValue::isError_data()
{
    newEngine();

    QTest::addColumn<QScriptValue>("value");
    QTest::addColumn<bool>("error");

    QTest::newRow("syntax error") << engine->evaluate("%fsdg's") << true;
    QTest::newRow("[]") << engine->evaluate("[]") << false;
    QTest::newRow("{}") << engine->evaluate("{}") << false;
    QTest::newRow("globalObject") << engine->globalObject() << false;
    QTest::newRow("invalid") << QScriptValue() << false;
    QTest::newRow("number") << QScriptValue(123) << false;
    QTest::newRow("bool") << QScriptValue(false) << false;
    QTest::newRow("null") << engine->nullValue() << false;
    QTest::newRow("undefined") << engine->undefinedValue() << false;
    QTest::newRow("newObject") << engine->newObject() << false;
    QTest::newRow("new Object") << engine->evaluate("new Object()") << false;
}

void tst_QScriptValue::isError()
{
    QFETCH(QScriptValue, value);
    QFETCH(bool, error);

    QCOMPARE(value.isError(), error);
}

void tst_QScriptValue::isRegExp_data()
{
    newEngine();

    QTest::addColumn<QScriptValue>("value");
    QTest::addColumn<bool>("regexp");

    QTest::newRow("/foo/") << engine->evaluate("/foo/") << true;
    QTest::newRow("[]") << engine->evaluate("[]") << false;
    QTest::newRow("{}") << engine->evaluate("{}") << false;
    QTest::newRow("globalObject") << engine->globalObject() << false;
    QTest::newRow("invalid") << QScriptValue() << false;
    QTest::newRow("number") << QScriptValue(123) << false;
    QTest::newRow("bool") << QScriptValue(false) << false;
    QTest::newRow("null") << engine->nullValue() << false;
    QTest::newRow("undefined") << engine->undefinedValue() << false;
}

void tst_QScriptValue::isRegExp()
{
    QFETCH(QScriptValue, value);
    QFETCH(bool, regexp);

    QCOMPARE(value.isRegExp(), regexp);
}

static QScriptValue getter(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->thisObject().property("x");
}

static QScriptValue setter(QScriptContext *ctx, QScriptEngine *)
{
    ctx->thisObject().setProperty("x", ctx->argument(0));
    return ctx->argument(0);
}

static QScriptValue getterSetter(QScriptContext *ctx, QScriptEngine *)
{
    if (ctx->argumentCount() > 0)
        ctx->thisObject().setProperty("x", ctx->argument(0));
    return ctx->thisObject().property("x");
}

static QScriptValue getterSetterThrowingError(QScriptContext *ctx, QScriptEngine *)
{
    if (ctx->argumentCount() > 0)
        return ctx->throwError("set foo");
    else
        return ctx->throwError("get foo");
}

static QScriptValue getSet__proto__(QScriptContext *ctx, QScriptEngine *)
{
    if (ctx->argumentCount() > 0)
        ctx->callee().setProperty("value", ctx->argument(0));
    return ctx->callee().property("value");
}

void tst_QScriptValue::getSetProperty_HooliganTask162051()
{
    QScriptEngine eng;
    // task 162051 -- detecting whether the property is an array index or not
    QVERIFY(eng.evaluate("a = []; a['00'] = 123; a['00']").strictlyEquals(QScriptValue(&eng, 123)));
    QVERIFY(eng.evaluate("a.length").strictlyEquals(QScriptValue(&eng, 0)));
    QVERIFY(eng.evaluate("a.hasOwnProperty('00')").strictlyEquals(QScriptValue(&eng, true)));
    QVERIFY(eng.evaluate("a.hasOwnProperty('0')").strictlyEquals(QScriptValue(&eng, false)));
    QVERIFY(eng.evaluate("a[0]").isUndefined());
    QVERIFY(eng.evaluate("a[0.5] = 456; a[0.5]").strictlyEquals(QScriptValue(&eng, 456)));
    QVERIFY(eng.evaluate("a.length").strictlyEquals(QScriptValue(&eng, 0)));
    QVERIFY(eng.evaluate("a.hasOwnProperty('0.5')").strictlyEquals(QScriptValue(&eng, true)));
    QVERIFY(eng.evaluate("a[0]").isUndefined());
    QVERIFY(eng.evaluate("a[0] = 789; a[0]").strictlyEquals(QScriptValue(&eng, 789)));
    QVERIFY(eng.evaluate("a.length").strictlyEquals(QScriptValue(&eng, 1)));
}

void tst_QScriptValue::getSetProperty_HooliganTask183072()
{
    QScriptEngine eng;
    // task 183072 -- 0x800000000 is not an array index
    eng.evaluate("a = []; a[0x800000000] = 123");
    QVERIFY(eng.evaluate("a.length").strictlyEquals(QScriptValue(&eng, 0)));
    QVERIFY(eng.evaluate("a[0]").isUndefined());
    QVERIFY(eng.evaluate("a[0x800000000]").strictlyEquals(QScriptValue(&eng, 123)));
}

void tst_QScriptValue::getSetProperty_propertyRemoval()
{
    // test property removal (setProperty(QScriptValue()))
    QScriptEngine eng;
    QScriptValue object = eng.newObject();
    QScriptValue str = QScriptValue(&eng, "bar");
    QScriptValue num = QScriptValue(&eng, 123.0);

    object.setProperty("foo", num);
    QCOMPARE(object.property("foo").strictlyEquals(num), true);
    object.setProperty("bar", str);
    QCOMPARE(object.property("bar").strictlyEquals(str), true);
    object.setProperty("foo", QScriptValue());
    QCOMPARE(object.property("foo").isValid(), false);
    QCOMPARE(object.property("bar").strictlyEquals(str), true);
    object.setProperty("foo", num);
    QCOMPARE(object.property("foo").strictlyEquals(num), true);
    QCOMPARE(object.property("bar").strictlyEquals(str), true);
    object.setProperty("bar", QScriptValue());
    QCOMPARE(object.property("bar").isValid(), false);
    QCOMPARE(object.property("foo").strictlyEquals(num), true);
    object.setProperty("foo", QScriptValue());
    object.setProperty("foo", QScriptValue());

    eng.globalObject().setProperty("object3", object);
    QCOMPARE(eng.evaluate("object3.hasOwnProperty('foo')")
             .strictlyEquals(QScriptValue(&eng, false)), true);
    object.setProperty("foo", num);
    QCOMPARE(eng.evaluate("object3.hasOwnProperty('foo')")
             .strictlyEquals(QScriptValue(&eng, true)), true);
    eng.globalObject().setProperty("object3", QScriptValue());
    QCOMPARE(eng.evaluate("this.hasOwnProperty('object3')")
             .strictlyEquals(QScriptValue(&eng, false)), true);
}

void tst_QScriptValue::getSetProperty_resolveMode()
{
    // test ResolveMode
    QScriptEngine eng;
    QScriptValue object = eng.newObject();
    QScriptValue prototype = eng.newObject();
    object.setPrototype(prototype);
    QScriptValue num2 = QScriptValue(&eng, 456.0);
    prototype.setProperty("propertyInPrototype", num2);
    // default is ResolvePrototype
    QCOMPARE(object.property("propertyInPrototype")
             .strictlyEquals(num2), true);
    QCOMPARE(object.property("propertyInPrototype", QScriptValue::ResolvePrototype)
             .strictlyEquals(num2), true);
    QCOMPARE(object.property("propertyInPrototype", QScriptValue::ResolveLocal)
             .isValid(), false);
    QCOMPARE(object.property("propertyInPrototype", QScriptValue::ResolveScope)
             .strictlyEquals(num2), false);
    QCOMPARE(object.property("propertyInPrototype", QScriptValue::ResolveFull)
             .strictlyEquals(num2), true);
}

void tst_QScriptValue::getSetProperty_twoEngines()
{
    QScriptEngine engine;
    QScriptValue object = engine.newObject();

    QScriptEngine otherEngine;
    QScriptValue otherNum = QScriptValue(&otherEngine, 123);
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setProperty(oof) failed: cannot set value created in a different engine");
    object.setProperty("oof", otherNum);
    QCOMPARE(object.property("oof").isValid(), false);
}


void tst_QScriptValue::getSetProperty_gettersAndSetters()
{
    QScriptEngine eng;
    QScriptValue str = QScriptValue(&eng, "bar");
    QScriptValue num = QScriptValue(&eng, 123.0);
    QScriptValue object = eng.newObject();
    for (int x = 0; x < 2; ++x) {
        object.setProperty("foo", QScriptValue());
        // getter() returns this.x
        object.setProperty("foo", eng.newFunction(getter),
                            QScriptValue::PropertyGetter | QScriptValue::UserRange);
        QCOMPARE(object.propertyFlags("foo") & ~QScriptValue::UserRange,
                 QScriptValue::PropertyGetter );

        QEXPECT_FAIL("", "QTBUG-17615: User-range flags are not retained for getter/setter properties", Continue);
        QCOMPARE(object.propertyFlags("foo"),
                 QScriptValue::PropertyGetter | QScriptValue::UserRange);
        object.setProperty("x", num);
        QCOMPARE(object.property("foo").strictlyEquals(num), true);

        // setter() sets this.x
        object.setProperty("foo", eng.newFunction(setter),
                            QScriptValue::PropertySetter);
        QCOMPARE(object.propertyFlags("foo") & ~QScriptValue::UserRange,
                 QScriptValue::PropertySetter | QScriptValue::PropertyGetter);

        QCOMPARE(object.propertyFlags("foo"),
                 QScriptValue::PropertySetter | QScriptValue::PropertyGetter);
        object.setProperty("foo", str);
        QCOMPARE(object.property("x").strictlyEquals(str), true);
        QCOMPARE(object.property("foo").strictlyEquals(str), true);

        // kill the getter
        object.setProperty("foo", QScriptValue(), QScriptValue::PropertyGetter);
        QVERIFY(!(object.propertyFlags("foo") & QScriptValue::PropertyGetter));
        QVERIFY(object.propertyFlags("foo") & QScriptValue::PropertySetter);
        QCOMPARE(object.property("foo").isUndefined(), true);

        // setter should still work
        object.setProperty("foo", num);
        QCOMPARE(object.property("x").strictlyEquals(num), true);

        // kill the setter too
        object.setProperty("foo", QScriptValue(), QScriptValue::PropertySetter);
        QVERIFY(!(object.propertyFlags("foo") & QScriptValue::PropertySetter));
        // now foo is just a regular property
        object.setProperty("foo", str);
        QCOMPARE(object.property("x").strictlyEquals(num), true);
        QCOMPARE(object.property("foo").strictlyEquals(str), true);
    }

    for (int x = 0; x < 2; ++x) {
        object.setProperty("foo", QScriptValue());
        // setter() sets this.x
        object.setProperty("foo", eng.newFunction(setter), QScriptValue::PropertySetter);
        object.setProperty("foo", str);
        QCOMPARE(object.property("x").strictlyEquals(str), true);
        QCOMPARE(object.property("foo").isUndefined(), true);

        // getter() returns this.x
        object.setProperty("foo", eng.newFunction(getter), QScriptValue::PropertyGetter);
        object.setProperty("x", num);
        QCOMPARE(object.property("foo").strictlyEquals(num), true);

        // kill the setter
        object.setProperty("foo", QScriptValue(), QScriptValue::PropertySetter);
        QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setProperty() failed: property 'foo' has a getter but no setter");
        object.setProperty("foo", str);

        // getter should still work
        QCOMPARE(object.property("foo").strictlyEquals(num), true);

        // kill the getter too
        object.setProperty("foo", QScriptValue(), QScriptValue::PropertyGetter);
        // now foo is just a regular property
        object.setProperty("foo", str);
        QCOMPARE(object.property("x").strictlyEquals(num), true);
        QCOMPARE(object.property("foo").strictlyEquals(str), true);
    }

    // use a single function as both getter and setter
    object.setProperty("foo", QScriptValue());
    object.setProperty("foo", eng.newFunction(getterSetter),
                        QScriptValue::PropertyGetter | QScriptValue::PropertySetter);
    QCOMPARE(object.propertyFlags("foo"),
             QScriptValue::PropertyGetter | QScriptValue::PropertySetter);
    object.setProperty("x", num);
    QCOMPARE(object.property("foo").strictlyEquals(num), true);

    // killing the getter will preserve the setter, even though they are the same function
    object.setProperty("foo", QScriptValue(), QScriptValue::PropertyGetter);
    QVERIFY(object.propertyFlags("foo") & QScriptValue::PropertySetter);
    QCOMPARE(object.property("foo").isUndefined(), true);
}

void tst_QScriptValue::getSetProperty_gettersAndSettersThrowErrorNative()
{
    // getter/setter that throws an error
    QScriptEngine eng;
    QScriptValue str = QScriptValue(&eng, "bar");
    QScriptValue object = eng.newObject();

    object.setProperty("foo", eng.newFunction(getterSetterThrowingError),
                        QScriptValue::PropertyGetter | QScriptValue::PropertySetter);
    QVERIFY(!eng.hasUncaughtException());
    QScriptValue ret = object.property("foo");
    QVERIFY(ret.isError());
    QVERIFY(eng.hasUncaughtException());
    QVERIFY(ret.strictlyEquals(eng.uncaughtException()));
    QCOMPARE(ret.toString(), QLatin1String("Error: get foo"));
    eng.evaluate("Object"); // clear exception state...
    QVERIFY(!eng.hasUncaughtException());
    object.setProperty("foo", str);
    QVERIFY(eng.hasUncaughtException());
    QCOMPARE(eng.uncaughtException().toString(), QLatin1String("Error: set foo"));
}

void tst_QScriptValue::getSetProperty_gettersAndSettersThrowErrorJS()
{
    // getter/setter that throws an error (from js function)
    QScriptEngine eng;
    QScriptValue str = QScriptValue(&eng, "bar");

    eng.evaluate("o = new Object; "
                 "o.__defineGetter__('foo', function() { throw new Error('get foo') }); "
                 "o.__defineSetter__('foo', function() { throw new Error('set foo') }); ");
    QScriptValue object = eng.evaluate("o");
    QVERIFY(!eng.hasUncaughtException());
    QScriptValue ret = object.property("foo");
    QEXPECT_FAIL("", "QTBUG-17616: Exception thrown from js function are not returned by the JSC port", Continue);
    QVERIFY(ret.isError());
    QVERIFY(eng.hasUncaughtException());
    QEXPECT_FAIL("", "QTBUG-17616: Exception thrown from js function are not returned by the JSC port", Continue);
    QVERIFY(ret.strictlyEquals(eng.uncaughtException()));
    QCOMPARE(eng.uncaughtException().toString(), QLatin1String("Error: get foo"));
    eng.evaluate("Object"); // clear exception state...
    QVERIFY(!eng.hasUncaughtException());
    object.setProperty("foo", str);
    QVERIFY(eng.hasUncaughtException());
    QCOMPARE(eng.uncaughtException().toString(), QLatin1String("Error: set foo"));
}

void tst_QScriptValue::getSetProperty_gettersAndSettersOnNative()
{
    // attempt to install getter+setter on built-in (native) property
    QScriptEngine eng;
    QScriptValue object = eng.newObject();
    QVERIFY(object.property("__proto__").strictlyEquals(object.prototype()));

    QScriptValue fun = eng.newFunction(getSet__proto__);
    fun.setProperty("value", QScriptValue(&eng, "boo"));
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setProperty() failed: "
                         "cannot set getter or setter of native property "
                         "`__proto__'");
    object.setProperty("__proto__", fun,
                        QScriptValue::PropertyGetter | QScriptValue::PropertySetter
                        | QScriptValue::UserRange);
    QVERIFY(object.property("__proto__").strictlyEquals(object.prototype()));

    object.setProperty("__proto__", QScriptValue(),
                        QScriptValue::PropertyGetter | QScriptValue::PropertySetter);
    QVERIFY(object.property("__proto__").strictlyEquals(object.prototype()));
}

void tst_QScriptValue::getSetProperty_gettersAndSettersOnGlobalObject()
{
    // global property that's a getter+setter
    QScriptEngine eng;
    eng.globalObject().setProperty("globalGetterSetterProperty", eng.newFunction(getterSetter),
                                   QScriptValue::PropertyGetter | QScriptValue::PropertySetter);
    eng.evaluate("globalGetterSetterProperty = 123");
    {
        QScriptValue ret = eng.evaluate("globalGetterSetterProperty");
        QVERIFY(ret.isNumber());
        QVERIFY(ret.strictlyEquals(QScriptValue(&eng, 123)));
    }
    QCOMPARE(eng.evaluate("typeof globalGetterSetterProperty").toString(),
             QString::fromLatin1("number"));
    {
        QScriptValue ret = eng.evaluate("this.globalGetterSetterProperty()");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QString::fromLatin1("TypeError: Result of expression 'this.globalGetterSetterProperty' [123] is not a function."));
    }
    {
        QScriptValue ret = eng.evaluate("new this.globalGetterSetterProperty()");
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QString::fromLatin1("TypeError: Result of expression 'this.globalGetterSetterProperty' [123] is not a constructor."));
    }
}

void tst_QScriptValue::getSetProperty_gettersAndSettersChange()
{
    // "upgrading" an existing property to become a getter+setter
    QScriptEngine eng;
    QScriptValue object = eng.newObject();
    QScriptValue num(&eng, 123);
    object.setProperty("foo", num);
    object.setProperty("foo", eng.newFunction(getterSetter),
                        QScriptValue::PropertyGetter | QScriptValue::PropertySetter);
    QVERIFY(!object.property("x").isValid());
    object.setProperty("foo", num);
    QVERIFY(object.property("x").equals(num));

    eng.globalObject().setProperty("object", object);
    QScriptValue res = eng.evaluate("object.x = 89; var a = object.foo; object.foo = 65; a");
    QCOMPARE(res.toInt32(), 89);
    QCOMPARE(object.property("x").toInt32(), 65);
    QCOMPARE(object.property("foo").toInt32(), 65);
}

void tst_QScriptValue::getSetProperty_array()
{
    QScriptEngine eng;
    QScriptValue str = QScriptValue(&eng, "bar");
    QScriptValue num = QScriptValue(&eng, 123.0);
    QScriptValue array = eng.newArray();

    QVERIFY(array.isArray());
    array.setProperty(0, num);
    QCOMPARE(array.property(0).toNumber(), num.toNumber());
    QCOMPARE(array.property("0").toNumber(), num.toNumber());
    QCOMPARE(array.property("length").toUInt32(), quint32(1));
    array.setProperty(1, str);
    QCOMPARE(array.property(1).toString(), str.toString());
    QCOMPARE(array.property("1").toString(), str.toString());
    QCOMPARE(array.property("length").toUInt32(), quint32(2));
    array.setProperty("length", QScriptValue(&eng, 1));
    QCOMPARE(array.property("length").toUInt32(), quint32(1));
    QCOMPARE(array.property(1).isValid(), false);
}

void tst_QScriptValue::getSetProperty()
{
    QScriptEngine eng;

    QScriptValue object = eng.newObject();

    QScriptValue str = QScriptValue(&eng, "bar");
    object.setProperty("foo", str);
    QCOMPARE(object.property("foo").toString(), str.toString());

    QScriptValue num = QScriptValue(&eng, 123.0);
    object.setProperty("baz", num);
    QCOMPARE(object.property("baz").toNumber(), num.toNumber());

    QScriptValue strstr = QScriptValue("bar");
    QCOMPARE(strstr.engine(), (QScriptEngine *)0);
    object.setProperty("foo", strstr);
    QCOMPARE(object.property("foo").toString(), strstr.toString());
    QCOMPARE(strstr.engine(), &eng); // the value has been bound to the engine

    QScriptValue numnum = QScriptValue(123.0);
    object.setProperty("baz", numnum);
    QCOMPARE(object.property("baz").toNumber(), numnum.toNumber());

    QScriptValue inv;
    inv.setProperty("foo", num);
    QCOMPARE(inv.property("foo").isValid(), false);

    eng.globalObject().setProperty("object", object);

  // ReadOnly
    object.setProperty("readOnlyProperty", num, QScriptValue::ReadOnly);
    QCOMPARE(object.propertyFlags("readOnlyProperty"), QScriptValue::ReadOnly);
    QCOMPARE(object.property("readOnlyProperty").strictlyEquals(num), true);
    eng.evaluate("object.readOnlyProperty = !object.readOnlyProperty");
    QCOMPARE(object.property("readOnlyProperty").strictlyEquals(num), true);
    // should still be part of enumeration
    {
        QScriptValue ret = eng.evaluate(
            "found = false;"
            "for (var p in object) {"
            "  if (p == 'readOnlyProperty') {"
            "    found = true; break;"
            "  }"
            "} found");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, true)), true);
    }
    // should still be deletable
    {
        QScriptValue ret = eng.evaluate("delete object.readOnlyProperty");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, true)), true);
        QCOMPARE(object.property("readOnlyProperty").isValid(), false);
    }

  // Undeletable
    object.setProperty("undeletableProperty", num, QScriptValue::Undeletable);
    QCOMPARE(object.propertyFlags("undeletableProperty"), QScriptValue::Undeletable);
    QCOMPARE(object.property("undeletableProperty").strictlyEquals(num), true);
    {
        QScriptValue ret = eng.evaluate("delete object.undeletableProperty");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, true)), false);
        QCOMPARE(object.property("undeletableProperty").strictlyEquals(num), true);
    }
    // should still be writable
    eng.evaluate("object.undeletableProperty = object.undeletableProperty + 1");
    QCOMPARE(object.property("undeletableProperty").toNumber(), num.toNumber() + 1);
    // should still be part of enumeration
    {
        QScriptValue ret = eng.evaluate(
            "found = false;"
            "for (var p in object) {"
            "  if (p == 'undeletableProperty') {"
            "    found = true; break;"
            "  }"
            "} found");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, true)), true);
    }
    // should still be deletable from C++
    object.setProperty("undeletableProperty", QScriptValue());
    QEXPECT_FAIL("", "QTBUG-17617: With JSC-based back-end, undeletable properties can't be deleted from C++", Continue);
    QVERIFY(!object.property("undeletableProperty").isValid());
    QEXPECT_FAIL("", "QTBUG-17617: With JSC-based back-end, undeletable properties can't be deleted from C++", Continue);
    QCOMPARE(object.propertyFlags("undeletableProperty"), 0);

  // SkipInEnumeration
    object.setProperty("dontEnumProperty", num, QScriptValue::SkipInEnumeration);
    QCOMPARE(object.propertyFlags("dontEnumProperty"), QScriptValue::SkipInEnumeration);
    QCOMPARE(object.property("dontEnumProperty").strictlyEquals(num), true);
    // should not be part of enumeration
    {
        QScriptValue ret = eng.evaluate(
            "found = false;"
            "for (var p in object) {"
            "  if (p == 'dontEnumProperty') {"
            "    found = true; break;"
            "  }"
            "} found");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, false)), true);
    }
    // should still be writable
    eng.evaluate("object.dontEnumProperty = object.dontEnumProperty + 1");
    QCOMPARE(object.property("dontEnumProperty").toNumber(), num.toNumber() + 1);
    // should still be deletable
    {
        QScriptValue ret = eng.evaluate("delete object.dontEnumProperty");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&eng, true)), true);
        QCOMPARE(object.property("dontEnumProperty").isValid(), false);
    }

    // change flags
    object.setProperty("flagProperty", str);
    QCOMPARE(object.propertyFlags("flagProperty"), static_cast<QScriptValue::PropertyFlags>(0));

    object.setProperty("flagProperty", str, QScriptValue::ReadOnly);
    QCOMPARE(object.propertyFlags("flagProperty"), QScriptValue::ReadOnly);

    object.setProperty("flagProperty", str, object.propertyFlags("flagProperty") | QScriptValue::SkipInEnumeration);
    QCOMPARE(object.propertyFlags("flagProperty"), QScriptValue::ReadOnly | QScriptValue::SkipInEnumeration);

    object.setProperty("flagProperty", str, QScriptValue::KeepExistingFlags);
    QCOMPARE(object.propertyFlags("flagProperty"), QScriptValue::ReadOnly | QScriptValue::SkipInEnumeration);

    object.setProperty("flagProperty", str, QScriptValue::UserRange);
    QCOMPARE(object.propertyFlags("flagProperty"), QScriptValue::UserRange);

    // flags of property in the prototype
    {
        QScriptValue object2 = eng.newObject();
        object2.setPrototype(object);
        QCOMPARE(object2.propertyFlags("flagProperty", QScriptValue::ResolveLocal), 0);
        QCOMPARE(object2.propertyFlags("flagProperty"), QScriptValue::UserRange);
    }

    // using interned strings
    QScriptString foo = eng.toStringHandle("foo");

    object.setProperty(foo, QScriptValue());
    QVERIFY(!object.property(foo).isValid());

    object.setProperty(foo, num);
    QVERIFY(object.property(foo).strictlyEquals(num));
    QVERIFY(object.property("foo").strictlyEquals(num));
    QVERIFY(object.propertyFlags(foo) == 0);

    // Setting index property on non-Array
    object.setProperty(13, num);
    QVERIFY(object.property(13).equals(num));
}

void tst_QScriptValue::arrayElementGetterSetter()
{
    QScriptEngine eng;
    QScriptValue obj = eng.newObject();
    obj.setProperty(1, eng.newFunction(getterSetter), QScriptValue::PropertyGetter|QScriptValue::PropertySetter);
    {
        QScriptValue num(123);
        obj.setProperty("x", num);
        QScriptValue ret = obj.property(1);
        QVERIFY(ret.isValid());
        QVERIFY(ret.equals(num));
    }
    {
        QScriptValue num(456);
        obj.setProperty(1, num);
        QScriptValue ret = obj.property(1);
        QVERIFY(ret.isValid());
        QVERIFY(ret.equals(num));
        QVERIFY(ret.equals(obj.property("1")));
    }
    QCOMPARE(obj.propertyFlags("1"), QScriptValue::PropertyGetter|QScriptValue::PropertySetter);

    obj.setProperty(1, QScriptValue(), QScriptValue::PropertyGetter|QScriptValue::PropertySetter);
    QVERIFY(obj.propertyFlags("1") == 0);
}

void tst_QScriptValue::getSetPrototype_cyclicPrototype()
{
    QScriptEngine eng;
    QScriptValue prototype = eng.newObject();
    QScriptValue object = eng.newObject();
    object.setPrototype(prototype);

    QScriptValue previousPrototype = prototype.prototype();
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setPrototype() failed: cyclic prototype value");
    prototype.setPrototype(prototype);
    QCOMPARE(prototype.prototype().strictlyEquals(previousPrototype), true);

    object.setPrototype(prototype);
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setPrototype() failed: cyclic prototype value");
    prototype.setPrototype(object);
    QCOMPARE(prototype.prototype().strictlyEquals(previousPrototype), true);

}

void tst_QScriptValue::getSetPrototype_evalCyclicPrototype()
{
    QScriptEngine eng;
    QScriptValue ret = eng.evaluate("o = { }; p = { }; o.__proto__ = p; p.__proto__ = o");
    QCOMPARE(eng.hasUncaughtException(), true);
    QVERIFY(ret.strictlyEquals(eng.uncaughtException()));
    QCOMPARE(ret.isError(), true);
    QCOMPARE(ret.toString(), QLatin1String("Error: cyclic __proto__ value"));
}

void tst_QScriptValue::getSetPrototype_eval()
{
    QScriptEngine eng;
    QScriptValue ret = eng.evaluate("p = { }; p.__proto__ = { }");
    QCOMPARE(eng.hasUncaughtException(), false);
    QCOMPARE(ret.isError(), false);
}

void tst_QScriptValue::getSetPrototype_invalidPrototype()
{
    QScriptEngine eng;
    QScriptValue inv;
    QScriptValue object = eng.newObject();
    QScriptValue proto = object.prototype();
    QVERIFY(object.prototype().strictlyEquals(proto));
    inv.setPrototype(object);
    QCOMPARE(inv.prototype().isValid(), false);
    object.setPrototype(inv);
    QVERIFY(object.prototype().strictlyEquals(proto));
}

void tst_QScriptValue::getSetPrototype_twoEngines()
{
    QScriptEngine eng;
    QScriptValue prototype = eng.newObject();
    QScriptValue object = eng.newObject();
    object.setPrototype(prototype);
    QScriptEngine otherEngine;
    QScriptValue newPrototype = otherEngine.newObject();
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setPrototype() failed: cannot set a prototype created in a different engine");
    object.setPrototype(newPrototype);
    QCOMPARE(object.prototype().strictlyEquals(prototype), true);

}

void tst_QScriptValue::getSetPrototype_null()
{
    QScriptEngine eng;
    QScriptValue object = eng.newObject();
    object.setPrototype(QScriptValue(QScriptValue::NullValue));
    QVERIFY(object.prototype().isNull());

    QScriptValue newProto = eng.newObject();
    object.setPrototype(newProto);
    QVERIFY(object.prototype().equals(newProto));

    object.setPrototype(QScriptValue(&eng, QScriptValue::NullValue));
    QVERIFY(object.prototype().isNull());
}

void tst_QScriptValue::getSetPrototype_notObjectOrNull()
{
    QScriptEngine eng;
    QScriptValue object = eng.newObject();
    QScriptValue originalProto = object.prototype();

    // bool
    object.setPrototype(true);
    QVERIFY(object.prototype().equals(originalProto));
    object.setPrototype(QScriptValue(&eng, true));
    QVERIFY(object.prototype().equals(originalProto));

    // number
    object.setPrototype(123);
    QVERIFY(object.prototype().equals(originalProto));
    object.setPrototype(QScriptValue(&eng, 123));
    QVERIFY(object.prototype().equals(originalProto));

    // string
    object.setPrototype("foo");
    QVERIFY(object.prototype().equals(originalProto));
    object.setPrototype(QScriptValue(&eng, "foo"));
    QVERIFY(object.prototype().equals(originalProto));

    // undefined
    object.setPrototype(QScriptValue(QScriptValue::UndefinedValue));
    QVERIFY(object.prototype().equals(originalProto));
    object.setPrototype(QScriptValue(&eng, QScriptValue::UndefinedValue));
    QVERIFY(object.prototype().equals(originalProto));
}

void tst_QScriptValue::getSetPrototype()
{
    QScriptEngine eng;
    QScriptValue prototype = eng.newObject();
    QScriptValue object = eng.newObject();
    object.setPrototype(prototype);
    QCOMPARE(object.prototype().strictlyEquals(prototype), true);
}

void tst_QScriptValue::getSetScope()
{
    QScriptEngine eng;

    QScriptValue object = eng.newObject();
    QCOMPARE(object.scope().isValid(), false);

    QScriptValue object2 = eng.newObject();
    object2.setScope(object);

    QCOMPARE(object2.scope().strictlyEquals(object), true);

    object.setProperty("foo", 123);
    QVERIFY(!object2.property("foo").isValid());
    {
        QScriptValue ret = object2.property("foo", QScriptValue::ResolveScope);
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }

    QScriptValue inv;
    inv.setScope(object);
    QCOMPARE(inv.scope().isValid(), false);

    QScriptEngine otherEngine;
    QScriptValue object3 = otherEngine.newObject();
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setScope() failed: cannot set a scope object created in a different engine");
    object2.setScope(object3);
    QCOMPARE(object2.scope().strictlyEquals(object), true);

    object2.setScope(QScriptValue());
    QVERIFY(!object2.scope().isValid());
}

void tst_QScriptValue::getSetData_objects_data()
{
    newEngine();

    QTest::addColumn<QScriptValue>("object");

    QTest::newRow("object from evaluate") << engine->evaluate("new Object()");
    QTest::newRow("object from engine") << engine->newObject();
    QTest::newRow("Array") << engine->newArray();
    QTest::newRow("Date") << engine->newDate(12324);
    QTest::newRow("QObject") << engine->newQObject(this);
    QTest::newRow("RegExp") << engine->newRegExp(QRegExp());
}

void tst_QScriptValue::getSetData_objects()
{
    QFETCH(QScriptValue, object);

    QVERIFY(!object.data().isValid());
    QScriptValue v1(true);
    object.setData(v1);
    QVERIFY(object.data().strictlyEquals(v1));
    QScriptValue v2(123);
    object.setData(v2);
    QVERIFY(object.data().strictlyEquals(v2));
    QScriptValue v3 = engine->newObject();
    object.setData(v3);
    QVERIFY(object.data().strictlyEquals(v3));
    object.setData(QScriptValue());
    QVERIFY(!object.data().isValid());
}

void tst_QScriptValue::getSetData_nonObjects_data()
{
    newEngine();

    QTest::addColumn<QScriptValue>("value");

    QTest::newRow("undefined (bound)") << engine->undefinedValue();
    QTest::newRow("null (bound)") << engine->nullValue();
    QTest::newRow("string (bound)") << QScriptValue(engine, "Pong");
    QTest::newRow("bool (bound)") << QScriptValue(engine, false);

    QTest::newRow("undefined") << QScriptValue(QScriptValue::UndefinedValue);
    QTest::newRow("null") << QScriptValue(QScriptValue::NullValue);
    QTest::newRow("string") << QScriptValue("Pong");
    QTest::newRow("bool") << QScriptValue(true);
}

void tst_QScriptValue::getSetData_nonObjects()
{
    QFETCH(QScriptValue, value);

    QVERIFY(!value.data().isValid());
    QScriptValue v1(true);
    value.setData(v1);
    QVERIFY(!value.data().isValid());
    QScriptValue v2(123);
    value.setData(v2);
    QVERIFY(!value.data().isValid());
    QScriptValue v3 = engine->newObject();
    value.setData(v3);
    QVERIFY(!value.data().isValid());
    value.setData(QScriptValue());
    QVERIFY(!value.data().isValid());
}

void tst_QScriptValue::setData_QTBUG15144()
{
    QScriptEngine eng;
    QScriptValue obj = eng.newObject();
    for (int i = 0; i < 10000; ++i) {
        // Create an object with property 'fooN' on it, and immediately kill
        // the reference to the object so it and the property name become garbage.
        eng.evaluate(QString::fromLatin1("o = {}; o.foo%0 = 10; o = null;").arg(i));
        // Setting the data will cause a JS string to be allocated, which could
        // trigger a GC. This should not cause a crash.
        obj.setData("foodfight");
    }
}

class TestScriptClass : public QScriptClass
{
public:
    TestScriptClass(QScriptEngine *engine) : QScriptClass(engine) {}
};

void tst_QScriptValue::getSetScriptClass_emptyClass_data()
{
    newEngine();
    QTest::addColumn<QScriptValue>("value");

    QTest::newRow("invalid") << QScriptValue();
    QTest::newRow("number") << QScriptValue(123);
    QTest::newRow("string") << QScriptValue("pong");
    QTest::newRow("bool") << QScriptValue(false);
    QTest::newRow("null") << QScriptValue(QScriptValue::NullValue);
    QTest::newRow("undefined") << QScriptValue(QScriptValue::UndefinedValue);

    QTest::newRow("number") << QScriptValue(engine, 123);
    QTest::newRow("string") << QScriptValue(engine, "pong");
    QTest::newRow("bool") << QScriptValue(engine, true);
    QTest::newRow("null") << QScriptValue(engine->nullValue());
    QTest::newRow("undefined") << QScriptValue(engine->undefinedValue());
    QTest::newRow("object") << QScriptValue(engine->newObject());
    QTest::newRow("date") << QScriptValue(engine->evaluate("new Date()"));
    QTest::newRow("qobject") << QScriptValue(engine->newQObject(this));
}

void tst_QScriptValue::getSetScriptClass_emptyClass()
{
    QFETCH(QScriptValue, value);
    QCOMPARE(value.scriptClass(), (QScriptClass*)0);
}

void tst_QScriptValue::getSetScriptClass_JSObjectFromCpp()
{
    QScriptEngine eng;
    TestScriptClass testClass(&eng);
    // object created in C++ (newObject())
    {
        QScriptValue obj = eng.newObject();
        obj.setScriptClass(&testClass);
        QCOMPARE(obj.scriptClass(), (QScriptClass*)&testClass);
        obj.setScriptClass(0);
        QCOMPARE(obj.scriptClass(), (QScriptClass*)0);
    }
}

void tst_QScriptValue::getSetScriptClass_JSObjectFromJS()
{
    QScriptEngine eng;
    TestScriptClass testClass(&eng);
    // object created in JS
    {
        QScriptValue obj = eng.evaluate("new Object");
        QVERIFY(!eng.hasUncaughtException());
        QVERIFY(obj.isObject());
        QCOMPARE(obj.scriptClass(), (QScriptClass*)0);
        QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setScriptClass() failed: cannot change class of non-QScriptObject");
        obj.setScriptClass(&testClass);
        QEXPECT_FAIL("", "With JSC back-end, the class of a plain object created in JS can't be changed", Continue);
        QCOMPARE(obj.scriptClass(), (QScriptClass*)&testClass);
        QTest::ignoreMessage(QtWarningMsg, "QScriptValue::setScriptClass() failed: cannot change class of non-QScriptObject");
        obj.setScriptClass(0);
        QCOMPARE(obj.scriptClass(), (QScriptClass*)0);
    }
}

void tst_QScriptValue::getSetScriptClass_QVariant()
{
    QScriptEngine eng;
    TestScriptClass testClass(&eng);
    // object that already has a(n internal) class
    {
        QScriptValue obj = eng.newVariant(QUrl("http://example.com"));
        QVERIFY(obj.isVariant());
        QCOMPARE(obj.scriptClass(), (QScriptClass*)0);
        obj.setScriptClass(&testClass);
        QCOMPARE(obj.scriptClass(), (QScriptClass*)&testClass);
        QVERIFY(obj.isObject());
        QVERIFY(!obj.isVariant());
        QCOMPARE(obj.toVariant(), QVariant(QVariantMap()));
    }
}

void tst_QScriptValue::getSetScriptClass_QObject()
{
    QScriptEngine eng;
    TestScriptClass testClass(&eng);
    {
        QScriptValue obj = eng.newQObject(this);
        QVERIFY(obj.isQObject());
        obj.setScriptClass(&testClass);
        QCOMPARE(obj.scriptClass(), (QScriptClass*)&testClass);
        QVERIFY(obj.isObject());
        QVERIFY(!obj.isQObject());
        QVERIFY(obj.toQObject() == 0);
    }
}

static QScriptValue getArg(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->argument(0);
}

static QScriptValue evaluateArg(QScriptContext *, QScriptEngine *eng)
{
    return eng->evaluate("arguments[0]");
}

static QScriptValue addArgs(QScriptContext *, QScriptEngine *eng)
{
    return eng->evaluate("arguments[0] + arguments[1]");
}

static QScriptValue returnInvalidValue(QScriptContext *, QScriptEngine *)
{
    return QScriptValue();
}

void tst_QScriptValue::call_function()
{
    QScriptEngine eng;
    QScriptValue fun = eng.evaluate("(function() { return 1; })");
    QVERIFY(fun.isFunction());
    QScriptValue result = fun.call();
    QVERIFY(result.isNumber());
    QCOMPARE(result.toInt32(), 1);
}

void tst_QScriptValue::call_object()
{
    QScriptEngine eng;
    QScriptValue Object = eng.evaluate("Object");
    QCOMPARE(Object.isFunction(), true);
    QScriptValue result = Object.call(Object);
    QCOMPARE(result.isObject(), true);
}

void tst_QScriptValue::call_newObjects()
{
    QScriptEngine eng;
    // test that call() doesn't construct new objects
    QScriptValue Number = eng.evaluate("Number");
    QScriptValue Object = eng.evaluate("Object");
    QCOMPARE(Object.isFunction(), true);
    QScriptValueList args;
    args << QScriptValue(&eng, 123);
    QScriptValue result = Number.call(Object, args);
    QCOMPARE(result.strictlyEquals(args.at(0)), true);
}

void tst_QScriptValue::call_this()
{
    QScriptEngine eng;
    // test that correct "this" object is used
    QScriptValue fun = eng.evaluate("(function() { return this; })");
    QCOMPARE(fun.isFunction(), true);

    QScriptValue numberObject = QScriptValue(&eng, 123.0).toObject();
    QScriptValue result = fun.call(numberObject);
    QCOMPARE(result.isObject(), true);
    QCOMPARE(result.toNumber(), 123.0);
}

void tst_QScriptValue::call_arguments()
{
    QScriptEngine eng;
    // test that correct arguments are passed

    QScriptValue fun = eng.evaluate("(function() { return arguments[0]; })");
    QCOMPARE(fun.isFunction(), true);
    {
        QScriptValue result = fun.call(eng.undefinedValue());
        QCOMPARE(result.isUndefined(), true);
    }
    {
        QScriptValueList args;
        args << QScriptValue(&eng, 123.0);
        QScriptValue result = fun.call(eng.undefinedValue(), args);
        QCOMPARE(result.isNumber(), true);
        QCOMPARE(result.toNumber(), 123.0);
    }
    // V2 constructors
    {
        QScriptValueList args;
        args << QScriptValue(123.0);
        QScriptValue result = fun.call(eng.undefinedValue(), args);
        QCOMPARE(result.isNumber(), true);
        QCOMPARE(result.toNumber(), 123.0);
    }
    {
        QScriptValue args = eng.newArray();
        args.setProperty(0, 123);
        QScriptValue result = fun.call(eng.undefinedValue(), args);
        QVERIFY(result.isNumber());
        QCOMPARE(result.toNumber(), 123.0);
    }
}

void tst_QScriptValue::call()
{
    QScriptEngine eng;
    {
        QScriptValue fun = eng.evaluate("(function() { return arguments[1]; })");
        QCOMPARE(fun.isFunction(), true);

        {
            QScriptValueList args;
            args << QScriptValue(&eng, 123.0) << QScriptValue(&eng, 456.0);
            QScriptValue result = fun.call(eng.undefinedValue(), args);
            QCOMPARE(result.isNumber(), true);
            QCOMPARE(result.toNumber(), 456.0);
        }
        {
            QScriptValue args = eng.newArray();
            args.setProperty(0, 123);
            args.setProperty(1, 456);
            QScriptValue result = fun.call(eng.undefinedValue(), args);
            QVERIFY(result.isNumber());
            QCOMPARE(result.toNumber(), 456.0);
        }
    }
    {
        QScriptValue fun = eng.evaluate("(function() { throw new Error('foo'); })");
        QCOMPARE(fun.isFunction(), true);
        QVERIFY(!eng.hasUncaughtException());

        {
            QScriptValue result = fun.call();
            QCOMPARE(result.isError(), true);
            QCOMPARE(eng.hasUncaughtException(), true);
            QVERIFY(result.strictlyEquals(eng.uncaughtException()));
        }
    }
    {
        eng.clearExceptions();
        QScriptValue fun = eng.newFunction(getArg);
        {
            QScriptValueList args;
            args << QScriptValue(&eng, 123.0);
            QScriptValue result = fun.call(eng.undefinedValue(), args);
            QVERIFY(!eng.hasUncaughtException());
            QCOMPARE(result.isNumber(), true);
            QCOMPARE(result.toNumber(), 123.0);
        }
        // V2 constructors
        {
            QScriptValueList args;
            args << QScriptValue(123.0);
            QScriptValue result = fun.call(eng.undefinedValue(), args);
            QCOMPARE(result.isNumber(), true);
            QCOMPARE(result.toNumber(), 123.0);
        }
        {
            QScriptValue args = eng.newArray();
            args.setProperty(0, 123);
            QScriptValue result = fun.call(eng.undefinedValue(), args);
            QVERIFY(result.isNumber());
            QCOMPARE(result.toNumber(), 123.0);
        }
    }
    {
        QScriptValue fun = eng.newFunction(evaluateArg);
        {
            QScriptValueList args;
            args << QScriptValue(&eng, 123.0);
            QScriptValue result = fun.call(eng.undefinedValue(), args);
            QVERIFY(!eng.hasUncaughtException());
            QCOMPARE(result.isNumber(), true);
            QCOMPARE(result.toNumber(), 123.0);
        }
    }
}

void tst_QScriptValue::call_invalidArguments()
{
    // test that invalid arguments are handled gracefully
    QScriptEngine eng;
    {
        QScriptValue fun = eng.newFunction(getArg);
        {
            QScriptValueList args;
            args << QScriptValue();
            QScriptValue ret = fun.call(QScriptValue(), args);
            QVERIFY(!eng.hasUncaughtException());
            QCOMPARE(ret.isValid(), true);
            QCOMPARE(ret.isUndefined(), true);
        }
    }
    {
        QScriptValue fun = eng.newFunction(evaluateArg);
        {
            QScriptValueList args;
            args << QScriptValue();
            QScriptValue ret = fun.call(QScriptValue(), args);
            QCOMPARE(ret.isValid(), true);
            QCOMPARE(ret.isUndefined(), true);
        }
    }
    {
        QScriptValue fun = eng.newFunction(addArgs);
        {
            QScriptValueList args;
            args << QScriptValue() << QScriptValue();
            QScriptValue ret = fun.call(QScriptValue(), args);
            QCOMPARE(ret.isValid(), true);
            QCOMPARE(ret.isNumber(), true);
            QCOMPARE(qIsNaN(ret.toNumber()), true);
        }
    }
}

void tst_QScriptValue::call_invalidReturn()
{
    // test that invalid return value is handled gracefully
    QScriptEngine eng;
    QScriptValue fun = eng.newFunction(returnInvalidValue);
    eng.globalObject().setProperty("returnInvalidValue", fun);
    QScriptValue ret = eng.evaluate("returnInvalidValue() + returnInvalidValue()");
    QCOMPARE(ret.isValid(), true);
    QCOMPARE(ret.isNumber(), true);
    QCOMPARE(qIsNaN(ret.toNumber()), true);
}

void tst_QScriptValue::call_twoEngines()
{
    QScriptEngine eng;
    QScriptValue object = eng.evaluate("Object");
    QScriptEngine otherEngine;
    QScriptValue fun = otherEngine.evaluate("(function() { return 1; })");
    QVERIFY(fun.isFunction());
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::call() failed: "
                         "cannot call function with thisObject created in "
                         "a different engine");
    QCOMPARE(fun.call(object).isValid(), false);
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::call() failed: "
                         "cannot call function with argument created in "
                         "a different engine");
    QCOMPARE(fun.call(QScriptValue(), QScriptValueList() << QScriptValue(&eng, 123)).isValid(), false);
    {
        QScriptValue fun = eng.evaluate("Object");
        QVERIFY(fun.isFunction());
        QScriptEngine eng2;
        QScriptValue objectInDifferentEngine = eng2.newObject();
        QScriptValueList args;
        args << objectInDifferentEngine;
        QTest::ignoreMessage(QtWarningMsg, "QScriptValue::call() failed: cannot call function with argument created in a different engine");
        fun.call(QScriptValue(), args);
    }
}

void tst_QScriptValue::call_array()
{
    QScriptEngine eng;
    QScriptValue fun = eng.evaluate("(function() { return arguments; })");
    QVERIFY(fun.isFunction());
    QScriptValue array = eng.newArray(3);
    array.setProperty(0, QScriptValue(&eng, 123.0));
    array.setProperty(1, QScriptValue(&eng, 456.0));
    array.setProperty(2, QScriptValue(&eng, 789.0));
    // call with single array object as arguments
    QScriptValue ret = fun.call(QScriptValue(), array);
    QVERIFY(!eng.hasUncaughtException());
    QCOMPARE(ret.isError(), false);
    QCOMPARE(ret.property(0).strictlyEquals(array.property(0)), true);
    QCOMPARE(ret.property(1).strictlyEquals(array.property(1)), true);
    QCOMPARE(ret.property(2).strictlyEquals(array.property(2)), true);
    // call with arguments object as arguments
    QScriptValue ret2 = fun.call(QScriptValue(), ret);
    QCOMPARE(ret2.isError(), false);
    QCOMPARE(ret2.property(0).strictlyEquals(ret.property(0)), true);
    QCOMPARE(ret2.property(1).strictlyEquals(ret.property(1)), true);
    QCOMPARE(ret2.property(2).strictlyEquals(ret.property(2)), true);
    // call with null as arguments
    QScriptValue ret3 = fun.call(QScriptValue(), eng.nullValue());
    QCOMPARE(ret3.isError(), false);
    QCOMPARE(ret3.property("length").isNumber(), true);
    QCOMPARE(ret3.property("length").toNumber(), 0.0);
    // call with undefined as arguments
    QScriptValue ret4 = fun.call(QScriptValue(), eng.undefinedValue());
    QCOMPARE(ret4.isError(), false);
    QCOMPARE(ret4.property("length").isNumber(), true);
    QCOMPARE(ret4.property("length").toNumber(), 0.0);
    // call with something else as arguments
    QScriptValue ret5 = fun.call(QScriptValue(), QScriptValue(&eng, 123.0));
    QCOMPARE(ret5.isError(), true);
    // call with a non-array object as arguments
    QScriptValue ret6 = fun.call(QScriptValue(), eng.globalObject());
    QVERIFY(ret6.isError());
    QCOMPARE(ret6.toString(), QString::fromLatin1("TypeError: Arguments must be an array"));
}


void tst_QScriptValue::call_nonFunction_data()
{
    newEngine();
    QTest::addColumn<QScriptValue>("value");

    QTest::newRow("invalid") << QScriptValue();
    QTest::newRow("bool") << QScriptValue(false);
    QTest::newRow("int") << QScriptValue(123);
    QTest::newRow("string") << QScriptValue(QString::fromLatin1("ciao"));
    QTest::newRow("undefined") << QScriptValue(QScriptValue::UndefinedValue);
    QTest::newRow("null") << QScriptValue(QScriptValue::NullValue);

    QTest::newRow("bool bound") << QScriptValue(engine, false);
    QTest::newRow("int bound") << QScriptValue(engine, 123);
    QTest::newRow("string bound") << QScriptValue(engine, QString::fromLatin1("ciao"));
    QTest::newRow("undefined bound") << engine->undefinedValue();
    QTest::newRow("null bound") << engine->nullValue();
}

void tst_QScriptValue::call_nonFunction()
{
    // calling things that are not functions
    QFETCH(QScriptValue, value);
    QVERIFY(!value.call().isValid());
}

static QScriptValue ctorReturningUndefined(QScriptContext *ctx, QScriptEngine *)
{
    ctx->thisObject().setProperty("foo", 123);
    return QScriptValue(QScriptValue::UndefinedValue);
}

static QScriptValue ctorReturningNewObject(QScriptContext *, QScriptEngine *eng)
{
    QScriptValue result = eng->newObject();
    result.setProperty("bar", 456);
    return result;
}

void tst_QScriptValue::construct_nonFunction_data()
{
    newEngine();
    QTest::addColumn<QScriptValue>("value");

    QTest::newRow("invalid") << QScriptValue();
    QTest::newRow("bool") << QScriptValue(false);
    QTest::newRow("int") << QScriptValue(123);
    QTest::newRow("string") << QScriptValue(QString::fromLatin1("ciao"));
    QTest::newRow("undefined") << QScriptValue(QScriptValue::UndefinedValue);
    QTest::newRow("null") << QScriptValue(QScriptValue::NullValue);

    QTest::newRow("bool bound") << QScriptValue(engine, false);
    QTest::newRow("int bound") << QScriptValue(engine, 123);
    QTest::newRow("string bound") << QScriptValue(engine, QString::fromLatin1("ciao"));
    QTest::newRow("undefined bound") << engine->undefinedValue();
    QTest::newRow("null bound") << engine->nullValue();
}

void tst_QScriptValue::construct_nonFunction()
{
    QFETCH(QScriptValue, value);
    QVERIFY(!value.construct().isValid());
}

void tst_QScriptValue::construct_simple()
{
    QScriptEngine eng;
    QScriptValue fun = eng.evaluate("(function () { this.foo = 123; })");
    QVERIFY(fun.isFunction());
    QScriptValue ret = fun.construct();
    QVERIFY(ret.isObject());
    QVERIFY(ret.instanceOf(fun));
    QCOMPARE(ret.property("foo").toInt32(), 123);
}

void tst_QScriptValue::construct_newObjectJS()
{
    QScriptEngine eng;
    // returning a different object overrides the default-constructed one
    QScriptValue fun = eng.evaluate("(function () { return { bar: 456 }; })");
    QVERIFY(fun.isFunction());
    QScriptValue ret = fun.construct();
    QVERIFY(ret.isObject());
    QVERIFY(!ret.instanceOf(fun));
    QCOMPARE(ret.property("bar").toInt32(), 456);
}

void tst_QScriptValue::construct_undefined()
{
    QScriptEngine eng;
    QScriptValue fun = eng.newFunction(ctorReturningUndefined);
    QScriptValue ret = fun.construct();
    QVERIFY(ret.isObject());
    QVERIFY(ret.instanceOf(fun));
    QCOMPARE(ret.property("foo").toInt32(), 123);
}

void tst_QScriptValue::construct_newObjectCpp()
{
    QScriptEngine eng;
    QScriptValue fun = eng.newFunction(ctorReturningNewObject);
    QScriptValue ret = fun.construct();
    QVERIFY(ret.isObject());
    QVERIFY(!ret.instanceOf(fun));
    QCOMPARE(ret.property("bar").toInt32(), 456);
}

void tst_QScriptValue::construct_arg()
{
    QScriptEngine eng;
    QScriptValue Number = eng.evaluate("Number");
    QCOMPARE(Number.isFunction(), true);
    QScriptValueList args;
    args << QScriptValue(&eng, 123);
    QScriptValue ret = Number.construct(args);
    QCOMPARE(ret.isObject(), true);
    QCOMPARE(ret.toNumber(), args.at(0).toNumber());
}

void tst_QScriptValue::construct_proto()
{
    QScriptEngine eng;
    // test that internal prototype is set correctly
    QScriptValue fun = eng.evaluate("(function() { return this.__proto__; })");
    QCOMPARE(fun.isFunction(), true);
    QCOMPARE(fun.property("prototype").isObject(), true);
    QScriptValue ret = fun.construct();
    QCOMPARE(fun.property("prototype").strictlyEquals(ret), true);
}

void tst_QScriptValue::construct_returnInt()
{
    QScriptEngine eng;
    // test that we return the new object even if a non-object value is returned from the function
    QScriptValue fun = eng.evaluate("(function() { return 123; })");
    QCOMPARE(fun.isFunction(), true);
    QScriptValue ret = fun.construct();
    QCOMPARE(ret.isObject(), true);
}

void tst_QScriptValue::construct_throw()
{
    QScriptEngine eng;
    QScriptValue fun = eng.evaluate("(function() { throw new Error('foo'); })");
    QCOMPARE(fun.isFunction(), true);
    QScriptValue ret = fun.construct();
    QCOMPARE(ret.isError(), true);
    QCOMPARE(eng.hasUncaughtException(), true);
    QVERIFY(ret.strictlyEquals(eng.uncaughtException()));
}

void tst_QScriptValue::construct()
{
    QScriptEngine eng;
    QScriptValue fun = eng.evaluate("(function() { return arguments; })");
    QVERIFY(fun.isFunction());
    QScriptValue array = eng.newArray(3);
    array.setProperty(0, QScriptValue(&eng, 123.0));
    array.setProperty(1, QScriptValue(&eng, 456.0));
    array.setProperty(2, QScriptValue(&eng, 789.0));
    // construct with single array object as arguments
    QScriptValue ret = fun.construct(array);
    QVERIFY(!eng.hasUncaughtException());
    QVERIFY(ret.isValid());
    QVERIFY(ret.isObject());
    QCOMPARE(ret.property(0).strictlyEquals(array.property(0)), true);
    QCOMPARE(ret.property(1).strictlyEquals(array.property(1)), true);
    QCOMPARE(ret.property(2).strictlyEquals(array.property(2)), true);
    // construct with arguments object as arguments
    QScriptValue ret2 = fun.construct(ret);
    QCOMPARE(ret2.property(0).strictlyEquals(ret.property(0)), true);
    QCOMPARE(ret2.property(1).strictlyEquals(ret.property(1)), true);
    QCOMPARE(ret2.property(2).strictlyEquals(ret.property(2)), true);
    // construct with null as arguments
    QScriptValue ret3 = fun.construct(eng.nullValue());
    QCOMPARE(ret3.isError(), false);
    QCOMPARE(ret3.property("length").isNumber(), true);
    QCOMPARE(ret3.property("length").toNumber(), 0.0);
    // construct with undefined as arguments
    QScriptValue ret4 = fun.construct(eng.undefinedValue());
    QCOMPARE(ret4.isError(), false);
    QCOMPARE(ret4.property("length").isNumber(), true);
    QCOMPARE(ret4.property("length").toNumber(), 0.0);
    // construct with something else as arguments
    QScriptValue ret5 = fun.construct(QScriptValue(&eng, 123.0));
    QCOMPARE(ret5.isError(), true);
    // construct with a non-array object as arguments
    QScriptValue ret6 = fun.construct(eng.globalObject());
    QVERIFY(ret6.isError());
    QCOMPARE(ret6.toString(), QString::fromLatin1("TypeError: Arguments must be an array"));
}

void tst_QScriptValue::construct_twoEngines()
{
    QScriptEngine engine;
    QScriptEngine otherEngine;
    QScriptValue ctor = engine.evaluate("(function (a, b) { this.foo = 123; })");
    QScriptValue arg(&otherEngine, 124567);
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::construct() failed: cannot construct function with argument created in a different engine");
    QVERIFY(!ctor.construct(arg).isValid());
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::construct() failed: cannot construct function with argument created in a different engine");
    QVERIFY(!ctor.construct(QScriptValueList() << arg << otherEngine.newObject()).isValid());
}

void tst_QScriptValue::construct_constructorThrowsPrimitive()
{
    QScriptEngine eng;
    QScriptValue fun = eng.evaluate("(function() { throw 123; })");
    QVERIFY(fun.isFunction());
    // construct(QScriptValueList)
    {
        QScriptValue ret = fun.construct();
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toNumber(), 123.0);
        QVERIFY(eng.hasUncaughtException());
        QVERIFY(ret.strictlyEquals(eng.uncaughtException()));
        eng.clearExceptions();
    }
    // construct(QScriptValue)
    {
        QScriptValue ret = fun.construct(eng.newArray());
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toNumber(), 123.0);
        QVERIFY(eng.hasUncaughtException());
        QVERIFY(ret.strictlyEquals(eng.uncaughtException()));
        eng.clearExceptions();
    }
}

void tst_QScriptValue::lessThan()
{
    QScriptEngine eng;

    QVERIFY(!QScriptValue().lessThan(QScriptValue()));

    QScriptValue num = QScriptValue(&eng, 123);
    QCOMPARE(num.lessThan(QScriptValue(&eng, 124)), true);
    QCOMPARE(num.lessThan(QScriptValue(&eng, 122)), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, 123)), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, "124")), true);
    QCOMPARE(num.lessThan(QScriptValue(&eng, "122")), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, "123")), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, qSNaN())), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, +qInf())), true);
    QCOMPARE(num.lessThan(QScriptValue(&eng, -qInf())), false);
    QCOMPARE(num.lessThan(num), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, 124).toObject()), true);
    QCOMPARE(num.lessThan(QScriptValue(&eng, 122).toObject()), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, 123).toObject()), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, "124").toObject()), true);
    QCOMPARE(num.lessThan(QScriptValue(&eng, "122").toObject()), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, "123").toObject()), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, qSNaN()).toObject()), false);
    QCOMPARE(num.lessThan(QScriptValue(&eng, +qInf()).toObject()), true);
    QCOMPARE(num.lessThan(QScriptValue(&eng, -qInf()).toObject()), false);
    QCOMPARE(num.lessThan(num.toObject()), false);
    QCOMPARE(num.lessThan(QScriptValue()), false);

    QScriptValue str = QScriptValue(&eng, "123");
    QCOMPARE(str.lessThan(QScriptValue(&eng, "124")), true);
    QCOMPARE(str.lessThan(QScriptValue(&eng, "122")), false);
    QCOMPARE(str.lessThan(QScriptValue(&eng, "123")), false);
    QCOMPARE(str.lessThan(QScriptValue(&eng, 124)), true);
    QCOMPARE(str.lessThan(QScriptValue(&eng, 122)), false);
    QCOMPARE(str.lessThan(QScriptValue(&eng, 123)), false);
    QCOMPARE(str.lessThan(str), false);
    QCOMPARE(str.lessThan(QScriptValue(&eng, "124").toObject()), true);
    QCOMPARE(str.lessThan(QScriptValue(&eng, "122").toObject()), false);
    QCOMPARE(str.lessThan(QScriptValue(&eng, "123").toObject()), false);
    QCOMPARE(str.lessThan(QScriptValue(&eng, 124).toObject()), true);
    QCOMPARE(str.lessThan(QScriptValue(&eng, 122).toObject()), false);
    QCOMPARE(str.lessThan(QScriptValue(&eng, 123).toObject()), false);
    QCOMPARE(str.lessThan(str.toObject()), false);
    QCOMPARE(str.lessThan(QScriptValue()), false);

    // V2 constructors
    QScriptValue num2 = QScriptValue(123);
    QCOMPARE(num2.lessThan(QScriptValue(124)), true);
    QCOMPARE(num2.lessThan(QScriptValue(122)), false);
    QCOMPARE(num2.lessThan(QScriptValue(123)), false);
    QCOMPARE(num2.lessThan(QScriptValue("124")), true);
    QCOMPARE(num2.lessThan(QScriptValue("122")), false);
    QCOMPARE(num2.lessThan(QScriptValue("123")), false);
    QCOMPARE(num2.lessThan(QScriptValue(qSNaN())), false);
    QCOMPARE(num2.lessThan(QScriptValue(+qInf())), true);
    QCOMPARE(num2.lessThan(QScriptValue(-qInf())), false);
    QCOMPARE(num2.lessThan(num), false);
    QCOMPARE(num2.lessThan(QScriptValue()), false);

    QScriptValue str2 = QScriptValue("123");
    QCOMPARE(str2.lessThan(QScriptValue("124")), true);
    QCOMPARE(str2.lessThan(QScriptValue("122")), false);
    QCOMPARE(str2.lessThan(QScriptValue("123")), false);
    QCOMPARE(str2.lessThan(QScriptValue(124)), true);
    QCOMPARE(str2.lessThan(QScriptValue(122)), false);
    QCOMPARE(str2.lessThan(QScriptValue(123)), false);
    QCOMPARE(str2.lessThan(str), false);
    QCOMPARE(str2.lessThan(QScriptValue()), false);

    QScriptValue obj1 = eng.newObject();
    QScriptValue obj2 = eng.newObject();
    QCOMPARE(obj1.lessThan(obj2), false);
    QCOMPARE(obj2.lessThan(obj1), false);
    QCOMPARE(obj1.lessThan(obj1), false);
    QCOMPARE(obj2.lessThan(obj2), false);

    QScriptValue date1 = eng.newDate(QDateTime(QDate(2000, 1, 1)));
    QScriptValue date2 = eng.newDate(QDateTime(QDate(1999, 1, 1)));
    QCOMPARE(date1.lessThan(date2), false);
    QCOMPARE(date2.lessThan(date1), true);
    QCOMPARE(date1.lessThan(date1), false);
    QCOMPARE(date2.lessThan(date2), false);
    QCOMPARE(date1.lessThan(QScriptValue()), false);

    QCOMPARE(QScriptValue().lessThan(date2), false);

    QScriptEngine otherEngine;
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::lessThan: "
                         "cannot compare to a value created in "
                         "a different engine");
    QCOMPARE(date1.lessThan(QScriptValue(&otherEngine, 123)), false);
}

void tst_QScriptValue::equals()
{
    QScriptEngine eng;

    QVERIFY(QScriptValue().equals(QScriptValue()));

    QScriptValue num = QScriptValue(&eng, 123);
    QCOMPARE(num.equals(QScriptValue(&eng, 123)), true);
    QCOMPARE(num.equals(QScriptValue(&eng, 321)), false);
    QCOMPARE(num.equals(QScriptValue(&eng, "123")), true);
    QCOMPARE(num.equals(QScriptValue(&eng, "321")), false);
    QCOMPARE(num.equals(QScriptValue(&eng, 123).toObject()), true);
    QCOMPARE(num.equals(QScriptValue(&eng, 321).toObject()), false);
    QCOMPARE(num.equals(QScriptValue(&eng, "123").toObject()), true);
    QCOMPARE(num.equals(QScriptValue(&eng, "321").toObject()), false);
    QVERIFY(num.toObject().equals(num));
    QCOMPARE(num.equals(QScriptValue()), false);

    QScriptValue str = QScriptValue(&eng, "123");
    QCOMPARE(str.equals(QScriptValue(&eng, "123")), true);
    QCOMPARE(str.equals(QScriptValue(&eng, "321")), false);
    QCOMPARE(str.equals(QScriptValue(&eng, 123)), true);
    QCOMPARE(str.equals(QScriptValue(&eng, 321)), false);
    QCOMPARE(str.equals(QScriptValue(&eng, "123").toObject()), true);
    QCOMPARE(str.equals(QScriptValue(&eng, "321").toObject()), false);
    QCOMPARE(str.equals(QScriptValue(&eng, 123).toObject()), true);
    QCOMPARE(str.equals(QScriptValue(&eng, 321).toObject()), false);
    QVERIFY(str.toObject().equals(str));
    QCOMPARE(str.equals(QScriptValue()), false);

    QScriptValue num2 = QScriptValue(123);
    QCOMPARE(num2.equals(QScriptValue(123)), true);
    QCOMPARE(num2.equals(QScriptValue(321)), false);
    QCOMPARE(num2.equals(QScriptValue("123")), true);
    QCOMPARE(num2.equals(QScriptValue("321")), false);
    QCOMPARE(num2.equals(QScriptValue()), false);

    QScriptValue str2 = QScriptValue("123");
    QCOMPARE(str2.equals(QScriptValue("123")), true);
    QCOMPARE(str2.equals(QScriptValue("321")), false);
    QCOMPARE(str2.equals(QScriptValue(123)), true);
    QCOMPARE(str2.equals(QScriptValue(321)), false);
    QCOMPARE(str2.equals(QScriptValue()), false);

    QScriptValue date1 = eng.newDate(QDateTime(QDate(2000, 1, 1)));
    QScriptValue date2 = eng.newDate(QDateTime(QDate(1999, 1, 1)));
    QCOMPARE(date1.equals(date2), false);
    QCOMPARE(date1.equals(date1), true);
    QCOMPARE(date2.equals(date2), true);

    QScriptValue undefined = eng.undefinedValue();
    QScriptValue null = eng.nullValue();
    QCOMPARE(undefined.equals(undefined), true);
    QCOMPARE(null.equals(null), true);
    QCOMPARE(undefined.equals(null), true);
    QCOMPARE(null.equals(undefined), true);
    QCOMPARE(undefined.equals(QScriptValue()), false);
    QCOMPARE(null.equals(QScriptValue()), false);
    QVERIFY(!null.equals(num));
    QVERIFY(!undefined.equals(num));

    QScriptValue sant = QScriptValue(&eng, true);
    QVERIFY(sant.equals(QScriptValue(&eng, 1)));
    QVERIFY(sant.equals(QScriptValue(&eng, "1")));
    QVERIFY(sant.equals(sant));
    QVERIFY(sant.equals(QScriptValue(&eng, 1).toObject()));
    QVERIFY(sant.equals(QScriptValue(&eng, "1").toObject()));
    QVERIFY(sant.equals(sant.toObject()));
    QVERIFY(sant.toObject().equals(sant));
    QVERIFY(!sant.equals(QScriptValue(&eng, 0)));
    QVERIFY(!sant.equals(undefined));
    QVERIFY(!sant.equals(null));

    QScriptValue falskt = QScriptValue(&eng, false);
    QVERIFY(falskt.equals(QScriptValue(&eng, 0)));
    QVERIFY(falskt.equals(QScriptValue(&eng, "0")));
    QVERIFY(falskt.equals(falskt));
    QVERIFY(falskt.equals(QScriptValue(&eng, 0).toObject()));
    QVERIFY(falskt.equals(QScriptValue(&eng, "0").toObject()));
    QVERIFY(falskt.equals(falskt.toObject()));
    QVERIFY(falskt.toObject().equals(falskt));
    QVERIFY(!falskt.equals(sant));
    QVERIFY(!falskt.equals(undefined));
    QVERIFY(!falskt.equals(null));

    QScriptValue obj1 = eng.newObject();
    QScriptValue obj2 = eng.newObject();
    QCOMPARE(obj1.equals(obj2), false);
    QCOMPARE(obj2.equals(obj1), false);
    QCOMPARE(obj1.equals(obj1), true);
    QCOMPARE(obj2.equals(obj2), true);

    QScriptValue qobj1 = eng.newQObject(this);
    QScriptValue qobj2 = eng.newQObject(this);
    QScriptValue qobj3 = eng.newQObject(0);
    QScriptValue qobj4 = eng.newQObject(new QObject(), QScriptEngine::ScriptOwnership);
    QVERIFY(qobj1.equals(qobj2)); // compares the QObject pointers
    QVERIFY(!qobj2.equals(qobj4)); // compares the QObject pointers
    QVERIFY(!qobj2.equals(obj2)); // compares the QObject pointers

    QScriptValue compareFun = eng.evaluate("(function(a, b) { return a == b; })");
    QVERIFY(compareFun.isFunction());
    {
        QScriptValue ret = compareFun.call(QScriptValue(), QScriptValueList() << qobj1 << qobj2);
        QVERIFY(ret.isBool());
        QVERIFY(ret.toBool());
        ret = compareFun.call(QScriptValue(), QScriptValueList() << qobj1 << qobj3);
        QVERIFY(ret.isBool());
        QVERIFY(!ret.toBool());
        ret = compareFun.call(QScriptValue(), QScriptValueList() << qobj1 << qobj4);
        QVERIFY(ret.isBool());
        QVERIFY(!ret.toBool());
        ret = compareFun.call(QScriptValue(), QScriptValueList() << qobj1 << obj1);
        QVERIFY(ret.isBool());
        QVERIFY(!ret.toBool());
    }

    {
        QScriptValue var1 = eng.newVariant(QVariant(false));
        QScriptValue var2 = eng.newVariant(QVariant(false));
        QVERIFY(var1.equals(var2));
        {
            QScriptValue ret = compareFun.call(QScriptValue(), QScriptValueList() << var1 << var2);
            QVERIFY(ret.isBool());
            QVERIFY(ret.toBool());
        }
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(false));
        QScriptValue var2 = eng.newVariant(QVariant(0));
        // QVariant::operator==() performs type conversion
        QVERIFY(var1.equals(var2));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(QStringList() << "a"));
        QScriptValue var2 = eng.newVariant(QVariant(QStringList() << "a"));
        QVERIFY(var1.equals(var2));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(QStringList() << "a"));
        QScriptValue var2 = eng.newVariant(QVariant(QStringList() << "b"));
        QVERIFY(!var1.equals(var2));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(QPoint(1, 2)));
        QScriptValue var2 = eng.newVariant(QVariant(QPoint(1, 2)));
        QVERIFY(var1.equals(var2));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(QPoint(1, 2)));
        QScriptValue var2 = eng.newVariant(QVariant(QPoint(3, 4)));
        QVERIFY(!var1.equals(var2));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(int(1)));
        QScriptValue var2 = eng.newVariant(QVariant(double(1)));
        // QVariant::operator==() performs type conversion
        QVERIFY(var1.equals(var2));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(QString::fromLatin1("123")));
        QScriptValue var2 = eng.newVariant(QVariant(double(123)));
        QScriptValue var3(QString::fromLatin1("123"));
        QScriptValue var4(123);

        QVERIFY(var1.equals(var1));
        QVERIFY(var1.equals(var2));
        QVERIFY(var1.equals(var3));
        QVERIFY(var1.equals(var4));

        QVERIFY(var2.equals(var1));
        QVERIFY(var2.equals(var2));
        QVERIFY(var2.equals(var3));
        QVERIFY(var2.equals(var4));

        QVERIFY(var3.equals(var1));
        QVERIFY(var3.equals(var2));
        QVERIFY(var3.equals(var3));
        QVERIFY(var3.equals(var4));

        QVERIFY(var4.equals(var1));
        QVERIFY(var4.equals(var2));
        QVERIFY(var4.equals(var3));
        QVERIFY(var4.equals(var4));
    }

    QScriptEngine otherEngine;
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::equals: "
                         "cannot compare to a value created in "
                         "a different engine");
    QCOMPARE(date1.equals(QScriptValue(&otherEngine, 123)), false);
}

void tst_QScriptValue::strictlyEquals()
{
    QScriptEngine eng;

    QVERIFY(QScriptValue().strictlyEquals(QScriptValue()));

    QScriptValue num = QScriptValue(&eng, 123);
    QCOMPARE(num.strictlyEquals(QScriptValue(&eng, 123)), true);
    QCOMPARE(num.strictlyEquals(QScriptValue(&eng, 321)), false);
    QCOMPARE(num.strictlyEquals(QScriptValue(&eng, "123")), false);
    QCOMPARE(num.strictlyEquals(QScriptValue(&eng, "321")), false);
    QCOMPARE(num.strictlyEquals(QScriptValue(&eng, 123).toObject()), false);
    QCOMPARE(num.strictlyEquals(QScriptValue(&eng, 321).toObject()), false);
    QCOMPARE(num.strictlyEquals(QScriptValue(&eng, "123").toObject()), false);
    QCOMPARE(num.strictlyEquals(QScriptValue(&eng, "321").toObject()), false);
    QVERIFY(!num.toObject().strictlyEquals(num));
    QVERIFY(!num.strictlyEquals(QScriptValue()));
    QVERIFY(!QScriptValue().strictlyEquals(num));

    QScriptValue str = QScriptValue(&eng, "123");
    QCOMPARE(str.strictlyEquals(QScriptValue(&eng, "123")), true);
    QCOMPARE(str.strictlyEquals(QScriptValue(&eng, "321")), false);
    QCOMPARE(str.strictlyEquals(QScriptValue(&eng, 123)), false);
    QCOMPARE(str.strictlyEquals(QScriptValue(&eng, 321)), false);
    QCOMPARE(str.strictlyEquals(QScriptValue(&eng, "123").toObject()), false);
    QCOMPARE(str.strictlyEquals(QScriptValue(&eng, "321").toObject()), false);
    QCOMPARE(str.strictlyEquals(QScriptValue(&eng, 123).toObject()), false);
    QCOMPARE(str.strictlyEquals(QScriptValue(&eng, 321).toObject()), false);
    QVERIFY(!str.toObject().strictlyEquals(str));
    QVERIFY(!str.strictlyEquals(QScriptValue()));

    QScriptValue num2 = QScriptValue(123);
    QCOMPARE(num2.strictlyEquals(QScriptValue(123)), true);
    QCOMPARE(num2.strictlyEquals(QScriptValue(321)), false);
    QCOMPARE(num2.strictlyEquals(QScriptValue("123")), false);
    QCOMPARE(num2.strictlyEquals(QScriptValue("321")), false);
    QVERIFY(!num2.strictlyEquals(QScriptValue()));

    QScriptValue str2 = QScriptValue("123");
    QCOMPARE(str2.strictlyEquals(QScriptValue("123")), true);
    QCOMPARE(str2.strictlyEquals(QScriptValue("321")), false);
    QCOMPARE(str2.strictlyEquals(QScriptValue(123)), false);
    QCOMPARE(str2.strictlyEquals(QScriptValue(321)), false);
    QVERIFY(!str2.strictlyEquals(QScriptValue()));

    QScriptValue date1 = eng.newDate(QDateTime(QDate(2000, 1, 1)));
    QScriptValue date2 = eng.newDate(QDateTime(QDate(1999, 1, 1)));
    QCOMPARE(date1.strictlyEquals(date2), false);
    QCOMPARE(date1.strictlyEquals(date1), true);
    QCOMPARE(date2.strictlyEquals(date2), true);
    QVERIFY(!date1.strictlyEquals(QScriptValue()));

    QScriptValue undefined = eng.undefinedValue();
    QScriptValue null = eng.nullValue();
    QCOMPARE(undefined.strictlyEquals(undefined), true);
    QCOMPARE(null.strictlyEquals(null), true);
    QCOMPARE(undefined.strictlyEquals(null), false);
    QCOMPARE(null.strictlyEquals(undefined), false);
    QVERIFY(!null.strictlyEquals(QScriptValue()));

    QScriptValue sant = QScriptValue(&eng, true);
    QVERIFY(!sant.strictlyEquals(QScriptValue(&eng, 1)));
    QVERIFY(!sant.strictlyEquals(QScriptValue(&eng, "1")));
    QVERIFY(sant.strictlyEquals(sant));
    QVERIFY(!sant.strictlyEquals(QScriptValue(&eng, 1).toObject()));
    QVERIFY(!sant.strictlyEquals(QScriptValue(&eng, "1").toObject()));
    QVERIFY(!sant.strictlyEquals(sant.toObject()));
    QVERIFY(!sant.toObject().strictlyEquals(sant));
    QVERIFY(!sant.strictlyEquals(QScriptValue(&eng, 0)));
    QVERIFY(!sant.strictlyEquals(undefined));
    QVERIFY(!sant.strictlyEquals(null));
    QVERIFY(!sant.strictlyEquals(QScriptValue()));

    QScriptValue falskt = QScriptValue(&eng, false);
    QVERIFY(!falskt.strictlyEquals(QScriptValue(&eng, 0)));
    QVERIFY(!falskt.strictlyEquals(QScriptValue(&eng, "0")));
    QVERIFY(falskt.strictlyEquals(falskt));
    QVERIFY(!falskt.strictlyEquals(QScriptValue(&eng, 0).toObject()));
    QVERIFY(!falskt.strictlyEquals(QScriptValue(&eng, "0").toObject()));
    QVERIFY(!falskt.strictlyEquals(falskt.toObject()));
    QVERIFY(!falskt.toObject().strictlyEquals(falskt));
    QVERIFY(!falskt.strictlyEquals(sant));
    QVERIFY(!falskt.strictlyEquals(undefined));
    QVERIFY(!falskt.strictlyEquals(null));
    QVERIFY(!falskt.strictlyEquals(QScriptValue()));

    QVERIFY(!QScriptValue(false).strictlyEquals(123));
    QVERIFY(!QScriptValue(QScriptValue::UndefinedValue).strictlyEquals(123));
    QVERIFY(!QScriptValue(QScriptValue::NullValue).strictlyEquals(123));
    QVERIFY(!QScriptValue(false).strictlyEquals("ciao"));
    QVERIFY(!QScriptValue(QScriptValue::UndefinedValue).strictlyEquals("ciao"));
    QVERIFY(!QScriptValue(QScriptValue::NullValue).strictlyEquals("ciao"));
    QVERIFY(QScriptValue(&eng, "ciao").strictlyEquals("ciao"));
    QVERIFY(QScriptValue("ciao").strictlyEquals(QScriptValue(&eng, "ciao")));
    QVERIFY(!QScriptValue("ciao").strictlyEquals(123));
    QVERIFY(!QScriptValue("ciao").strictlyEquals(QScriptValue(&eng, 123)));
    QVERIFY(!QScriptValue(123).strictlyEquals("ciao"));
    QVERIFY(!QScriptValue(123).strictlyEquals(QScriptValue(&eng, "ciao")));
    QVERIFY(!QScriptValue(&eng, 123).strictlyEquals("ciao"));

    QScriptValue obj1 = eng.newObject();
    QScriptValue obj2 = eng.newObject();
    QCOMPARE(obj1.strictlyEquals(obj2), false);
    QCOMPARE(obj2.strictlyEquals(obj1), false);
    QCOMPARE(obj1.strictlyEquals(obj1), true);
    QCOMPARE(obj2.strictlyEquals(obj2), true);
    QVERIFY(!obj1.strictlyEquals(QScriptValue()));

    QScriptValue qobj1 = eng.newQObject(this);
    QScriptValue qobj2 = eng.newQObject(this);
    QVERIFY(!qobj1.strictlyEquals(qobj2));

    {
        QScriptValue var1 = eng.newVariant(QVariant(false));
        QScriptValue var2 = eng.newVariant(QVariant(false));
        QVERIFY(!var1.strictlyEquals(var2));
        QVERIFY(!var1.strictlyEquals(QScriptValue()));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(false));
        QScriptValue var2 = eng.newVariant(QVariant(0));
        QVERIFY(!var1.strictlyEquals(var2));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(QStringList() << "a"));
        QScriptValue var2 = eng.newVariant(QVariant(QStringList() << "a"));
        QVERIFY(!var1.strictlyEquals(var2));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(QStringList() << "a"));
        QScriptValue var2 = eng.newVariant(QVariant(QStringList() << "b"));
        QVERIFY(!var1.strictlyEquals(var2));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(QPoint(1, 2)));
        QScriptValue var2 = eng.newVariant(QVariant(QPoint(1, 2)));
        QVERIFY(!var1.strictlyEquals(var2));
    }
    {
        QScriptValue var1 = eng.newVariant(QVariant(QPoint(1, 2)));
        QScriptValue var2 = eng.newVariant(QVariant(QPoint(3, 4)));
        QVERIFY(!var1.strictlyEquals(var2));
    }

    QScriptEngine otherEngine;
    QTest::ignoreMessage(QtWarningMsg, "QScriptValue::strictlyEquals: "
                         "cannot compare to a value created in "
                         "a different engine");
    QCOMPARE(date1.strictlyEquals(QScriptValue(&otherEngine, 123)), false);
}

Q_DECLARE_METATYPE(int*)
Q_DECLARE_METATYPE(double*)
Q_DECLARE_METATYPE(QColor*)
Q_DECLARE_METATYPE(QBrush*)

void tst_QScriptValue::castToPointer()
{
    QScriptEngine eng;
    {
        QScriptValue v = eng.newVariant(int(123));
        int *ip = qscriptvalue_cast<int*>(v);
        QVERIFY(ip != 0);
        QCOMPARE(*ip, 123);
        *ip = 456;
        QCOMPARE(qscriptvalue_cast<int>(v), 456);

        double *dp = qscriptvalue_cast<double*>(v);
        QVERIFY(dp == 0);

        QScriptValue v2 = eng.newVariant(qVariantFromValue(ip));
        QCOMPARE(qscriptvalue_cast<int*>(v2), ip);
    }
    {
        QColor c(123, 210, 231);
        QScriptValue v = eng.newVariant(c);
        QColor *cp = qscriptvalue_cast<QColor*>(v);
        QVERIFY(cp != 0);
        QCOMPARE(*cp, c);

        QBrush *bp = qscriptvalue_cast<QBrush*>(v);
        QVERIFY(bp == 0);

        QScriptValue v2 = eng.newVariant(qVariantFromValue(cp));
        QCOMPARE(qscriptvalue_cast<QColor*>(v2), cp);
    }
}

void tst_QScriptValue::prettyPrinter_data()
{
    QTest::addColumn<QString>("function");
    QTest::addColumn<QString>("expected");
    QTest::newRow("function() { }") << QString("function() { }") << QString("function () { }");
    QTest::newRow("function foo() { }") << QString("(function foo() { })") << QString("function foo() { }");
    QTest::newRow("function foo(bar) { }") << QString("(function foo(bar) { })") << QString("function foo(bar) { }");
    QTest::newRow("function foo(bar, baz) { }") << QString("(function foo(bar, baz) { })") << QString("function foo(bar, baz) { }");
    QTest::newRow("this") << QString("function() { this; }") << QString("function () { this; }");
    QTest::newRow("identifier") << QString("function(a) { a; }") << QString("function (a) { a; }");
    QTest::newRow("null") << QString("function() { null; }") << QString("function () { null; }");
    QTest::newRow("true") << QString("function() { true; }") << QString("function () { true; }");
    QTest::newRow("false") << QString("function() { false; }") << QString("function () { false; }");
    QTest::newRow("string") << QString("function() { 'test'; }") << QString("function () { \'test\'; }");
    QTest::newRow("string") << QString("function() { \"test\"; }") << QString("function () { \"test\"; }");
    QTest::newRow("number") << QString("function() { 123; }") << QString("function () { 123; }");
    QTest::newRow("number") << QString("function() { 123.456; }") << QString("function () { 123.456; }");
    QTest::newRow("regexp") << QString("function() { /hello/; }") << QString("function () { /hello/; }");
    QTest::newRow("regexp") << QString("function() { /hello/gim; }") << QString("function () { /hello/gim; }");
    QTest::newRow("array") << QString("function() { []; }") << QString("function () { []; }");
    QTest::newRow("array") << QString("function() { [10]; }") << QString("function () { [10]; }");
    QTest::newRow("array") << QString("function() { [10, 20, 30]; }") << QString("function () { [10, 20, 30]; }");
    QTest::newRow("array") << QString("function() { [10, 20, , 40]; }") << QString("function () { [10, 20, , 40]; }");
    QTest::newRow("array") << QString("function() { [,]; }") << QString("function () { [,]; }");
    QTest::newRow("array") << QString("function() { [, 10]; }") << QString("function () { [, 10]; }");
    QTest::newRow("array") << QString("function() { [, 10, ]; }") << QString("function () { [, 10, ]; }");
    QTest::newRow("array") << QString("function() { [, 10, ,]; }") << QString("function () { [, 10, ,]; }");
    QTest::newRow("array") << QString("function() { [[10], [20]]; }") << QString("function () { [[10], [20]]; }");
    QTest::newRow("member") << QString("function() { a.b; }") << QString("function () { a.b; }");
    QTest::newRow("member") << QString("function() { a.b.c; }") << QString("function () { a.b.c; }");
    QTest::newRow("call") << QString("function() { f(); }") << QString("function () { f(); }");
    QTest::newRow("call") << QString("function() { f(a); }") << QString("function () { f(a); }");
    QTest::newRow("call") << QString("function() { f(a, b); }") << QString("function () { f(a, b); }");
    QTest::newRow("new") << QString("function() { new C(); }") << QString("function () { new C(); }");
    QTest::newRow("new") << QString("function() { new C(a); }") << QString("function () { new C(a); }");
    QTest::newRow("new") << QString("function() { new C(a, b); }") << QString("function () { new C(a, b); }");
    QTest::newRow("++") << QString("function() { a++; }") << QString("function () { a++; }");
    QTest::newRow("++") << QString("function() { ++a; }") << QString("function () { ++a; }");
    QTest::newRow("--") << QString("function() { a--; }") << QString("function () { a--; }");
    QTest::newRow("--") << QString("function() { --a; }") << QString("function () { --a; }");
    QTest::newRow("delete") << QString("function() { delete a; }") << QString("function () { delete a; }");
    QTest::newRow("void") << QString("function() { void a; }") << QString("function () { void a; }");
    QTest::newRow("typeof") << QString("function() { typeof a; }") << QString("function () { typeof a; }");
    QTest::newRow("+") << QString("function() { +a; }") << QString("function () { +a; }");
    QTest::newRow("-") << QString("function() { -a; }") << QString("function () { -a; }");
    QTest::newRow("~") << QString("function() { ~a; }") << QString("function () { ~a; }");
    QTest::newRow("!") << QString("function() { !a; }") << QString("function () { !a; }");
    QTest::newRow("+") << QString("function() { a + b; }") << QString("function () { a + b; }");
    QTest::newRow("&&") << QString("function() { a && b; }") << QString("function () { a && b; }");
    QTest::newRow("&=") << QString("function() { a &= b; }") << QString("function () { a &= b; }");
    QTest::newRow("=") << QString("function() { a = b; }") << QString("function () { a = b; }");
    QTest::newRow("&") << QString("function() { a & b; }") << QString("function () { a & b; }");
    QTest::newRow("|") << QString("function() { a | b; }") << QString("function () { a | b; }");
    QTest::newRow("^") << QString("function() { a ^ b; }") << QString("function () { a ^ b; }");
    QTest::newRow("-=") << QString("function() { a -= b; }") << QString("function () { a -= b; }");
    QTest::newRow("/") << QString("function() { a / b; }") << QString("function () { a / b; }");
    QTest::newRow("/=") << QString("function() { a /= b; }") << QString("function () { a /= b; }");
    QTest::newRow("==") << QString("function() { a == b; }") << QString("function () { a == b; }");
    QTest::newRow(">=") << QString("function() { a >= b; }") << QString("function () { a >= b; }");
    QTest::newRow(">") << QString("function() { a > b; }") << QString("function () { a > b; }");
    QTest::newRow("in") << QString("function() { a in b; }") << QString("function () { a in b; }");
    QTest::newRow("+=") << QString("function() { a += b; }") << QString("function () { a += b; }");
    QTest::newRow("instanceof") << QString("function() { a instanceof b; }") << QString("function () { a instanceof b; }");
    QTest::newRow("<=") << QString("function() { a <= b; }") << QString("function () { a <= b; }");
    QTest::newRow("<<") << QString("function() { a << b; }") << QString("function () { a << b; }");
    QTest::newRow("<<=") << QString("function() { a <<= b; }") << QString("function () { a <<= b; }");
    QTest::newRow("<") << QString("function() { a < b; }") << QString("function () { a < b; }");
    QTest::newRow("%") << QString("function() { a % b; }") << QString("function () { a % b; }");
    QTest::newRow("%=") << QString("function() { a %= b; }") << QString("function () { a %= b; }");
    QTest::newRow("*") << QString("function() { a * b; }") << QString("function () { a * b; }");
    QTest::newRow("*=") << QString("function() { a *= b; }") << QString("function () { a *= b; }");
    QTest::newRow("!=") << QString("function() { a != b; }") << QString("function () { a != b; }");
    QTest::newRow("||") << QString("function() { a || b; }") << QString("function () { a || b; }");
    QTest::newRow("|=") << QString("function() { a |= b; }") << QString("function () { a |= b; }");
    QTest::newRow(">>") << QString("function() { a >> b; }") << QString("function () { a >> b; }");
    QTest::newRow(">>=") << QString("function() { a >>= b; }") << QString("function () { a >>= b; }");
    QTest::newRow("===") << QString("function() { a === b; }") << QString("function () { a === b; }");
    QTest::newRow("!==") << QString("function() { a !== b; }") << QString("function () { a !== b; }");
    QTest::newRow("-") << QString("function() { a - b; }") << QString("function () { a - b; }");
    QTest::newRow(">>>") << QString("function() { a >>> b; }") << QString("function () { a >>> b; }");
    QTest::newRow(">>>=") << QString("function() { a >>>= b; }") << QString("function () { a >>>= b; }");
    QTest::newRow("^=") << QString("function() { a ^= b; }") << QString("function () { a ^= b; }");
    QTest::newRow("? :") << QString("function() { a ? b : c; }") << QString("function () { a ? b : c; }");
    QTest::newRow("a; b; c") << QString("function() { a; b; c; }") << QString("function () { a; b; c; }");
    QTest::newRow("var a;") << QString("function() { var a; }") << QString("function () { var a; }");
    QTest::newRow("var a, b;") << QString("function() { var a, b; }") << QString("function () { var a, b; }");
    QTest::newRow("var a = 10;") << QString("function() { var a = 10; }") << QString("function () { var a = 10; }");
    QTest::newRow("var a, b = 20;") << QString("function() { var a, b = 20; }") << QString("function () { var a, b = 20; }");
    QTest::newRow("var a = 10, b = 20;") << QString("function() { var a = 10, b = 20; }") << QString("function () { var a = 10, b = 20; }");
    QTest::newRow("if") << QString("function() { if (a) b; }") << QString("function () { if (a) b; }");
    QTest::newRow("if") << QString("function() { if (a) { b; c; } }") << QString("function () { if (a) { b; c; } }");
    QTest::newRow("if-else") << QString("function() { if (a) b; else c; }") << QString("function () { if (a) b; else c; }");
    QTest::newRow("if-else") << QString("function() { if (a) { b; c; } else { d; e; } }") << QString("function () { if (a) { b; c; } else { d; e; } }");
    QTest::newRow("do-while") << QString("function() { do { a; } while (b); }") << QString("function () { do { a; } while (b); }");
    QTest::newRow("do-while") << QString("function() { do { a; b; c; } while (d); }") << QString("function () { do { a; b; c; } while (d); }");
    QTest::newRow("while") << QString("function() { while (a) { b; } }") << QString("function () { while (a) { b; } }");
    QTest::newRow("while") << QString("function() { while (a) { b; c; } }") << QString("function () { while (a) { b; c; } }");
    QTest::newRow("for") << QString("function() { for (a; b; c) { } }") << QString("function () { for (a; b; c) { } }");
    QTest::newRow("for") << QString("function() { for (; a; b) { } }") << QString("function () { for (; a; b) { } }");
    QTest::newRow("for") << QString("function() { for (; ; a) { } }") << QString("function () { for (; ; a) { } }");
    QTest::newRow("for") << QString("function() { for (; ; ) { } }") << QString("function () { for (; ; ) { } }");
    QTest::newRow("for") << QString("function() { for (var a; b; c) { } }") << QString("function () { for (var a; b; c) { } }");
    QTest::newRow("for") << QString("function() { for (var a, b, c; d; e) { } }") << QString("function () { for (var a, b, c; d; e) { } }");
    QTest::newRow("continue") << QString("function() { for (; ; ) { continue; } }") << QString("function () { for (; ; ) { continue; } }");
    QTest::newRow("continue") << QString("function() { for (; ; ) { continue label; } }") << QString("function () { for (; ; ) { continue label; } }");
    QTest::newRow("break") << QString("function() { for (; ; ) { break; } }") << QString("function () { for (; ; ) { break; } }");
    QTest::newRow("break") << QString("function() { for (; ; ) { break label; } }") << QString("function () { for (; ; ) { break label; } }");
    QTest::newRow("return") << QString("function() { return; }") << QString("function () { return; }");
    QTest::newRow("return") << QString("function() { return 10; }") << QString("function () { return 10; }");
    QTest::newRow("with") << QString("function() { with (a) { b; } }") << QString("function () { with (a) { b; } }");
    QTest::newRow("with") << QString("function() { with (a) { b; c; } }") << QString("function () { with (a) { b; c; } }");
    QTest::newRow("switch") << QString("function() { switch (a) { } }") << QString("function () { switch (a) { } }");
    QTest::newRow("switch") << QString("function() { switch (a) { case 1: ; } }") << QString("function () { switch (a) { case 1: ; } }");
    QTest::newRow("switch") << QString("function() { switch (a) { case 1: b; break; } }") << QString("function () { switch (a) { case 1: b; break; } }");
    QTest::newRow("switch") << QString("function() { switch (a) { case 1: b; break; case 2: break; } }") << QString("function () { switch (a) { case 1: b; break; case 2: break; } }");
    QTest::newRow("switch") << QString("function() { switch (a) { case 1: case 2: ; } }") << QString("function () { switch (a) { case 1: case 2: ; } }");
    QTest::newRow("switch") << QString("function() { switch (a) { case 1: default: ; } }") << QString("function () { switch (a) { case 1: default: ; } }");
    QTest::newRow("switch") << QString("function() { switch (a) { case 1: default: ; case 3: ; } }") << QString("function () { switch (a) { case 1: default: ; case 3: ; } }");
    QTest::newRow("label") << QString("function() { a: b; }") << QString("function () { a: b; }");
    QTest::newRow("throw") << QString("function() { throw a; }") << QString("function () { throw a; }");
    QTest::newRow("try-catch") << QString("function() { try { a; } catch (e) { b; } }") << QString("function () { try { a; } catch (e) { b; } }");
    QTest::newRow("try-finally") << QString("function() { try { a; } finally { b; } }") << QString("function () { try { a; } finally { b; } }");
    QTest::newRow("try-catch-finally") << QString("function() { try { a; } catch (e) { b; } finally { c; } }") << QString("function () { try { a; } catch (e) { b; } finally { c; } }");
    QTest::newRow("a + b + c + d") << QString("function() { a + b + c + d; }") << QString("function () { a + b + c + d; }");
    QTest::newRow("a + b - c") << QString("function() { a + b - c; }") << QString("function () { a + b - c; }");
    QTest::newRow("a + -b") << QString("function() { a + -b; }") << QString("function () { a + -b; }");
    QTest::newRow("a + ~b") << QString("function() { a + ~b; }") << QString("function () { a + ~b; }");
    QTest::newRow("a + !b") << QString("function() { a + !b; }") << QString("function () { a + !b; }");
    QTest::newRow("a + +b") << QString("function() { a + +b; }") << QString("function () { a + +b; }");
    QTest::newRow("(a + b) - c") << QString("function() { (a + b) - c; }") << QString("function () { (a + b) - c; }");
    QTest::newRow("(a - b + c") << QString("function() { a - b + c; }") << QString("function () { a - b + c; }");
    QTest::newRow("(a - (b + c)") << QString("function() { a - (b + c); }") << QString("function () { a - (b + c); }");
    QTest::newRow("a + -(b + c)") << QString("function() { a + -(b + c); }") << QString("function () { a + -(b + c); }");
    QTest::newRow("a + ~(b + c)") << QString("function() { a + ~(b + c); }") << QString("function () { a + ~(b + c); }");
    QTest::newRow("a + !(b + c)") << QString("function() { a + !(b + c); }") << QString("function () { a + !(b + c); }");
    QTest::newRow("a + +(b + c)") << QString("function() { a + +(b + c); }") << QString("function () { a + +(b + c); }");
    QTest::newRow("a + b * c") << QString("function() { a + b * c; }") << QString("function () { a + b * c; }");
    QTest::newRow("(a + b) * c") << QString("function() { (a + b) * c; }") << QString("function () { (a + b) * c; }");
    QTest::newRow("(a + b) * (c + d)") << QString("function() { (a + b) * (c + d); }") << QString("function () { (a + b) * (c + d); }");
    QTest::newRow("a + (b * c)") << QString("function() { a + (b * c); }") << QString("function () { a + (b * c); }");
    QTest::newRow("a + (b / c)") << QString("function() { a + (b / c); }") << QString("function () { a + (b / c); }");
    QTest::newRow("(a / b) * c") << QString("function() { (a / b) * c; }") << QString("function () { (a / b) * c; }");
    QTest::newRow("a / (b * c)") << QString("function() { a / (b * c); }") << QString("function () { a / (b * c); }");
    QTest::newRow("a / (b % c)") << QString("function() { a / (b % c); }") << QString("function () { a / (b % c); }");
    QTest::newRow("a && b || c") << QString("function() { a && b || c; }") << QString("function () { a && b || c; }");
    QTest::newRow("a && (b || c)") << QString("function() { a && (b || c); }") << QString("function () { a && (b || c); }");
    QTest::newRow("a & b | c") << QString("function() { a & b | c; }") << QString("function () { a & b | c; }");
    QTest::newRow("a & (b | c)") << QString("function() { a & (b | c); }") << QString("function () { a & (b | c); }");
    QTest::newRow("a & b | c ^ d") << QString("function() { a & b | c ^ d; }") << QString("function () { a & b | c ^ d; }");
    QTest::newRow("a & (b | c ^ d)") << QString("function() { a & (b | c ^ d); }") << QString("function () { a & (b | c ^ d); }");
    QTest::newRow("(a & b | c) ^ d") << QString("function() { (a & b | c) ^ d; }") << QString("function () { (a & b | c) ^ d; }");
    QTest::newRow("a << b + c") << QString("function() { a << b + c; }") << QString("function () { a << b + c; }");
    QTest::newRow("(a << b) + c") << QString("function() { (a << b) + c; }") << QString("function () { (a << b) + c; }");
    QTest::newRow("a >> b + c") << QString("function() { a >> b + c; }") << QString("function () { a >> b + c; }");
    QTest::newRow("(a >> b) + c") << QString("function() { (a >> b) + c; }") << QString("function () { (a >> b) + c; }");
    QTest::newRow("a >>> b + c") << QString("function() { a >>> b + c; }") << QString("function () { a >>> b + c; }");
    QTest::newRow("(a >>> b) + c") << QString("function() { (a >>> b) + c; }") << QString("function () { (a >>> b) + c; }");
    QTest::newRow("a == b || c != d") << QString("function() { a == b || c != d; }") << QString("function () { a == b || c != d; }");
    QTest::newRow("a == (b || c != d)") << QString("function() { a == (b || c != d); }") << QString("function () { a == (b || c != d); }");
    QTest::newRow("a === b || c !== d") << QString("function() { a === b || c !== d; }") << QString("function () { a === b || c !== d; }");
    QTest::newRow("a === (b || c !== d)") << QString("function() { a === (b || c !== d); }") << QString("function () { a === (b || c !== d); }");
    QTest::newRow("a &= b + c") << QString("function() { a &= b + c; }") << QString("function () { a &= b + c; }");
    QTest::newRow("debugger") << QString("function() { debugger }") << QString("function () { debugger; }");
}

void tst_QScriptValue::prettyPrinter()
{
    QFETCH(QString, function);
    QFETCH(QString, expected);
    QScriptEngine eng;
    QScriptValue val = eng.evaluate("(" + function + ")");
    QVERIFY(val.isFunction());
    QString actual = val.toString();
    int count = qMin(actual.size(), expected.size());
//    qDebug() << actual << expected;
    for (int i = 0; i < count; ++i) {
//        qDebug() << i << actual.at(i) << expected.at(i);
        QCOMPARE(actual.at(i), expected.at(i));
    }
    QCOMPARE(actual.size(), expected.size());
}

void tst_QScriptValue::engineDeleted()
{
    QScriptEngine *eng = new QScriptEngine;
    QScriptValue v1(eng, 123);
    QVERIFY(v1.isNumber());
    QScriptValue v2(eng, QString("ciao"));
    QVERIFY(v2.isString());
    QScriptValue v3 = eng->newObject();
    QVERIFY(v3.isObject());
    QScriptValue v4 = eng->newQObject(this);
    QVERIFY(v4.isQObject());
    QScriptValue v5 = "Hello";
    QVERIFY(v2.isString());

    delete eng;

    QVERIFY(!v1.isValid());
    QVERIFY(v1.engine() == 0);
    QVERIFY(!v2.isValid());
    QVERIFY(v2.engine() == 0);
    QVERIFY(!v3.isValid());
    QVERIFY(v3.engine() == 0);
    QVERIFY(!v4.isValid());
    QVERIFY(v4.engine() == 0);
    QVERIFY(v5.isValid());
    QVERIFY(v5.engine() == 0);

    QVERIFY(!v3.property("foo").isValid());
}

void tst_QScriptValue::valueOfWithClosure()
{
    QScriptEngine eng;
    // valueOf()
    {
        QScriptValue obj = eng.evaluate("o = {}; (function(foo) { o.valueOf = function() { return foo; } })(123); o");
        QVERIFY(obj.isObject());
        QCOMPARE(obj.toInt32(), 123);
    }
    // toString()
    {
        QScriptValue obj = eng.evaluate("o = {}; (function(foo) { o.toString = function() { return foo; } })('ciao'); o");
        QVERIFY(obj.isObject());
        QCOMPARE(obj.toString(), QString::fromLatin1("ciao"));
    }
}

void tst_QScriptValue::objectId()
{
    QCOMPARE(QScriptValue().objectId(), (qint64)-1);
    QCOMPARE(QScriptValue(QScriptValue::UndefinedValue).objectId(), (qint64)-1);
    QCOMPARE(QScriptValue(QScriptValue::NullValue).objectId(), (qint64)-1);
    QCOMPARE(QScriptValue(false).objectId(), (qint64)-1);
    QCOMPARE(QScriptValue(123).objectId(), (qint64)-1);
    QCOMPARE(QScriptValue(uint(123)).objectId(), (qint64)-1);
    QCOMPARE(QScriptValue(123.5).objectId(), (qint64)-1);
    QCOMPARE(QScriptValue("ciao").objectId(), (qint64)-1);

    QScriptEngine eng;
    QScriptValue o1 = eng.newObject();
    QVERIFY(o1.objectId() != -1);
    QScriptValue o2 = eng.newObject();
    QVERIFY(o2.objectId() != -1);
    QVERIFY(o1.objectId() != o2.objectId());

    QVERIFY(eng.objectById(o1.objectId()).strictlyEquals(o1));
    QVERIFY(eng.objectById(o2.objectId()).strictlyEquals(o2));

    qint64 globalObjectId = -1;
    {
        QScriptValue global = eng.globalObject();
        globalObjectId = global.objectId();
        QVERIFY(globalObjectId != -1);
        QVERIFY(eng.objectById(globalObjectId).strictlyEquals(global));
    }
    QScriptValue obj = eng.objectById(globalObjectId);
    QVERIFY(obj.isObject());
    QVERIFY(obj.strictlyEquals(eng.globalObject()));
}

void tst_QScriptValue::nestedObjectToVariant_data()
{
    QTest::addColumn<QString>("program");
    QTest::addColumn<QVariant>("expected");

    // Array literals
    QTest::newRow("[[]]")
        << QString::fromLatin1("[[]]")
        << QVariant(QVariantList() << (QVariant(QVariantList())));
    QTest::newRow("[[123]]")
        << QString::fromLatin1("[[123]]")
        << QVariant(QVariantList() << (QVariant(QVariantList() << 123)));
    QTest::newRow("[[], 123]")
        << QString::fromLatin1("[[], 123]")
        << QVariant(QVariantList() << QVariant(QVariantList()) << 123);

    // Cyclic arrays
    QTest::newRow("var a=[]; a.push(a)")
        << QString::fromLatin1("var a=[]; a.push(a); a")
        << QVariant(QVariantList() << QVariant(QVariantList()));
    QTest::newRow("var a=[]; a.push(123, a)")
        << QString::fromLatin1("var a=[]; a.push(123, a); a")
        << QVariant(QVariantList() << 123 << QVariant(QVariantList()));
    QTest::newRow("var a=[]; var b=[]; a.push(b); b.push(a)")
        << QString::fromLatin1("var a=[]; var b=[]; a.push(b); b.push(a); a")
        << QVariant(QVariantList() << QVariant(QVariantList() << QVariant(QVariantList())));
    QTest::newRow("var a=[]; var b=[]; a.push(123, b); b.push(456, a)")
        << QString::fromLatin1("var a=[]; var b=[]; a.push(123, b); b.push(456, a); a")
        << QVariant(QVariantList() << 123 << QVariant(QVariantList() << 456 << QVariant(QVariantList())));

    // Object literals
    {
        QVariantMap m;
        m["a"] = QVariantMap();
        QTest::newRow("{ a:{} }")
            << QString::fromLatin1("({ a:{} })")
            << QVariant(m);
    }
    {
        QVariantMap m, m2;
        m2["b"] = 10;
        m2["c"] = 20;
        m["a"] = m2;
        QTest::newRow("{ a:{b:10, c:20} }")
            << QString::fromLatin1("({ a:{b:10, c:20} })")
            << QVariant(m);
    }
    {
        QVariantMap m;
        m["a"] = 10;
        m["b"] = QVariantList() << 20 << 30;
        QTest::newRow("{ a:10, b:[20, 30]}")
            << QString::fromLatin1("({ a:10, b:[20,30]})")
            << QVariant(m);
    }

    // Cyclic objects
    {
        QVariantMap m;
        m["p"] = QVariantMap();
        QTest::newRow("var o={}; o.p=o")
            << QString::fromLatin1("var o={}; o.p=o; o")
            << QVariant(m);
    }
    {
        QVariantMap m;
        m["p"] = 123;
        m["q"] = QVariantMap();
        QTest::newRow("var o={}; o.p=123; o.q=o")
            << QString::fromLatin1("var o={}; o.p=123; o.q=o; o")
            << QVariant(m);
    }
}

void tst_QScriptValue::nestedObjectToVariant()
{
    QScriptEngine eng;
    QFETCH(QString, program);
    QFETCH(QVariant, expected);
    QScriptValue o = eng.evaluate(program);
    QVERIFY(!o.isError());
    QVERIFY(o.isObject());
    QCOMPARE(o.toVariant(), expected);
}

void tst_QScriptValue::propertyFlags_data()
{
    QTest::addColumn<QString>("program");
    QTest::addColumn<uint>("expected");

    QTest::newRow("nothing") << "" << 0u;
    QTest::newRow("getter") << "o.__defineGetter__('prop', function() { return 'blah' } );\n" << uint(QScriptValue::PropertyGetter);
    QTest::newRow("setter") << "o.__defineSetter__('prop', function(a) { this.setted_prop2 = a; } );\n" << uint(QScriptValue::PropertySetter);
    QTest::newRow("getterSetter") <<  "o.__defineGetter__('prop', function() { return 'ploup' } );\n"
                                      "o.__defineSetter__('prop', function(a) { this.setted_prop3 = a; } );\n" << uint(QScriptValue::PropertySetter|QScriptValue::PropertyGetter);
    QTest::newRow("nothing2") << "o.prop = 'nothing'" << 0u;
}

void tst_QScriptValue::propertyFlags()
{
    QFETCH(QString, program);
    QFETCH(uint, expected);
    QScriptEngine eng;
    eng.evaluate("o = new Object;");
    eng.evaluate(program);
    QScriptValue o = eng.evaluate("o");

    QCOMPARE(uint(o.propertyFlags("prop")), expected);
}


QTEST_MAIN(tst_QScriptValue)
