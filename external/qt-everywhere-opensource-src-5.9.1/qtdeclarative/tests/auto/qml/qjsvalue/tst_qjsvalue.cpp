/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "tst_qjsvalue.h"
#include <QtWidgets/QPushButton>

tst_QJSValue::tst_QJSValue()
    : engine(0)
{
}

tst_QJSValue::~tst_QJSValue()
{
    delete engine;
}

void tst_QJSValue::ctor_invalid()
{
    QJSEngine eng;
    {
        QJSValue v;
        QVERIFY(v.isUndefined());
        QCOMPARE(v.engine(), (QJSEngine *)0);
    }
}

void tst_QJSValue::ctor_undefinedWithEngine()
{
    QJSEngine eng;
    {
        QJSValue v = eng.toScriptValue(QVariant());
        QVERIFY(v.isUndefined());
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QJSValue::ctor_nullWithEngine()
{
    QJSEngine eng;
    {
        QJSValue v = eng.evaluate("null");
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isNull(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QJSValue::ctor_boolWithEngine()
{
    QJSEngine eng;
    {
        QJSValue v = eng.toScriptValue(false);
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isBool(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toBool(), false);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QJSValue::ctor_intWithEngine()
{
    QJSEngine eng;
    {
        QJSValue v = eng.toScriptValue(int(1));
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QJSValue::ctor_int()
{
    {
        QJSValue v(int(0x43211234));
        QVERIFY(v.isNumber());
        QCOMPARE(v.toInt(), 0x43211234);
    }
    {
        QJSValue v(int(1));
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QJSEngine *)0);
    }
}

void tst_QJSValue::ctor_uintWithEngine()
{
    QJSEngine eng;
    {
        QJSValue v = eng.toScriptValue(uint(1));
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QJSValue::ctor_uint()
{
    {
        QJSValue v(uint(0x43211234));
        QVERIFY(v.isNumber());
        QCOMPARE(v.toUInt(), uint(0x43211234));
    }
    {
        QJSValue v(uint(1));
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QJSEngine *)0);
    }
}

void tst_QJSValue::ctor_floatWithEngine()
{
    QJSEngine eng;
    {
        QJSValue v = eng.toScriptValue(float(1.0));
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QJSValue::ctor_float()
{
    {
        QJSValue v(12345678910.5);
        QVERIFY(v.isNumber());
        QCOMPARE(v.toNumber(), 12345678910.5);
    }
    {
        QJSValue v(1.0);
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QJSEngine *)0);
    }
}

void tst_QJSValue::ctor_stringWithEngine()
{
    QJSEngine eng;
    {
        QJSValue v = eng.toScriptValue(QString::fromLatin1("ciao"));
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), &eng);
    }
}

void tst_QJSValue::ctor_string()
{
    {
        QJSValue v(QString("ciao"));
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), (QJSEngine *)0);
    }
    {
        QJSValue v("ciao");
        QVERIFY(!v.isUndefined());
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), (QJSEngine *)0);
    }
}

void tst_QJSValue::ctor_copyAndAssignWithEngine()
{
    QJSEngine eng;
    // copy constructor, operator=
    {
        QJSValue v = eng.toScriptValue(1.0);
        QJSValue v2(v);
        QCOMPARE(v2.strictlyEquals(v), true);
        QCOMPARE(v2.engine(), &eng);

        QJSValue v3(v);
        QCOMPARE(v3.strictlyEquals(v), true);
        QCOMPARE(v3.strictlyEquals(v2), true);
        QCOMPARE(v3.engine(), &eng);

        QJSValue v4 = eng.toScriptValue(2.0);
        QCOMPARE(v4.strictlyEquals(v), false);
        v3 = v4;
        QCOMPARE(v3.strictlyEquals(v), false);
        QCOMPARE(v3.strictlyEquals(v4), true);

        v2 = QJSValue();
        QCOMPARE(v2.strictlyEquals(v), false);
        QCOMPARE(v.toNumber(), 1.0);

        QJSValue v5(v);
        QCOMPARE(v5.strictlyEquals(v), true);
        v = QJSValue();
        QCOMPARE(v5.strictlyEquals(v), false);
        QCOMPARE(v5.toNumber(), 1.0);
    }
}

void tst_QJSValue::ctor_undefined()
{
    QJSValue v(QJSValue::UndefinedValue);
    QVERIFY(v.isUndefined());
    QCOMPARE(v.isObject(), false);
    QCOMPARE(v.engine(), (QJSEngine *)0);
}

void tst_QJSValue::ctor_null()
{
    QJSValue v(QJSValue::NullValue);
    QVERIFY(!v.isUndefined());
    QCOMPARE(v.isNull(), true);
    QCOMPARE(v.isObject(), false);
    QCOMPARE(v.engine(), (QJSEngine *)0);
}

void tst_QJSValue::ctor_bool()
{
    QJSValue v(false);
    QVERIFY(!v.isUndefined());
    QCOMPARE(v.isBool(), true);
    QCOMPARE(v.isBool(), true);
    QCOMPARE(v.isObject(), false);
    QCOMPARE(v.toBool(), false);
    QCOMPARE(v.engine(), (QJSEngine *)0);
}

void tst_QJSValue::ctor_copyAndAssign()
{
    QJSValue v(1.0);
    QJSValue v2(v);
    QCOMPARE(v2.strictlyEquals(v), true);
    QCOMPARE(v2.engine(), (QJSEngine *)0);

    QJSValue v3(v);
    QCOMPARE(v3.strictlyEquals(v), true);
    QCOMPARE(v3.strictlyEquals(v2), true);
    QCOMPARE(v3.engine(), (QJSEngine *)0);

    QJSValue v4(2.0);
    QCOMPARE(v4.strictlyEquals(v), false);
    v3 = v4;
    QCOMPARE(v3.strictlyEquals(v), false);
    QCOMPARE(v3.strictlyEquals(v4), true);

    v2 = QJSValue();
    QCOMPARE(v2.strictlyEquals(v), false);
    QCOMPARE(v.toNumber(), 1.0);

    QJSValue v5(v);
    QCOMPARE(v5.strictlyEquals(v), true);
    v = QJSValue();
    QCOMPARE(v5.strictlyEquals(v), false);
    QCOMPARE(v5.toNumber(), 1.0);
}

static QJSValue createUnboundValue(const QJSValue &value)
{
    QVariant variant = QVariant::fromValue(value);
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QDataStream stream(&buffer);
    variant.save(stream);
    buffer.seek(0);
    QVariant resultVariant;
    resultVariant.load(stream);
    return resultVariant.value<QJSValue>();
}

void tst_QJSValue::toString()
{
    QJSEngine eng;

    QJSValue undefined = eng.toScriptValue(QVariant());
    QCOMPARE(undefined.toString(), QString("undefined"));
    QCOMPARE(qjsvalue_cast<QString>(undefined), QString());

    QJSValue null = eng.evaluate("null");
    QCOMPARE(null.toString(), QString("null"));
    QCOMPARE(qjsvalue_cast<QString>(null), QString());

    {
        QJSValue falskt = eng.toScriptValue(false);
        QCOMPARE(falskt.toString(), QString("false"));
        QCOMPARE(qjsvalue_cast<QString>(falskt), QString("false"));

        QJSValue sant = eng.toScriptValue(true);
        QCOMPARE(sant.toString(), QString("true"));
        QCOMPARE(qjsvalue_cast<QString>(sant), QString("true"));
    }
    {
        QJSValue number = eng.toScriptValue(123);
        QCOMPARE(number.toString(), QString("123"));
        QCOMPARE(qjsvalue_cast<QString>(number), QString("123"));
    }
    {
        QJSValue number = eng.toScriptValue(6.37e-8);
        QCOMPARE(number.toString(), QString("6.37e-8"));
    }
    {
        QJSValue number = eng.toScriptValue(-6.37e-8);
        QCOMPARE(number.toString(), QString("-6.37e-8"));

        QJSValue str = eng.toScriptValue(QString("ciao"));
        QCOMPARE(str.toString(), QString("ciao"));
        QCOMPARE(qjsvalue_cast<QString>(str), QString("ciao"));
    }

    QJSValue object = eng.newObject();
    QCOMPARE(object.toString(), QString("[object Object]"));
    QCOMPARE(qjsvalue_cast<QString>(object), QString("[object Object]"));

    // toString() that throws exception
    {
        QJSValue objectObject = eng.evaluate(
            "(function(){"
            "  o = { };"
            "  o.toString = function() { throw new Error('toString'); };"
            "  return o;"
            "})()");
        QCOMPARE(objectObject.toString(), QLatin1String("Error: toString"));
    }
    {
        QJSValue objectObject = eng.evaluate(
            "(function(){"
            "  var f = function() {};"
            "  f.prototype = Date;"
            "  return new f;"
            "})()");
        QVERIFY(!objectObject.isError());
        QVERIFY(objectObject.isObject());
        QCOMPARE(objectObject.toString(), QString::fromLatin1("TypeError: Type error"));
    }

    QJSValue inv = QJSValue();
    QCOMPARE(inv.toString(), QString::fromLatin1("undefined"));

    // V2 constructors
    {
        QJSValue falskt = QJSValue(false);
        QCOMPARE(falskt.toString(), QString("false"));
        QCOMPARE(qjsvalue_cast<QString>(falskt), QString("false"));

        QJSValue sant = QJSValue(true);
        QCOMPARE(sant.toString(), QString("true"));
        QCOMPARE(qjsvalue_cast<QString>(sant), QString("true"));

        QJSValue number = QJSValue(123);
        QCOMPARE(number.toString(), QString("123"));
        QCOMPARE(qjsvalue_cast<QString>(number), QString("123"));

        QJSValue number2(int(0x43211234));
        QCOMPARE(number2.toString(), QString("1126240820"));

        QJSValue str = QJSValue(QString("ciao"));
        QCOMPARE(str.toString(), QString("ciao"));
        QCOMPARE(qjsvalue_cast<QString>(str), QString("ciao"));
    }

    // variant should use internal valueOf(), then fall back to QVariant::toString(),
    // then fall back to "QVariant(typename)"
    QJSValue variant = eng.toScriptValue(QPoint(10, 20));
    QVERIFY(variant.isVariant());
    QCOMPARE(variant.toString(), QString::fromLatin1("QVariant(QPoint)"));
    variant = eng.toScriptValue(QUrl());
    QVERIFY(variant.isVariant());
    QVERIFY(variant.toString().isEmpty());

    {
        QJSValue o = eng.newObject();
        o.setProperty(QStringLiteral("test"), 42);
        QCOMPARE(o.toString(), QStringLiteral("[object Object]"));

        o = createUnboundValue(o);
        QVERIFY(!o.engine());
        QCOMPARE(o.toString(), QStringLiteral("[object Object]"));
    }

    {
        QJSValue o = eng.newArray();
        o.setProperty(0, 1);
        o.setProperty(1, 2);
        o.setProperty(2, 3);
        QCOMPARE(o.toString(), QStringLiteral("1,2,3"));

        o = createUnboundValue(o);
        QVERIFY(!o.engine());
        QCOMPARE(o.toString(), QStringLiteral("1,2,3"));
    }

    {
        QByteArray hello = QByteArrayLiteral("Hello World");
        QJSValue jsValue = eng.toScriptValue(hello);
        QCOMPARE(jsValue.toString(), QString::fromUtf8(hello));
    }
}

void tst_QJSValue::toNumber()
{
    QJSEngine eng;

    QJSValue undefined = eng.toScriptValue(QVariant());
    QCOMPARE(qIsNaN(undefined.toNumber()), true);
    QCOMPARE(qIsNaN(qjsvalue_cast<qreal>(undefined)), true);

    QJSValue null = eng.evaluate("null");
    QCOMPARE(null.toNumber(), 0.0);
    QCOMPARE(qjsvalue_cast<qreal>(null), 0.0);
    QCOMPARE(createUnboundValue(null).toNumber(), 0.0);

    {
        QJSValue falskt = eng.toScriptValue(false);
        QCOMPARE(falskt.toNumber(), 0.0);
        QCOMPARE(createUnboundValue(falskt).toNumber(), 0.0);
        QCOMPARE(qjsvalue_cast<qreal>(falskt), 0.0);

        QJSValue sant = eng.toScriptValue(true);
        QCOMPARE(sant.toNumber(), 1.0);
        QCOMPARE(createUnboundValue(sant).toNumber(), 1.0);
        QCOMPARE(qjsvalue_cast<qreal>(sant), 1.0);

        QJSValue number = eng.toScriptValue(123.0);
        QCOMPARE(number.toNumber(), 123.0);
        QCOMPARE(qjsvalue_cast<qreal>(number), 123.0);
        QCOMPARE(createUnboundValue(number).toNumber(), 123.0);

        QJSValue str = eng.toScriptValue(QString("ciao"));
        QCOMPARE(qIsNaN(str.toNumber()), true);
        QCOMPARE(qIsNaN(qjsvalue_cast<qreal>(str)), true);
        QCOMPARE(qIsNaN(createUnboundValue(str).toNumber()), true);

        QJSValue str2 = eng.toScriptValue(QString("123"));
        QCOMPARE(str2.toNumber(), 123.0);
        QCOMPARE(qjsvalue_cast<qreal>(str2), 123.0);
        QCOMPARE(createUnboundValue(str2).toNumber(), 123.0);
    }

    QJSValue object = eng.newObject();
    QCOMPARE(qIsNaN(object.toNumber()), true);
    QCOMPARE(qIsNaN(createUnboundValue(object).toNumber()), true);
    QCOMPARE(qIsNaN(qjsvalue_cast<qreal>(object)), true);

    QJSValue inv = QJSValue();
    QVERIFY(qIsNaN(inv.toNumber()));
    QCOMPARE(qIsNaN(createUnboundValue(inv).toNumber()), true);
    QVERIFY(qIsNaN(qjsvalue_cast<qreal>(inv)));

    // V2 constructors
    {
        QJSValue falskt = QJSValue(false);
        QCOMPARE(falskt.toNumber(), 0.0);
        QCOMPARE(qjsvalue_cast<qreal>(falskt), 0.0);

        QJSValue sant = QJSValue(true);
        QCOMPARE(sant.toNumber(), 1.0);
        QCOMPARE(qjsvalue_cast<qreal>(sant), 1.0);

        QJSValue number = QJSValue(123.0);
        QCOMPARE(number.toNumber(), 123.0);
        QCOMPARE(qjsvalue_cast<qreal>(number), 123.0);

        QJSValue number2(int(0x43211234));
        QCOMPARE(number2.toNumber(), 1126240820.0);

        QJSValue str = QJSValue(QString("ciao"));
        QCOMPARE(qIsNaN(str.toNumber()), true);
        QCOMPARE(qIsNaN(qjsvalue_cast<qreal>(str)), true);

        QJSValue str2 = QJSValue(QString("123"));
        QCOMPARE(str2.toNumber(), 123.0);
        QCOMPARE(qjsvalue_cast<qreal>(str2), 123.0);
    }
}

void tst_QJSValue::toBoolean() // deprecated
{
    QJSEngine eng;

    QJSValue undefined = eng.toScriptValue(QVariant());
    QCOMPARE(undefined.toBool(), false);
    QCOMPARE(qjsvalue_cast<bool>(undefined), false);

    QJSValue null = eng.evaluate("null");
    QCOMPARE(null.toBool(), false);
    QCOMPARE(qjsvalue_cast<bool>(null), false);

    {
        QJSValue falskt = eng.toScriptValue(false);
        QCOMPARE(falskt.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(falskt), false);

        QJSValue sant = eng.toScriptValue(true);
        QCOMPARE(sant.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(sant), true);

        QJSValue number = eng.toScriptValue(0.0);
        QCOMPARE(number.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(number), false);

        QJSValue number2 = eng.toScriptValue(qQNaN());
        QCOMPARE(number2.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(number2), false);

        QJSValue number3 = eng.toScriptValue(123.0);
        QCOMPARE(number3.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(number3), true);

        QJSValue number4 = eng.toScriptValue(-456.0);
        QCOMPARE(number4.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(number4), true);

        QJSValue str = eng.toScriptValue(QString(""));
        QCOMPARE(str.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(str), false);

        QJSValue str2 = eng.toScriptValue(QString("123"));
        QCOMPARE(str2.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(str2), true);
    }

    QJSValue object = eng.newObject();
    QCOMPARE(object.toBool(), true);
    QCOMPARE(qjsvalue_cast<bool>(object), true);

    QJSValue inv = QJSValue();
    QCOMPARE(inv.toBool(), false);
    QCOMPARE(qjsvalue_cast<bool>(inv), false);

    // V2 constructors
    {
        QJSValue falskt = QJSValue(false);
        QCOMPARE(falskt.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(falskt), false);

        QJSValue sant = QJSValue(true);
        QCOMPARE(sant.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(sant), true);

        QJSValue number = QJSValue(0.0);
        QCOMPARE(number.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(number), false);

        QJSValue number2 = QJSValue(qQNaN());
        QCOMPARE(number2.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(number2), false);

        QJSValue number3 = QJSValue(123.0);
        QCOMPARE(number3.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(number3), true);

        QJSValue number4 = QJSValue(-456.0);
        QCOMPARE(number4.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(number4), true);

        QJSValue number5 = QJSValue(0x43211234);
        QCOMPARE(number5.toBool(), true);

        QJSValue str = QJSValue(QString(""));
        QCOMPARE(str.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(str), false);

        QJSValue str2 = QJSValue(QString("123"));
        QCOMPARE(str2.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(str2), true);
    }
}

void tst_QJSValue::toBool()
{
    QJSEngine eng;

    QJSValue undefined = eng.toScriptValue(QVariant());
    QCOMPARE(undefined.toBool(), false);
    QCOMPARE(qjsvalue_cast<bool>(undefined), false);

    QJSValue null = eng.evaluate("null");
    QCOMPARE(null.toBool(), false);
    QCOMPARE(qjsvalue_cast<bool>(null), false);

    {
        QJSValue falskt = eng.toScriptValue(false);
        QCOMPARE(falskt.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(falskt), false);

        QJSValue sant = eng.toScriptValue(true);
        QCOMPARE(sant.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(sant), true);

        QJSValue number = eng.toScriptValue(0.0);
        QCOMPARE(number.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(number), false);

        QJSValue number2 = eng.toScriptValue(qQNaN());
        QCOMPARE(number2.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(number2), false);

        QJSValue number3 = eng.toScriptValue(123.0);
        QCOMPARE(number3.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(number3), true);

        QJSValue number4 = eng.toScriptValue(-456.0);
        QCOMPARE(number4.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(number4), true);

        QJSValue str = eng.toScriptValue(QString(""));
        QCOMPARE(str.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(str), false);

        QJSValue str2 = eng.toScriptValue(QString("123"));
        QCOMPARE(str2.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(str2), true);
    }

    QJSValue object = eng.newObject();
    QCOMPARE(object.toBool(), true);
    QCOMPARE(qjsvalue_cast<bool>(object), true);

    QJSValue inv = QJSValue();
    QCOMPARE(inv.toBool(), false);
    QCOMPARE(qjsvalue_cast<bool>(inv), false);

    // V2 constructors
    {
        QJSValue falskt = QJSValue(false);
        QCOMPARE(falskt.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(falskt), false);

        QJSValue sant = QJSValue(true);
        QCOMPARE(sant.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(sant), true);

        QJSValue number = QJSValue(0.0);
        QCOMPARE(number.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(number), false);

        QJSValue number2 = QJSValue(qQNaN());
        QCOMPARE(number2.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(number2), false);

        QJSValue number3 = QJSValue(123.0);
        QCOMPARE(number3.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(number3), true);

        QJSValue number4 = QJSValue(-456.0);
        QCOMPARE(number4.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(number4), true);

        QJSValue number5 = QJSValue(0x43211234);
        QCOMPARE(number5.toBool(), true);

        QJSValue str = QJSValue(QString(""));
        QCOMPARE(str.toBool(), false);
        QCOMPARE(qjsvalue_cast<bool>(str), false);

        QJSValue str2 = QJSValue(QString("123"));
        QCOMPARE(str2.toBool(), true);
        QCOMPARE(qjsvalue_cast<bool>(str2), true);
    }
}

void tst_QJSValue::toInt()
{
    QJSEngine eng;

    {
        QJSValue zer0 = eng.toScriptValue(0.0);
        QCOMPARE(zer0.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(zer0), 0);

        QJSValue number = eng.toScriptValue(123.0);
        QCOMPARE(number.toInt(), 123);
        QCOMPARE(qjsvalue_cast<qint32>(number), 123);

        QJSValue number2 = eng.toScriptValue(qQNaN());
        QCOMPARE(number2.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(number2), 0);

        QJSValue number3 = eng.toScriptValue(+qInf());
        QCOMPARE(number3.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(number3), 0);

        QJSValue number3_2 = eng.toScriptValue(-qInf());
        QCOMPARE(number3_2.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(number3_2), 0);

        QJSValue number4 = eng.toScriptValue(0.5);
        QCOMPARE(number4.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(number4), 0);

        QJSValue number5 = eng.toScriptValue(123.5);
        QCOMPARE(number5.toInt(), 123);
        QCOMPARE(qjsvalue_cast<qint32>(number5), 123);

        QJSValue number6 = eng.toScriptValue(-456.5);
        QCOMPARE(number6.toInt(), -456);
        QCOMPARE(qjsvalue_cast<qint32>(number6), -456);

        QJSValue str = eng.toScriptValue(QString::fromLatin1("123.0"));
        QCOMPARE(str.toInt(), 123);
        QCOMPARE(qjsvalue_cast<qint32>(str), 123);

        QJSValue str2 = eng.toScriptValue(QString::fromLatin1("NaN"));
        QCOMPARE(str2.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(str2), 0);

        QJSValue str3 = eng.toScriptValue(QString::fromLatin1("Infinity"));
        QCOMPARE(str3.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(str3), 0);

        QJSValue str3_2 = eng.toScriptValue(QString::fromLatin1("-Infinity"));
        QCOMPARE(str3_2.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(str3_2), 0);

        QJSValue str4 = eng.toScriptValue(QString::fromLatin1("0.5"));
        QCOMPARE(str4.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(str4), 0);

        QJSValue str5 = eng.toScriptValue(QString::fromLatin1("123.5"));
        QCOMPARE(str5.toInt(), 123);
        QCOMPARE(qjsvalue_cast<qint32>(str5), 123);

        QJSValue str6 = eng.toScriptValue(QString::fromLatin1("-456.5"));
        QCOMPARE(str6.toInt(), -456);
        QCOMPARE(qjsvalue_cast<qint32>(str6), -456);
    }
    // V2 constructors
    {
        QJSValue zer0 = QJSValue(0.0);
        QCOMPARE(zer0.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(zer0), 0);

        QJSValue number = QJSValue(123.0);
        QCOMPARE(number.toInt(), 123);
        QCOMPARE(qjsvalue_cast<qint32>(number), 123);

        QJSValue number2 = QJSValue(qQNaN());
        QCOMPARE(number2.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(number2), 0);

        QJSValue number3 = QJSValue(+qInf());
        QCOMPARE(number3.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(number3), 0);

        QJSValue number3_2 = QJSValue(-qInf());
        QCOMPARE(number3_2.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(number3_2), 0);

        QJSValue number4 = QJSValue(0.5);
        QCOMPARE(number4.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(number4), 0);

        QJSValue number5 = QJSValue(123.5);
        QCOMPARE(number5.toInt(), 123);
        QCOMPARE(qjsvalue_cast<qint32>(number5), 123);

        QJSValue number6 = QJSValue(-456.5);
        QCOMPARE(number6.toInt(), -456);
        QCOMPARE(qjsvalue_cast<qint32>(number6), -456);

        QJSValue number7 = QJSValue(0x43211234);
        QCOMPARE(number7.toInt(), 0x43211234);

        QJSValue str = QJSValue("123.0");
        QCOMPARE(str.toInt(), 123);
        QCOMPARE(qjsvalue_cast<qint32>(str), 123);

        QJSValue str2 = QJSValue("NaN");
        QCOMPARE(str2.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(str2), 0);

        QJSValue str3 = QJSValue("Infinity");
        QCOMPARE(str3.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(str3), 0);

        QJSValue str3_2 = QJSValue("-Infinity");
        QCOMPARE(str3_2.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(str3_2), 0);

        QJSValue str4 = QJSValue("0.5");
        QCOMPARE(str4.toInt(), 0);
        QCOMPARE(qjsvalue_cast<qint32>(str4), 0);

        QJSValue str5 = QJSValue("123.5");
        QCOMPARE(str5.toInt(), 123);
        QCOMPARE(qjsvalue_cast<qint32>(str5), 123);

        QJSValue str6 = QJSValue("-456.5");
        QCOMPARE(str6.toInt(), -456);
        QCOMPARE(qjsvalue_cast<qint32>(str6), -456);
    }

    QJSValue inv;
    QCOMPARE(inv.toInt(), 0);
    QCOMPARE(qjsvalue_cast<qint32>(inv), 0);
}

void tst_QJSValue::toUInt()
{
    QJSEngine eng;

    {
        QJSValue zer0 = eng.toScriptValue(0.0);
        QCOMPARE(zer0.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(zer0), quint32(0));

        QJSValue number = eng.toScriptValue(123.0);
        QCOMPARE(number.toUInt(), quint32(123));
        QCOMPARE(qjsvalue_cast<quint32>(number), quint32(123));

        QJSValue number2 = eng.toScriptValue(qQNaN());
        QCOMPARE(number2.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(number2), quint32(0));

        QJSValue number3 = eng.toScriptValue(+qInf());
        QCOMPARE(number3.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(number3), quint32(0));

        QJSValue number3_2 = eng.toScriptValue(-qInf());
        QCOMPARE(number3_2.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(number3_2), quint32(0));

        QJSValue number4 = eng.toScriptValue(0.5);
        QCOMPARE(number4.toUInt(), quint32(0));

        QJSValue number5 = eng.toScriptValue(123.5);
        QCOMPARE(number5.toUInt(), quint32(123));

        QJSValue number6 = eng.toScriptValue(-456.5);
        QCOMPARE(number6.toUInt(), quint32(-456));
        QCOMPARE(qjsvalue_cast<quint32>(number6), quint32(-456));

        QJSValue str = eng.toScriptValue(QString::fromLatin1("123.0"));
        QCOMPARE(str.toUInt(), quint32(123));
        QCOMPARE(qjsvalue_cast<quint32>(str), quint32(123));

        QJSValue str2 = eng.toScriptValue(QString::fromLatin1("NaN"));
        QCOMPARE(str2.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(str2), quint32(0));

        QJSValue str3 = eng.toScriptValue(QString::fromLatin1("Infinity"));
        QCOMPARE(str3.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(str3), quint32(0));

        QJSValue str3_2 = eng.toScriptValue(QString::fromLatin1("-Infinity"));
        QCOMPARE(str3_2.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(str3_2), quint32(0));

        QJSValue str4 = eng.toScriptValue(QString::fromLatin1("0.5"));
        QCOMPARE(str4.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(str4), quint32(0));

        QJSValue str5 = eng.toScriptValue(QString::fromLatin1("123.5"));
        QCOMPARE(str5.toUInt(), quint32(123));
        QCOMPARE(qjsvalue_cast<quint32>(str5), quint32(123));

        QJSValue str6 = eng.toScriptValue(QString::fromLatin1("-456.5"));
        QCOMPARE(str6.toUInt(), quint32(-456));
        QCOMPARE(qjsvalue_cast<quint32>(str6), quint32(-456));
    }
    // V2 constructors
    {
        QJSValue zer0 = QJSValue(0.0);
        QCOMPARE(zer0.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(zer0), quint32(0));

        QJSValue number = QJSValue(123.0);
        QCOMPARE(number.toUInt(), quint32(123));
        QCOMPARE(qjsvalue_cast<quint32>(number), quint32(123));

        QJSValue number2 = QJSValue(qQNaN());
        QCOMPARE(number2.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(number2), quint32(0));

        QJSValue number3 = QJSValue(+qInf());
        QCOMPARE(number3.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(number3), quint32(0));

        QJSValue number3_2 = QJSValue(-qInf());
        QCOMPARE(number3_2.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(number3_2), quint32(0));

        QJSValue number4 = QJSValue(0.5);
        QCOMPARE(number4.toUInt(), quint32(0));

        QJSValue number5 = QJSValue(123.5);
        QCOMPARE(number5.toUInt(), quint32(123));

        QJSValue number6 = QJSValue(-456.5);
        QCOMPARE(number6.toUInt(), quint32(-456));
        QCOMPARE(qjsvalue_cast<quint32>(number6), quint32(-456));

        QJSValue number7 = QJSValue(0x43211234);
        QCOMPARE(number7.toUInt(), quint32(0x43211234));

        QJSValue str = QJSValue(QLatin1String("123.0"));
        QCOMPARE(str.toUInt(), quint32(123));
        QCOMPARE(qjsvalue_cast<quint32>(str), quint32(123));

        QJSValue str2 = QJSValue(QLatin1String("NaN"));
        QCOMPARE(str2.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(str2), quint32(0));

        QJSValue str3 = QJSValue(QLatin1String("Infinity"));
        QCOMPARE(str3.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(str3), quint32(0));

        QJSValue str3_2 = QJSValue(QLatin1String("-Infinity"));
        QCOMPARE(str3_2.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(str3_2), quint32(0));

        QJSValue str4 = QJSValue(QLatin1String("0.5"));
        QCOMPARE(str4.toUInt(), quint32(0));
        QCOMPARE(qjsvalue_cast<quint32>(str4), quint32(0));

        QJSValue str5 = QJSValue(QLatin1String("123.5"));
        QCOMPARE(str5.toUInt(), quint32(123));
        QCOMPARE(qjsvalue_cast<quint32>(str5), quint32(123));

        QJSValue str6 = QJSValue(QLatin1String("-456.5"));
        QCOMPARE(str6.toUInt(), quint32(-456));
        QCOMPARE(qjsvalue_cast<quint32>(str6), quint32(-456));
    }

    QJSValue inv;
    QCOMPARE(inv.toUInt(), quint32(0));
    QCOMPARE(qjsvalue_cast<quint32>(inv), quint32(0));
}

#if defined Q_CC_MSVC && _MSC_VER < 1300
Q_DECLARE_METATYPE(QVariant)
#endif

void tst_QJSValue::toVariant()
{
    QJSEngine eng;
    QObject temp;

    QJSValue undefined = eng.toScriptValue(QVariant());
    QCOMPARE(undefined.toVariant(), QVariant());
    QCOMPARE(qjsvalue_cast<QVariant>(undefined), QVariant());

    QJSValue null = eng.evaluate("null");
    QCOMPARE(null.toVariant(), QVariant::fromValue(nullptr));
    QCOMPARE(qjsvalue_cast<QVariant>(null), QVariant::fromValue(nullptr));

    {
        QJSValue number = eng.toScriptValue(123.0);
        QCOMPARE(number.toVariant(), QVariant(123.0));
        QCOMPARE(qjsvalue_cast<QVariant>(number), QVariant(123.0));

        QJSValue intNumber = eng.toScriptValue((qint32)123);
        QCOMPARE(intNumber.toVariant().type(), QVariant((qint32)123).type());
        QCOMPARE((qjsvalue_cast<QVariant>(number)).type(), QVariant((qint32)123).type());

        QJSValue falskt = eng.toScriptValue(false);
        QCOMPARE(falskt.toVariant(), QVariant(false));
        QCOMPARE(qjsvalue_cast<QVariant>(falskt), QVariant(false));

        QJSValue sant = eng.toScriptValue(true);
        QCOMPARE(sant.toVariant(), QVariant(true));
        QCOMPARE(qjsvalue_cast<QVariant>(sant), QVariant(true));

        QJSValue str = eng.toScriptValue(QString("ciao"));
        QCOMPARE(str.toVariant(), QVariant(QString("ciao")));
        QCOMPARE(qjsvalue_cast<QVariant>(str), QVariant(QString("ciao")));
    }

    QJSValue object = eng.newObject();
    QCOMPARE(object.toVariant(), QVariant(QVariantMap()));

    QJSValue qobject = eng.newQObject(&temp);
    {
        QVariant var = qobject.toVariant();
        QCOMPARE(var.userType(), int(QMetaType::QObjectStar));
        QCOMPARE(qvariant_cast<QObject*>(var), (QObject *)&temp);
    }

    {
        QDateTime dateTime = QDateTime(QDate(1980, 10, 4));
        QJSValue dateObject = eng.toScriptValue(dateTime);
        QVariant var = dateObject.toVariant();
        QCOMPARE(var, QVariant(dateTime));
    }

    {
        QRegExp rx = QRegExp("[0-9a-z]+", Qt::CaseSensitive, QRegExp::RegExp2);
        QJSValue rxObject = eng.toScriptValue(rx);
        QVERIFY(rxObject.isRegExp());
        QVariant var = rxObject.toVariant();
        QCOMPARE(var, QVariant(rx));
    }

    QJSValue inv;
    QCOMPARE(inv.toVariant(), QVariant());
    QCOMPARE(qjsvalue_cast<QVariant>(inv), QVariant());

    // V2 constructors
    {
        QJSValue number = QJSValue(123.0);
        QCOMPARE(number.toVariant(), QVariant(123.0));
        QCOMPARE(qjsvalue_cast<QVariant>(number), QVariant(123.0));

        QJSValue falskt = QJSValue(false);
        QCOMPARE(falskt.toVariant(), QVariant(false));
        QCOMPARE(qjsvalue_cast<QVariant>(falskt), QVariant(false));

        QJSValue sant = QJSValue(true);
        QCOMPARE(sant.toVariant(), QVariant(true));
        QCOMPARE(qjsvalue_cast<QVariant>(sant), QVariant(true));

        QJSValue str = QJSValue(QString("ciao"));
        QCOMPARE(str.toVariant(), QVariant(QString("ciao")));
        QCOMPARE(qjsvalue_cast<QVariant>(str), QVariant(QString("ciao")));

        QJSValue undef = QJSValue(QJSValue::UndefinedValue);
        QCOMPARE(undef.toVariant(), QVariant());
        QCOMPARE(qjsvalue_cast<QVariant>(undef), QVariant());

        QJSValue nil = QJSValue(QJSValue::NullValue);
        QCOMPARE(nil.toVariant(), QVariant::fromValue(nullptr));
        QCOMPARE(qjsvalue_cast<QVariant>(nil), QVariant::fromValue(nullptr));
    }

    // array
    {
        QVariantList listIn;
        listIn << 123 << "hello";
        QJSValue array = eng.toScriptValue(listIn);
        QVERIFY(array.isArray());
        QCOMPARE(array.property("length").toInt(), 2);
        QVariant ret = array.toVariant();
        QCOMPARE(ret.type(), QVariant::List);
        QVariantList listOut = ret.toList();
        QCOMPARE(listOut.size(), listIn.size());
        for (int i = 0; i < listIn.size(); ++i)
            QCOMPARE(listOut.at(i), listIn.at(i));
        // round-trip conversion
        QJSValue array2 = eng.toScriptValue(ret);
        QVERIFY(array2.isArray());
        QCOMPARE(array2.property("length").toInt(), array.property("length").toInt());
        for (int i = 0; i < array.property("length").toInt(); ++i)
            QVERIFY(array2.property(i).strictlyEquals(array.property(i)));
    }

}

void tst_QJSValue::toQObject_nonQObject_data()
{
    newEngine();
    QTest::addColumn<QJSValue>("value");

    QTest::newRow("invalid") << QJSValue();
    QTest::newRow("bool(false)") << QJSValue(false);
    QTest::newRow("bool(true)") << QJSValue(true);
    QTest::newRow("int") << QJSValue(123);
    QTest::newRow("string") << QJSValue(QString::fromLatin1("ciao"));
    QTest::newRow("undefined") << QJSValue(QJSValue::UndefinedValue);
    QTest::newRow("null") << QJSValue(QJSValue::NullValue);

    QTest::newRow("bool bound(false)") << engine->toScriptValue(false);
    QTest::newRow("bool bound(true)") << engine->toScriptValue(true);
    QTest::newRow("int bound") << engine->toScriptValue(123);
    QTest::newRow("string bound") << engine->toScriptValue(QString::fromLatin1("ciao"));
    QTest::newRow("undefined bound") << engine->toScriptValue(QVariant());
    QTest::newRow("null bound") << engine->evaluate("null");
    QTest::newRow("object") << engine->newObject();
    QTest::newRow("array") << engine->newArray();
    QTest::newRow("date") << engine->evaluate("new Date(124)");
    QTest::newRow("variant(12345)") << engine->toScriptValue(QVariant(12345));
    QTest::newRow("variant((QObject*)0)") << engine->toScriptValue(qVariantFromValue((QObject*)0));
    QTest::newRow("newQObject(0)") << engine->newQObject(0);
}


void tst_QJSValue::toQObject_nonQObject()
{
    QFETCH(QJSValue, value);
    QCOMPARE(value.toQObject(), (QObject *)0);
    QCOMPARE(qjsvalue_cast<QObject*>(value), (QObject *)0);
}

// unfortunately, this is necessary in order to do qscriptvalue_cast<QPushButton*>(...)
Q_DECLARE_METATYPE(QPushButton*);

void tst_QJSValue::toQObject()
{
    QJSEngine eng;
    QObject temp;

    QJSValue qobject = eng.newQObject(&temp);
    QCOMPARE(qobject.toQObject(), (QObject *)&temp);
    QCOMPARE(qjsvalue_cast<QObject*>(qobject), (QObject *)&temp);
    QCOMPARE(qjsvalue_cast<QWidget*>(qobject), (QWidget *)0);

    QWidget widget;
    QJSValue qwidget = eng.newQObject(&widget);
    QCOMPARE(qwidget.toQObject(), (QObject *)&widget);
    QCOMPARE(qjsvalue_cast<QObject*>(qwidget), (QObject *)&widget);
    QCOMPARE(qjsvalue_cast<QWidget*>(qwidget), &widget);

    QPushButton button;
    QJSValue qbutton = eng.newQObject(&button);
    QCOMPARE(qbutton.toQObject(), (QObject *)&button);
    QCOMPARE(qjsvalue_cast<QObject*>(qbutton), (QObject *)&button);
    QCOMPARE(qjsvalue_cast<QWidget*>(qbutton), (QWidget *)&button);
    QCOMPARE(qjsvalue_cast<QPushButton*>(qbutton), &button);
}

void tst_QJSValue::toDateTime()
{
    QJSEngine eng;
    QDateTime dt = eng.evaluate("new Date(0)").toDateTime();
    QVERIFY(dt.isValid());
    QCOMPARE(dt.timeSpec(), Qt::LocalTime);
    QCOMPARE(dt.toUTC(), QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0), Qt::UTC));

    QVERIFY(!eng.evaluate("[]").toDateTime().isValid());
    QVERIFY(!eng.evaluate("{}").toDateTime().isValid());
    QVERIFY(!eng.globalObject().toDateTime().isValid());
    QVERIFY(!QJSValue().toDateTime().isValid());
    QVERIFY(!QJSValue(123).toDateTime().isValid());
    QVERIFY(!QJSValue(false).toDateTime().isValid());
    QVERIFY(!eng.evaluate("null").toDateTime().isValid());
    QVERIFY(!eng.toScriptValue(QVariant()).toDateTime().isValid());
}

void tst_QJSValue::toRegExp()
{
    QJSEngine eng;
    {
        QRegExp rx = qjsvalue_cast<QRegExp>(eng.evaluate("/foo/"));
        QVERIFY(rx.isValid());
        QCOMPARE(rx.patternSyntax(), QRegExp::RegExp2);
        QCOMPARE(rx.pattern(), QString::fromLatin1("foo"));
        QCOMPARE(rx.caseSensitivity(), Qt::CaseSensitive);
        QVERIFY(!rx.isMinimal());
    }
    {
        QRegExp rx = qjsvalue_cast<QRegExp>(eng.evaluate("/bar/gi"));
        QVERIFY(rx.isValid());
        QCOMPARE(rx.patternSyntax(), QRegExp::RegExp2);
        QCOMPARE(rx.pattern(), QString::fromLatin1("bar"));
        QCOMPARE(rx.caseSensitivity(), Qt::CaseInsensitive);
        QVERIFY(!rx.isMinimal());
    }

    QVERIFY(qjsvalue_cast<QRegExp>(eng.evaluate("[]")).isEmpty());
    QVERIFY(qjsvalue_cast<QRegExp>(eng.evaluate("{}")).isEmpty());
    QVERIFY(qjsvalue_cast<QRegExp>(eng.globalObject()).isEmpty());
    QVERIFY(qjsvalue_cast<QRegExp>(QJSValue()).isEmpty());
    QVERIFY(qjsvalue_cast<QRegExp>(QJSValue(123)).isEmpty());
    QVERIFY(qjsvalue_cast<QRegExp>(QJSValue(false)).isEmpty());
    QVERIFY(qjsvalue_cast<QRegExp>(eng.evaluate("null")).isEmpty());
    QVERIFY(qjsvalue_cast<QRegExp>(eng.toScriptValue(QVariant())).isEmpty());
}

void tst_QJSValue::isArray_data()
{
    newEngine();

    QTest::addColumn<QJSValue>("value");
    QTest::addColumn<bool>("array");

    QTest::newRow("[]") << engine->evaluate("[]") << true;
    QTest::newRow("{}") << engine->evaluate("{}") << false;
    QTest::newRow("globalObject") << engine->globalObject() << false;
    QTest::newRow("invalid") << QJSValue() << false;
    QTest::newRow("number") << QJSValue(123) << false;
    QTest::newRow("bool") << QJSValue(false) << false;
    QTest::newRow("null") << engine->evaluate("null") << false;
    QTest::newRow("undefined") << engine->toScriptValue(QVariant()) << false;
}

void tst_QJSValue::isArray()
{
    QFETCH(QJSValue, value);
    QFETCH(bool, array);

    QCOMPARE(value.isArray(), array);
}

void tst_QJSValue::isDate_data()
{
    newEngine();

    QTest::addColumn<QJSValue>("value");
    QTest::addColumn<bool>("date");

    QTest::newRow("date") << engine->evaluate("new Date()") << true;
    QTest::newRow("[]") << engine->evaluate("[]") << false;
    QTest::newRow("{}") << engine->evaluate("{}") << false;
    QTest::newRow("globalObject") << engine->globalObject() << false;
    QTest::newRow("invalid") << QJSValue() << false;
    QTest::newRow("number") << QJSValue(123) << false;
    QTest::newRow("bool") << QJSValue(false) << false;
    QTest::newRow("null") << engine->evaluate("null") << false;
    QTest::newRow("undefined") << engine->toScriptValue(QVariant()) << false;
}

void tst_QJSValue::isDate()
{
    QFETCH(QJSValue, value);
    QFETCH(bool, date);

    QCOMPARE(value.isDate(), date);
}

void tst_QJSValue::isError_propertiesOfGlobalObject()
{
    QStringList errors;
    errors << "Error"
           << "EvalError"
           << "RangeError"
           << "ReferenceError"
           << "SyntaxError"
           << "TypeError"
           << "URIError";
    QJSEngine eng;
    for (int i = 0; i < errors.size(); ++i) {
        QJSValue ctor = eng.globalObject().property(errors.at(i));
        QVERIFY(ctor.isCallable());
        QVERIFY(ctor.property("prototype").isError());
    }
}

void tst_QJSValue::isError_data()
{
    newEngine();

    QTest::addColumn<QJSValue>("value");
    QTest::addColumn<bool>("error");

    QTest::newRow("syntax error") << engine->evaluate("%fsdg's") << true;
    QTest::newRow("[]") << engine->evaluate("[]") << false;
    QTest::newRow("{}") << engine->evaluate("{}") << false;
    QTest::newRow("globalObject") << engine->globalObject() << false;
    QTest::newRow("invalid") << QJSValue() << false;
    QTest::newRow("number") << QJSValue(123) << false;
    QTest::newRow("bool") << QJSValue(false) << false;
    QTest::newRow("null") << engine->evaluate("null") << false;
    QTest::newRow("undefined") << engine->toScriptValue(QVariant()) << false;
    QTest::newRow("newObject") << engine->newObject() << false;
    QTest::newRow("new Object") << engine->evaluate("new Object()") << false;
}

void tst_QJSValue::isError()
{
    QFETCH(QJSValue, value);
    QFETCH(bool, error);

    QCOMPARE(value.isError(), error);
}

void tst_QJSValue::isRegExp_data()
{
    newEngine();

    QTest::addColumn<QJSValue>("value");
    QTest::addColumn<bool>("regexp");

    QTest::newRow("/foo/") << engine->evaluate("/foo/") << true;
    QTest::newRow("[]") << engine->evaluate("[]") << false;
    QTest::newRow("{}") << engine->evaluate("{}") << false;
    QTest::newRow("globalObject") << engine->globalObject() << false;
    QTest::newRow("invalid") << QJSValue() << false;
    QTest::newRow("number") << QJSValue(123) << false;
    QTest::newRow("bool") << QJSValue(false) << false;
    QTest::newRow("null") << engine->evaluate("null") << false;
    QTest::newRow("undefined") << engine->toScriptValue(QVariant()) << false;
}

void tst_QJSValue::isRegExp()
{
    QFETCH(QJSValue, value);
    QFETCH(bool, regexp);

    QCOMPARE(value.isRegExp(), regexp);
}

void tst_QJSValue::hasProperty_basic()
{
    QJSEngine eng;
    QJSValue obj = eng.newObject();
    QVERIFY(obj.hasProperty("hasOwnProperty")); // inherited from Object.prototype
    QVERIFY(!obj.hasOwnProperty("hasOwnProperty"));

    QVERIFY(!obj.hasProperty("foo"));
    QVERIFY(!obj.hasOwnProperty("foo"));
    obj.setProperty("foo", 123);
    QVERIFY(obj.hasProperty("foo"));
    QVERIFY(obj.hasOwnProperty("foo"));

    QVERIFY(!obj.hasProperty("bar"));
    QVERIFY(!obj.hasOwnProperty("bar"));
}

void tst_QJSValue::hasProperty_globalObject()
{
    QJSEngine eng;
    QJSValue global = eng.globalObject();
    QVERIFY(global.hasProperty("Math"));
    QVERIFY(global.hasOwnProperty("Math"));
    QVERIFY(!global.hasProperty("NoSuchStandardProperty"));
    QVERIFY(!global.hasOwnProperty("NoSuchStandardProperty"));

    QVERIFY(!global.hasProperty("foo"));
    QVERIFY(!global.hasOwnProperty("foo"));
    global.setProperty("foo", 123);
    QVERIFY(global.hasProperty("foo"));
    QVERIFY(global.hasOwnProperty("foo"));
}

void tst_QJSValue::hasProperty_changePrototype()
{
    QJSEngine eng;
    QJSValue obj = eng.newObject();
    QJSValue proto = eng.newObject();
    obj.setPrototype(proto);

    QVERIFY(!obj.hasProperty("foo"));
    QVERIFY(!obj.hasOwnProperty("foo"));
    proto.setProperty("foo", 123);
    QVERIFY(obj.hasProperty("foo"));
    QVERIFY(!obj.hasOwnProperty("foo"));

    obj.setProperty("foo", 456); // override prototype property
    QVERIFY(obj.hasProperty("foo"));
    QVERIFY(obj.hasOwnProperty("foo"));
}

void tst_QJSValue::hasProperty_QTBUG56830_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("lookup");

    QTest::newRow("bugreport-1") << QStringLiteral("240000000000") << QStringLiteral("3776798720");
    QTest::newRow("bugreport-2") << QStringLiteral("240000000001") << QStringLiteral("3776798721");
    QTest::newRow("biggest-ok-before-bug") << QStringLiteral("238609294221") << QStringLiteral("2386092941");
    QTest::newRow("smallest-bugged") << QStringLiteral("238609294222") << QStringLiteral("2386092942");
    QTest::newRow("biggest-bugged") << QStringLiteral("249108103166") << QStringLiteral("12884901886");
    QTest::newRow("smallest-ok-after-bug") << QStringLiteral("249108103167") << QStringLiteral("12884901887");
}

void tst_QJSValue::hasProperty_QTBUG56830()
{
    QFETCH(QString, key);
    QFETCH(QString, lookup);

    QJSEngine eng;
    const QJSValue value(42);

    QJSValue obj = eng.newObject();
    obj.setProperty(key, value);
    QVERIFY(obj.hasProperty(key));
    QVERIFY(!obj.hasProperty(lookup));
}

void tst_QJSValue::deleteProperty_basic()
{
    QJSEngine eng;
    QJSValue obj = eng.newObject();
    // deleteProperty() behavior matches JS delete operator
    QVERIFY(obj.deleteProperty("foo"));

    obj.setProperty("foo", 123);
    QVERIFY(obj.deleteProperty("foo"));
    QVERIFY(!obj.hasOwnProperty("foo"));
}

void tst_QJSValue::deleteProperty_globalObject()
{
    QJSEngine eng;
    QJSValue global = eng.globalObject();
    // deleteProperty() behavior matches JS delete operator
    QVERIFY(global.deleteProperty("foo"));

    global.setProperty("foo", 123);
    QVERIFY(global.deleteProperty("foo"));
    QVERIFY(!global.hasProperty("foo"));

    QVERIFY(global.deleteProperty("Math"));
    QVERIFY(!global.hasProperty("Math"));

    QVERIFY(!global.deleteProperty("NaN")); // read-only
    QVERIFY(global.hasProperty("NaN"));
}

void tst_QJSValue::deleteProperty_inPrototype()
{
    QJSEngine eng;
    QJSValue obj = eng.newObject();
    QJSValue proto = eng.newObject();
    obj.setPrototype(proto);

    proto.setProperty("foo", 123);
    QVERIFY(obj.hasProperty("foo"));
    // deleteProperty() behavior matches JS delete operator
    QVERIFY(obj.deleteProperty("foo"));
    QVERIFY(obj.hasProperty("foo"));
}

void tst_QJSValue::getSetProperty_HooliganTask162051()
{
    QJSEngine eng;
    // task 162051 -- detecting whether the property is an array index or not
    QVERIFY(eng.evaluate("a = []; a['00'] = 123; a['00']").strictlyEquals(eng.toScriptValue(123)));
    QVERIFY(eng.evaluate("a.length").strictlyEquals(eng.toScriptValue(0)));
    QVERIFY(eng.evaluate("a.hasOwnProperty('00')").strictlyEquals(eng.toScriptValue(true)));
    QVERIFY(eng.evaluate("a.hasOwnProperty('0')").strictlyEquals(eng.toScriptValue(false)));
    QVERIFY(eng.evaluate("a[0]").isUndefined());
    QVERIFY(eng.evaluate("a[0.5] = 456; a[0.5]").strictlyEquals(eng.toScriptValue(456)));
    QVERIFY(eng.evaluate("a.length").strictlyEquals(eng.toScriptValue(0)));
    QVERIFY(eng.evaluate("a.hasOwnProperty('0.5')").strictlyEquals(eng.toScriptValue(true)));
    QVERIFY(eng.evaluate("a[0]").isUndefined());
    QVERIFY(eng.evaluate("a[0] = 789; a[0]").strictlyEquals(eng.toScriptValue(789)));
    QVERIFY(eng.evaluate("a.length").strictlyEquals(eng.toScriptValue(1)));
}

void tst_QJSValue::getSetProperty_HooliganTask183072()
{
    QJSEngine eng;
    // task 183072 -- 0x800000000 is not an array index
    eng.evaluate("a = []; a[0x800000000] = 123");
    QVERIFY(eng.evaluate("a.length").strictlyEquals(eng.toScriptValue(0)));
    QVERIFY(eng.evaluate("a[0]").isUndefined());
    QVERIFY(eng.evaluate("a[0x800000000]").strictlyEquals(eng.toScriptValue(123)));
}

void tst_QJSValue::getSetProperty_propertyRemoval()
{
    QJSEngine eng;
    QJSValue object = eng.newObject();
    QJSValue str = eng.toScriptValue(QString::fromLatin1("bar"));
    QJSValue num = eng.toScriptValue(123.0);

    object.setProperty("foo", num);
    QCOMPARE(object.property("foo").strictlyEquals(num), true);
    object.setProperty("bar", str);
    QCOMPARE(object.property("bar").strictlyEquals(str), true);
    QVERIFY(object.deleteProperty("foo"));
    QVERIFY(!object.hasOwnProperty("foo"));
    QCOMPARE(object.property("bar").strictlyEquals(str), true);
    object.setProperty("foo", num);
    QCOMPARE(object.property("foo").strictlyEquals(num), true);
    QCOMPARE(object.property("bar").strictlyEquals(str), true);
    QVERIFY(object.deleteProperty("bar"));
    QVERIFY(!object.hasOwnProperty("bar"));
    QCOMPARE(object.property("foo").strictlyEquals(num), true);
    QVERIFY(object.deleteProperty("foo"));
    QVERIFY(!object.hasOwnProperty("foo"));

    eng.globalObject().setProperty("object3", object);
    QCOMPARE(eng.evaluate("object3.hasOwnProperty('foo')")
             .strictlyEquals(eng.toScriptValue(false)), true);
    object.setProperty("foo", num);
    QCOMPARE(eng.evaluate("object3.hasOwnProperty('foo')")
             .strictlyEquals(eng.toScriptValue(true)), true);
    QVERIFY(eng.globalObject().deleteProperty("object3"));
    QCOMPARE(eng.evaluate("this.hasOwnProperty('object3')")
             .strictlyEquals(eng.toScriptValue(false)), true);
}

void tst_QJSValue::getSetProperty_resolveMode()
{
    // test ResolveMode
    QJSEngine eng;
    QJSValue object = eng.newObject();
    QJSValue prototype = eng.newObject();
    object.setPrototype(prototype);
    QJSValue num2 = eng.toScriptValue(456.0);
    prototype.setProperty("propertyInPrototype", num2);
    // default is ResolvePrototype
    QCOMPARE(object.property("propertyInPrototype")
             .strictlyEquals(num2), true);
#if 0 // FIXME: ResolveFlags removed from API
    QCOMPARE(object.property("propertyInPrototype", QJSValue::ResolvePrototype)
             .strictlyEquals(num2), true);
    QCOMPARE(object.property("propertyInPrototype", QJSValue::ResolveLocal)
             .isValid(), false);
    QCOMPARE(object.property("propertyInPrototype", QJSValue::ResolveScope)
             .strictlyEquals(num2), false);
    QCOMPARE(object.property("propertyInPrototype", QJSValue::ResolveFull)
             .strictlyEquals(num2), true);
#endif
}

void tst_QJSValue::getSetProperty_twoEngines()
{
    QJSEngine engine;
    QJSValue object = engine.newObject();

    QJSEngine otherEngine;
    QJSValue otherNum = otherEngine.toScriptValue(123);
    QTest::ignoreMessage(QtWarningMsg, "QJSValue::setProperty(oof) failed: cannot set value created in a different engine");
    object.setProperty("oof", otherNum);
    QVERIFY(!object.hasOwnProperty("oof"));
    QVERIFY(object.property("oof").isUndefined());
}

void tst_QJSValue::getSetProperty_gettersAndSettersThrowErrorJS()
{
    // getter/setter that throws an error (from js function)
    QJSEngine eng;
    QJSValue str = eng.toScriptValue(QString::fromLatin1("bar"));

    eng.evaluate("o = new Object; "
                 "o.__defineGetter__('foo', function() { throw new Error('get foo') }); "
                 "o.__defineSetter__('foo', function() { throw new Error('set foo') }); ");
    QJSValue object = eng.evaluate("o");
    QVERIFY(!object.isError());
    QJSValue ret = object.property("foo");
    QVERIFY(ret.isError());
    QCOMPARE(ret.toString(), QLatin1String("Error: get foo"));
    QVERIFY(!eng.evaluate("Object").isError()); // clear exception state...
    object.setProperty("foo", str);
// ### No way to check whether setProperty() threw an exception
//    QVERIFY(eng.hasUncaughtException());
//    QCOMPARE(eng.uncaughtException().toString(), QLatin1String("Error: set foo"));
}

void tst_QJSValue::getSetProperty_array()
{
    QJSEngine eng;
    QJSValue str = eng.toScriptValue(QString::fromLatin1("bar"));
    QJSValue num = eng.toScriptValue(123.0);
    QJSValue array = eng.newArray();

    QVERIFY(array.isArray());
    array.setProperty(0, num);
    QCOMPARE(array.property(0).toNumber(), num.toNumber());
    QCOMPARE(array.property("0").toNumber(), num.toNumber());
    QCOMPARE(array.property("length").toUInt(), quint32(1));
    array.setProperty(1, str);
    QCOMPARE(array.property(1).toString(), str.toString());
    QCOMPARE(array.property("1").toString(), str.toString());
    QCOMPARE(array.property("length").toUInt(), quint32(2));
    array.setProperty("length", eng.toScriptValue(1));
    QCOMPARE(array.property("length").toUInt(), quint32(1));
    QVERIFY(array.property(1).isUndefined());
}

void tst_QJSValue::getSetProperty()
{
    QJSEngine eng;

    QJSValue object = eng.newObject();

    QJSValue str = eng.toScriptValue(QString::fromLatin1("bar"));
    object.setProperty("foo", str);
    QCOMPARE(object.property("foo").toString(), str.toString());

    QJSValue num = eng.toScriptValue(123.0);
    object.setProperty("baz", num);
    QCOMPARE(object.property("baz").toNumber(), num.toNumber());

    QJSValue strstr = QJSValue("bar");
    QCOMPARE(strstr.engine(), (QJSEngine *)0);
    object.setProperty("foo", strstr);
    QCOMPARE(object.property("foo").toString(), strstr.toString());
    QCOMPARE(strstr.engine(), &eng); // the value has been bound to the engine

    QJSValue numnum = QJSValue(123.0);
    object.setProperty("baz", numnum);
    QCOMPARE(object.property("baz").toNumber(), numnum.toNumber());

    QJSValue inv;
    inv.setProperty("foo", num);
    QCOMPARE(inv.property("foo").isUndefined(), true);

    eng.globalObject().setProperty("object", object);

    // Setting index property on non-Array
    object.setProperty(13, num);
    QVERIFY(object.property(13).equals(num));
}

void tst_QJSValue::getSetPrototype_cyclicPrototype()
{
    QJSEngine eng;
    QJSValue prototype = eng.newObject();
    QJSValue object = eng.newObject();
    object.setPrototype(prototype);

    QJSValue previousPrototype = prototype.prototype();
    QTest::ignoreMessage(QtWarningMsg, "QJSValue::setPrototype() failed: cyclic prototype value");
    prototype.setPrototype(prototype);
    QCOMPARE(prototype.prototype().strictlyEquals(previousPrototype), true);

    object.setPrototype(prototype);
    QTest::ignoreMessage(QtWarningMsg, "QJSValue::setPrototype() failed: cyclic prototype value");
    prototype.setPrototype(object);
    QCOMPARE(prototype.prototype().strictlyEquals(previousPrototype), true);

}

void tst_QJSValue::getSetPrototype_evalCyclicPrototype()
{
    QJSEngine eng;
    QJSValue ret = eng.evaluate("o = { }; p = { }; o.__proto__ = p; p.__proto__ = o");
    QCOMPARE(ret.isError(), true);
    QCOMPARE(ret.toString(), QLatin1String("TypeError: Cyclic __proto__ value"));
}

void tst_QJSValue::getSetPrototype_eval()
{
    QJSEngine eng;
    QJSValue ret = eng.evaluate("p = { }; p.__proto__ = { }");
    QCOMPARE(ret.isError(), false);
}

void tst_QJSValue::getSetPrototype_invalidPrototype()
{
    QJSEngine eng;
    QJSValue inv;
    QJSValue object = eng.newObject();
    QJSValue proto = object.prototype();
    QVERIFY(object.prototype().strictlyEquals(proto));
    inv.setPrototype(object);
    QVERIFY(inv.prototype().isUndefined());
    object.setPrototype(inv);
    QVERIFY(object.prototype().strictlyEquals(proto));
}

void tst_QJSValue::getSetPrototype_twoEngines()
{
    QJSEngine eng;
    QJSValue prototype = eng.newObject();
    QJSValue object = eng.newObject();
    object.setPrototype(prototype);
    QJSEngine otherEngine;
    QJSValue newPrototype = otherEngine.newObject();
    QTest::ignoreMessage(QtWarningMsg, "QJSValue::setPrototype() failed: cannot set a prototype created in a different engine");
    object.setPrototype(newPrototype);
    QCOMPARE(object.prototype().strictlyEquals(prototype), true);

}

void tst_QJSValue::getSetPrototype_null()
{
    QJSEngine eng;
    QJSValue object = eng.newObject();
    object.setPrototype(QJSValue(QJSValue::NullValue));
    QVERIFY(object.prototype().isNull());

    QJSValue newProto = eng.newObject();
    object.setPrototype(newProto);
    QVERIFY(object.prototype().equals(newProto));

    object.setPrototype(eng.evaluate("null"));
    QVERIFY(object.prototype().isNull());
}

void tst_QJSValue::getSetPrototype_notObjectOrNull()
{
    QJSEngine eng;
    QJSValue object = eng.newObject();
    QJSValue originalProto = object.prototype();

    // bool
    object.setPrototype(true);
    QVERIFY(object.prototype().equals(originalProto));
    object.setPrototype(eng.toScriptValue(true));
    QVERIFY(object.prototype().equals(originalProto));

    // number
    object.setPrototype(123);
    QVERIFY(object.prototype().equals(originalProto));
    object.setPrototype(eng.toScriptValue(123));
    QVERIFY(object.prototype().equals(originalProto));

    // string
    object.setPrototype("foo");
    QVERIFY(object.prototype().equals(originalProto));
    object.setPrototype(eng.toScriptValue(QString::fromLatin1("foo")));
    QVERIFY(object.prototype().equals(originalProto));

    // undefined
    object.setPrototype(QJSValue(QJSValue::UndefinedValue));
    QVERIFY(object.prototype().equals(originalProto));
    object.setPrototype(eng.toScriptValue(QVariant()));
    QVERIFY(object.prototype().equals(originalProto));
}

void tst_QJSValue::getSetPrototype()
{
    QJSEngine eng;
    QJSValue prototype = eng.newObject();
    QJSValue object = eng.newObject();
    object.setPrototype(prototype);
    QCOMPARE(object.prototype().strictlyEquals(prototype), true);
}

void tst_QJSValue::call_function()
{
    QJSEngine eng;
    QJSValue fun = eng.evaluate("(function() { return 1; })");
    QVERIFY(fun.isCallable());
    QJSValue result = fun.call();
    QVERIFY(result.isNumber());
    QCOMPARE(result.toInt(), 1);
}

void tst_QJSValue::call_object()
{
    QJSEngine eng;
    QJSValue Object = eng.evaluate("Object");
    QCOMPARE(Object.isCallable(), true);
    QJSValue result = Object.callWithInstance(Object);
    QCOMPARE(result.isObject(), true);
}

void tst_QJSValue::call_newObjects()
{
    QJSEngine eng;
    // test that call() doesn't construct new objects
    QJSValue Number = eng.evaluate("Number");
    QJSValue Object = eng.evaluate("Object");
    QCOMPARE(Object.isCallable(), true);
    QJSValueList args;
    args << eng.toScriptValue(123);
    QJSValue result = Number.callWithInstance(Object, args);
    QCOMPARE(result.strictlyEquals(args.at(0)), true);
}

void tst_QJSValue::call_this()
{
    QJSEngine eng;
    // test that correct "this" object is used
    QJSValue fun = eng.evaluate("(function() { return this; })");
    QCOMPARE(fun.isCallable(), true);

    QJSValue numberObject = eng.evaluate("new Number(123)");
    QJSValue result = fun.callWithInstance(numberObject);
    QCOMPARE(result.isObject(), true);
    QCOMPARE(result.toNumber(), 123.0);
}

void tst_QJSValue::call_arguments()
{
    QJSEngine eng;
    // test that correct arguments are passed

    QJSValue fun = eng.evaluate("(function() { return arguments[0]; })");
    QCOMPARE(fun.isCallable(), true);
    {
        QJSValue result = fun.callWithInstance(eng.toScriptValue(QVariant()));
        QCOMPARE(result.isUndefined(), true);
    }
    {
        QJSValueList args;
        args << eng.toScriptValue(123.0);
        QJSValue result = fun.callWithInstance(eng.toScriptValue(QVariant()), args);
        QCOMPARE(result.isNumber(), true);
        QCOMPARE(result.toNumber(), 123.0);
    }
    // V2 constructors
    {
        QJSValueList args;
        args << QJSValue(123.0);
        QJSValue result = fun.callWithInstance(eng.toScriptValue(QVariant()), args);
        QCOMPARE(result.isNumber(), true);
        QCOMPARE(result.toNumber(), 123.0);
    }
}

void tst_QJSValue::call()
{
    QJSEngine eng;
    {
        QJSValue fun = eng.evaluate("(function() { return arguments[1]; })");
        QCOMPARE(fun.isCallable(), true);

        {
            QJSValueList args;
            args << eng.toScriptValue(123.0) << eng.toScriptValue(456.0);
            QJSValue result = fun.callWithInstance(eng.toScriptValue(QVariant()), args);
            QCOMPARE(result.isNumber(), true);
            QCOMPARE(result.toNumber(), 456.0);
        }
    }
    {
        QJSValue fun = eng.evaluate("(function() { throw new Error('foo'); })");
        QCOMPARE(fun.isCallable(), true);
        QVERIFY(!fun.isError());

        {
            QJSValue result = fun.call();
            QCOMPARE(result.isError(), true);
        }
    }
}

void tst_QJSValue::call_twoEngines()
{
    QJSEngine eng;
    QJSValue object = eng.evaluate("Object");
    QJSEngine otherEngine;
    QJSValue fun = otherEngine.evaluate("(function() { return 1; })");
    QVERIFY(fun.isCallable());
    QTest::ignoreMessage(QtWarningMsg, "QJSValue::call() failed: "
                         "cannot call function with thisObject created in "
                         "a different engine");
    QVERIFY(fun.callWithInstance(object).isUndefined());
    QTest::ignoreMessage(QtWarningMsg, "QJSValue::call() failed: "
                         "cannot call function with argument created in "
                         "a different engine");
    QVERIFY(fun.call(QJSValueList() << eng.toScriptValue(123)).isUndefined());
    {
        QJSValue fun = eng.evaluate("Object");
        QVERIFY(fun.isCallable());
        QJSEngine eng2;
        QJSValue objectInDifferentEngine = eng2.newObject();
        QJSValueList args;
        args << objectInDifferentEngine;
        QTest::ignoreMessage(QtWarningMsg, "QJSValue::call() failed: cannot call function with argument created in a different engine");
        fun.call(args);
    }
}

void tst_QJSValue::call_nonFunction_data()
{
    newEngine();
    QTest::addColumn<QJSValue>("value");

    QTest::newRow("invalid") << QJSValue();
    QTest::newRow("bool") << QJSValue(false);
    QTest::newRow("int") << QJSValue(123);
    QTest::newRow("string") << QJSValue(QString::fromLatin1("ciao"));
    QTest::newRow("undefined") << QJSValue(QJSValue::UndefinedValue);
    QTest::newRow("null") << QJSValue(QJSValue::NullValue);

    QTest::newRow("bool bound") << engine->toScriptValue(false);
    QTest::newRow("int bound") << engine->toScriptValue(123);
    QTest::newRow("string bound") << engine->toScriptValue(QString::fromLatin1("ciao"));
    QTest::newRow("undefined bound") << engine->toScriptValue(QVariant());
    QTest::newRow("null bound") << engine->evaluate("null");
}

void tst_QJSValue::call_nonFunction()
{
    // calling things that are not functions
    QFETCH(QJSValue, value);
    QVERIFY(value.call().isUndefined());
}

void tst_QJSValue::construct_nonFunction_data()
{
    newEngine();
    QTest::addColumn<QJSValue>("value");

    QTest::newRow("invalid") << QJSValue();
    QTest::newRow("bool") << QJSValue(false);
    QTest::newRow("int") << QJSValue(123);
    QTest::newRow("string") << QJSValue(QString::fromLatin1("ciao"));
    QTest::newRow("undefined") << QJSValue(QJSValue::UndefinedValue);
    QTest::newRow("null") << QJSValue(QJSValue::NullValue);

    QTest::newRow("bool bound") << engine->toScriptValue(false);
    QTest::newRow("int bound") << engine->toScriptValue(123);
    QTest::newRow("string bound") << engine->toScriptValue(QString::fromLatin1("ciao"));
    QTest::newRow("undefined bound") << engine->toScriptValue(QVariant());
    QTest::newRow("null bound") << engine->evaluate("null");
}

void tst_QJSValue::construct_nonFunction()
{
    QFETCH(QJSValue, value);
    QVERIFY(value.callAsConstructor().isUndefined());
}

void tst_QJSValue::construct_simple()
{
    QJSEngine eng;
    QJSValue fun = eng.evaluate("(function () { this.foo = 123; })");
    QVERIFY(fun.isCallable());
    QJSValue ret = fun.callAsConstructor();
    QVERIFY(!ret.isUndefined());
    QVERIFY(ret.isObject());
    QVERIFY(ret.prototype().strictlyEquals(fun.property("prototype")));
    QCOMPARE(ret.property("foo").toInt(), 123);
}

void tst_QJSValue::construct_newObjectJS()
{
    QJSEngine eng;
    // returning a different object overrides the default-constructed one
    QJSValue fun = eng.evaluate("(function () { return { bar: 456 }; })");
    QVERIFY(fun.isCallable());
    QJSValue ret = fun.callAsConstructor();
    QVERIFY(ret.isObject());
    QVERIFY(!ret.prototype().strictlyEquals(fun.property("prototype")));
    QCOMPARE(ret.property("bar").toInt(), 456);
}

void tst_QJSValue::construct_arg()
{
    QJSEngine eng;
    QJSValue Number = eng.evaluate("Number");
    QCOMPARE(Number.isCallable(), true);
    QJSValueList args;
    args << eng.toScriptValue(123);
    QJSValue ret = Number.callAsConstructor(args);
    QCOMPARE(ret.isObject(), true);
    QCOMPARE(ret.toNumber(), args.at(0).toNumber());
}

void tst_QJSValue::construct_proto()
{
    QJSEngine eng;
    // test that internal prototype is set correctly
    QJSValue fun = eng.evaluate("(function() { return this.__proto__; })");
    QCOMPARE(fun.isCallable(), true);
    QCOMPARE(fun.property("prototype").isObject(), true);
    QJSValue ret = fun.callAsConstructor();
    QCOMPARE(fun.property("prototype").strictlyEquals(ret), true);
}

void tst_QJSValue::construct_returnInt()
{
    QJSEngine eng;
    // test that we return the new object even if a non-object value is returned from the function
    QJSValue fun = eng.evaluate("(function() { return 123; })");
    QCOMPARE(fun.isCallable(), true);
    QJSValue ret = fun.callAsConstructor();
    QCOMPARE(ret.isObject(), true);
}

void tst_QJSValue::construct_throw()
{
    QJSEngine eng;
    QJSValue fun = eng.evaluate("(function() { throw new Error('foo'); })");
    QCOMPARE(fun.isCallable(), true);
    QJSValue ret = fun.callAsConstructor();
    QCOMPARE(ret.isError(), true);
}

void tst_QJSValue::construct_twoEngines()
{
    QJSEngine engine;
    QJSEngine otherEngine;
    QJSValue ctor = engine.evaluate("(function (a, b) { this.foo = 123; })");
    QJSValue arg = otherEngine.toScriptValue(124567);
    QTest::ignoreMessage(QtWarningMsg, "QJSValue::callAsConstructor() failed: cannot construct function with argument created in a different engine");
    QVERIFY(ctor.callAsConstructor(QJSValueList() << arg).isUndefined());
    QTest::ignoreMessage(QtWarningMsg, "QJSValue::callAsConstructor() failed: cannot construct function with argument created in a different engine");
    QVERIFY(ctor.callAsConstructor(QJSValueList() << arg << otherEngine.newObject()).isUndefined());
}

void tst_QJSValue::construct_constructorThrowsPrimitive()
{
    QJSEngine eng;
    QJSValue fun = eng.evaluate("(function() { throw 123; })");
    QVERIFY(fun.isCallable());
    // construct(QJSValueList)
    {
        QJSValue ret = fun.callAsConstructor();
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toNumber(), 123.0);
        QVERIFY(!ret.isError());
    }
}

void tst_QJSValue::equals()
{
    QJSEngine eng;
    QObject temp;

    QVERIFY(QJSValue().equals(QJSValue()));

    QJSValue num = eng.toScriptValue(123);
    QCOMPARE(num.equals(eng.toScriptValue(123)), true);
    QCOMPARE(num.equals(eng.toScriptValue(321)), false);
    QCOMPARE(num.equals(eng.toScriptValue(QString::fromLatin1("123"))), true);
    QCOMPARE(num.equals(eng.toScriptValue(QString::fromLatin1("321"))), false);
    QCOMPARE(num.equals(eng.evaluate("new Number(123)")), true);
    QCOMPARE(num.equals(eng.evaluate("new Number(321)")), false);
    QCOMPARE(num.equals(eng.evaluate("new String('123')")), true);
    QCOMPARE(num.equals(eng.evaluate("new String('321')")), false);
    QVERIFY(eng.evaluate("new Number(123)").equals(num));
    QCOMPARE(num.equals(QJSValue()), false);

    QJSValue str = eng.toScriptValue(QString::fromLatin1("123"));
    QCOMPARE(str.equals(eng.toScriptValue(QString::fromLatin1("123"))), true);
    QCOMPARE(str.equals(eng.toScriptValue(QString::fromLatin1("321"))), false);
    QCOMPARE(str.equals(eng.toScriptValue(123)), true);
    QCOMPARE(str.equals(eng.toScriptValue(321)), false);
    QCOMPARE(str.equals(eng.evaluate("new String('123')")), true);
    QCOMPARE(str.equals(eng.evaluate("new String('321')")), false);
    QCOMPARE(str.equals(eng.evaluate("new Number(123)")), true);
    QCOMPARE(str.equals(eng.evaluate("new Number(321)")), false);
    QVERIFY(eng.evaluate("new String('123')").equals(str));
    QCOMPARE(str.equals(QJSValue()), false);

    QJSValue num2 = QJSValue(123);
    QCOMPARE(num2.equals(QJSValue(123)), true);
    QCOMPARE(num2.equals(QJSValue(321)), false);
    QCOMPARE(num2.equals(QJSValue("123")), true);
    QCOMPARE(num2.equals(QJSValue("321")), false);
    QCOMPARE(num2.equals(QJSValue()), false);

    QJSValue str2 = QJSValue("123");
    QCOMPARE(str2.equals(QJSValue("123")), true);
    QCOMPARE(str2.equals(QJSValue("321")), false);
    QCOMPARE(str2.equals(QJSValue(123)), true);
    QCOMPARE(str2.equals(QJSValue(321)), false);
    QCOMPARE(str2.equals(QJSValue()), false);

    QJSValue date1 = eng.toScriptValue(QDateTime(QDate(2000, 1, 1)));
    QJSValue date2 = eng.toScriptValue(QDateTime(QDate(1999, 1, 1)));
    QCOMPARE(date1.equals(date2), false);
    QCOMPARE(date1.equals(date1), true);
    QCOMPARE(date2.equals(date2), true);

    QJSValue undefined = eng.toScriptValue(QVariant());
    QJSValue null = eng.evaluate("null");
    QCOMPARE(undefined.equals(undefined), true);
    QCOMPARE(null.equals(null), true);
    QCOMPARE(undefined.equals(null), true);
    QCOMPARE(null.equals(undefined), true);
    QVERIFY(undefined.equals(QJSValue()));
    QVERIFY(null.equals(QJSValue()));
    QVERIFY(!null.equals(num));
    QVERIFY(!undefined.equals(num));

    QJSValue sant = eng.toScriptValue(true);
    QVERIFY(sant.equals(eng.toScriptValue(1)));
    QVERIFY(sant.equals(eng.toScriptValue(QString::fromLatin1("1"))));
    QVERIFY(sant.equals(sant));
    QVERIFY(sant.equals(eng.evaluate("new Number(1)")));
    QVERIFY(sant.equals(eng.evaluate("new String('1')")));
    QVERIFY(sant.equals(eng.evaluate("new Boolean(true)")));
    QVERIFY(eng.evaluate("new Boolean(true)").equals(sant));
    QVERIFY(!sant.equals(eng.toScriptValue(0)));
    QVERIFY(!sant.equals(undefined));
    QVERIFY(!sant.equals(null));

    QJSValue falskt = eng.toScriptValue(false);
    QVERIFY(falskt.equals(eng.toScriptValue(0)));
    QVERIFY(falskt.equals(eng.toScriptValue(QString::fromLatin1("0"))));
    QVERIFY(falskt.equals(falskt));
    QVERIFY(falskt.equals(eng.evaluate("new Number(0)")));
    QVERIFY(falskt.equals(eng.evaluate("new String('0')")));
    QVERIFY(falskt.equals(eng.evaluate("new Boolean(false)")));
    QVERIFY(eng.evaluate("new Boolean(false)").equals(falskt));
    QVERIFY(!falskt.equals(sant));
    QVERIFY(!falskt.equals(undefined));
    QVERIFY(!falskt.equals(null));

    QJSValue obj1 = eng.newObject();
    QJSValue obj2 = eng.newObject();
    QCOMPARE(obj1.equals(obj2), false);
    QCOMPARE(obj2.equals(obj1), false);
    QCOMPARE(obj1.equals(obj1), true);
    QCOMPARE(obj2.equals(obj2), true);

    QJSValue qobj1 = eng.newQObject(&temp);
    QJSValue qobj2 = eng.newQObject(&temp);
    QJSValue qobj3 = eng.newQObject(0);

    // FIXME: No ScriptOwnership: QJSValue qobj4 = eng.newQObject(new QObject(), QScriptEngine::ScriptOwnership);
    QJSValue qobj4 = eng.newQObject(new QObject());

    QVERIFY(qobj1.equals(qobj2)); // compares the QObject pointers
    QVERIFY(!qobj2.equals(qobj4)); // compares the QObject pointers
    QVERIFY(!qobj2.equals(obj2)); // compares the QObject pointers

    QJSValue compareFun = eng.evaluate("(function(a, b) { return a == b; })");
    QVERIFY(compareFun.isCallable());
    {
        QJSValue ret = compareFun.call(QJSValueList() << qobj1 << qobj2);
        QVERIFY(ret.isBool());
        ret = compareFun.call(QJSValueList() << qobj1 << qobj3);
        QVERIFY(ret.isBool());
        QVERIFY(!ret.toBool());
        ret = compareFun.call(QJSValueList() << qobj1 << qobj4);
        QVERIFY(ret.isBool());
        QVERIFY(!ret.toBool());
        ret = compareFun.call(QJSValueList() << qobj1 << obj1);
        QVERIFY(ret.isBool());
        QVERIFY(!ret.toBool());
    }

    {
        QJSValue var1 = eng.toScriptValue(QVariant(QPoint(1, 2)));
        QJSValue var2 = eng.toScriptValue(QVariant(QPoint(1, 2)));
        QVERIFY(var1.equals(var2));
    }
    {
        QJSValue var1 = eng.toScriptValue(QVariant(QPoint(1, 2)));
        QJSValue var2 = eng.toScriptValue(QVariant(QPoint(3, 4)));
        QVERIFY(!var1.equals(var2));
    }
}

void tst_QJSValue::strictlyEquals()
{
    QJSEngine eng;
    QObject temp;

    QVERIFY(QJSValue().strictlyEquals(QJSValue()));

    QJSValue num = eng.toScriptValue(123);
    QCOMPARE(num.strictlyEquals(eng.toScriptValue(123)), true);
    QCOMPARE(num.strictlyEquals(eng.toScriptValue(321)), false);
    QCOMPARE(num.strictlyEquals(eng.toScriptValue(QString::fromLatin1("123"))), false);
    QCOMPARE(num.strictlyEquals(eng.toScriptValue(QString::fromLatin1("321"))), false);
    QCOMPARE(num.strictlyEquals(eng.evaluate("new Number(123)")), false);
    QCOMPARE(num.strictlyEquals(eng.evaluate("new Number(321)")), false);
    QCOMPARE(num.strictlyEquals(eng.evaluate("new String('123')")), false);
    QCOMPARE(num.strictlyEquals(eng.evaluate("new String('321')")), false);
    QVERIFY(!eng.evaluate("new Number(123)").strictlyEquals(num));
    QVERIFY(!num.strictlyEquals(QJSValue()));
    QVERIFY(!QJSValue().strictlyEquals(num));

    QJSValue str = eng.toScriptValue(QString::fromLatin1("123"));
    QCOMPARE(str.strictlyEquals(eng.toScriptValue(QString::fromLatin1("123"))), true);
    QCOMPARE(str.strictlyEquals(eng.toScriptValue(QString::fromLatin1("321"))), false);
    QCOMPARE(str.strictlyEquals(eng.toScriptValue(123)), false);
    QCOMPARE(str.strictlyEquals(eng.toScriptValue(321)), false);
    QCOMPARE(str.strictlyEquals(eng.evaluate("new String('123')")), false);
    QCOMPARE(str.strictlyEquals(eng.evaluate("new String('321')")), false);
    QCOMPARE(str.strictlyEquals(eng.evaluate("new Number(123)")), false);
    QCOMPARE(str.strictlyEquals(eng.evaluate("new Number(321)")), false);
    QVERIFY(!eng.evaluate("new String('123')").strictlyEquals(str));
    QVERIFY(!str.strictlyEquals(QJSValue()));

    QJSValue num2 = QJSValue(123);
    QCOMPARE(num2.strictlyEquals(QJSValue(123)), true);
    QCOMPARE(num2.strictlyEquals(QJSValue(321)), false);
    QCOMPARE(num2.strictlyEquals(QJSValue("123")), false);
    QCOMPARE(num2.strictlyEquals(QJSValue("321")), false);
    QVERIFY(!num2.strictlyEquals(QJSValue()));

    QJSValue str2 = QJSValue("123");
    QCOMPARE(str2.strictlyEquals(QJSValue("123")), true);
    QCOMPARE(str2.strictlyEquals(QJSValue("321")), false);
    QCOMPARE(str2.strictlyEquals(QJSValue(123)), false);
    QCOMPARE(str2.strictlyEquals(QJSValue(321)), false);
    QVERIFY(!str2.strictlyEquals(QJSValue()));

    QJSValue date1 = eng.toScriptValue(QDateTime(QDate(2000, 1, 1)));
    QJSValue date2 = eng.toScriptValue(QDateTime(QDate(1999, 1, 1)));
    QCOMPARE(date1.strictlyEquals(date2), false);
    QCOMPARE(date1.strictlyEquals(date1), true);
    QCOMPARE(date2.strictlyEquals(date2), true);
    QVERIFY(!date1.strictlyEquals(QJSValue()));

    QJSValue undefined = eng.toScriptValue(QVariant());
    QJSValue null = eng.evaluate("null");
    QCOMPARE(undefined.strictlyEquals(undefined), true);
    QCOMPARE(null.strictlyEquals(null), true);
    QCOMPARE(undefined.strictlyEquals(null), false);
    QCOMPARE(null.strictlyEquals(undefined), false);
    QVERIFY(!null.strictlyEquals(QJSValue()));

    QJSValue sant = eng.toScriptValue(true);
    QVERIFY(!sant.strictlyEquals(eng.toScriptValue(1)));
    QVERIFY(!sant.strictlyEquals(eng.toScriptValue(QString::fromLatin1("1"))));
    QVERIFY(sant.strictlyEquals(sant));
    QVERIFY(!sant.strictlyEquals(eng.evaluate("new Number(1)")));
    QVERIFY(!sant.strictlyEquals(eng.evaluate("new String('1')")));
    QVERIFY(!sant.strictlyEquals(eng.evaluate("new Boolean(true)")));
    QVERIFY(!eng.evaluate("new Boolean(true)").strictlyEquals(sant));
    QVERIFY(!sant.strictlyEquals(eng.toScriptValue(0)));
    QVERIFY(!sant.strictlyEquals(undefined));
    QVERIFY(!sant.strictlyEquals(null));
    QVERIFY(!sant.strictlyEquals(QJSValue()));

    QJSValue falskt = eng.toScriptValue(false);
    QVERIFY(!falskt.strictlyEquals(eng.toScriptValue(0)));
    QVERIFY(!falskt.strictlyEquals(eng.toScriptValue(QString::fromLatin1("0"))));
    QVERIFY(falskt.strictlyEquals(falskt));
    QVERIFY(!falskt.strictlyEquals(eng.evaluate("new Number(0)")));
    QVERIFY(!falskt.strictlyEquals(eng.evaluate("new String('0')")));
    QVERIFY(!falskt.strictlyEquals(eng.evaluate("new Boolean(false)")));
    QVERIFY(!eng.evaluate("new Boolean(false)").strictlyEquals(falskt));
    QVERIFY(!falskt.strictlyEquals(sant));
    QVERIFY(!falskt.strictlyEquals(undefined));
    QVERIFY(!falskt.strictlyEquals(null));
    QVERIFY(!falskt.strictlyEquals(QJSValue()));

    QVERIFY(!QJSValue(false).strictlyEquals(123));
    QVERIFY(!QJSValue(QJSValue::UndefinedValue).strictlyEquals(123));
    QVERIFY(!QJSValue(QJSValue::NullValue).strictlyEquals(123));
    QVERIFY(!QJSValue(false).strictlyEquals("ciao"));
    QVERIFY(!QJSValue(QJSValue::UndefinedValue).strictlyEquals("ciao"));
    QVERIFY(!QJSValue(QJSValue::NullValue).strictlyEquals("ciao"));
    QVERIFY(eng.toScriptValue(QString::fromLatin1("ciao")).strictlyEquals("ciao"));
    QVERIFY(QJSValue("ciao").strictlyEquals(eng.toScriptValue(QString::fromLatin1("ciao"))));
    QVERIFY(!QJSValue("ciao").strictlyEquals(123));
    QVERIFY(!QJSValue("ciao").strictlyEquals(eng.toScriptValue(123)));
    QVERIFY(!QJSValue(123).strictlyEquals("ciao"));
    QVERIFY(!QJSValue(123).strictlyEquals(eng.toScriptValue(QString::fromLatin1("ciao"))));
    QVERIFY(!eng.toScriptValue(123).strictlyEquals("ciao"));

    QJSValue obj1 = eng.newObject();
    QJSValue obj2 = eng.newObject();
    QCOMPARE(obj1.strictlyEquals(obj2), false);
    QCOMPARE(obj2.strictlyEquals(obj1), false);
    QCOMPARE(obj1.strictlyEquals(obj1), true);
    QCOMPARE(obj2.strictlyEquals(obj2), true);
    QVERIFY(!obj1.strictlyEquals(QJSValue()));

    QJSValue qobj1 = eng.newQObject(&temp);
    QJSValue qobj2 = eng.newQObject(&temp);
    QVERIFY(qobj1.strictlyEquals(qobj2));

    {
        QJSValue var1 = eng.toScriptValue(QVariant(QStringList() << "a"));
        QJSValue var2 = eng.toScriptValue(QVariant(QStringList() << "a"));
        QVERIFY(var1.isArray());
        QVERIFY(var2.isArray());
        QVERIFY(!var1.strictlyEquals(var2));
    }
    {
        QJSValue var1 = eng.toScriptValue(QVariant(QStringList() << "a"));
        QJSValue var2 = eng.toScriptValue(QVariant(QStringList() << "b"));
        QVERIFY(!var1.strictlyEquals(var2));
    }
    {
        QJSValue var1 = eng.toScriptValue(QVariant(QPoint(1, 2)));
        QJSValue var2 = eng.toScriptValue(QVariant(QPoint(1, 2)));
        QVERIFY(var1.strictlyEquals(var2));
    }
    {
        QJSValue var1 = eng.toScriptValue(QVariant(QPoint(1, 2)));
        QJSValue var2 = eng.toScriptValue(QVariant(QPoint(3, 4)));
        QVERIFY(!var1.strictlyEquals(var2));
    }
}

Q_DECLARE_METATYPE(int*)
Q_DECLARE_METATYPE(double*)
Q_DECLARE_METATYPE(QColor*)
Q_DECLARE_METATYPE(QBrush*)

void tst_QJSValue::castToPointer()
{
    QJSEngine eng;
    {
        QColor c(123, 210, 231);
        QJSValue v = eng.toScriptValue(c);
        QColor *cp = qjsvalue_cast<QColor*>(v);
        QVERIFY(cp != 0);
        QCOMPARE(*cp, c);

        QBrush *bp = qjsvalue_cast<QBrush*>(v);
        QVERIFY(!bp);

        QJSValue v2 = eng.toScriptValue(qVariantFromValue(cp));
        QCOMPARE(qjsvalue_cast<QColor*>(v2), cp);
    }
}

void tst_QJSValue::prettyPrinter_data()
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
    QTest::newRow("break") << QString("function() { for (; ; ) { break; } }") << QString("function () { for (; ; ) { break; } }");
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
    QTest::newRow("debugger") << QString("function() { debugger; }") << QString("function () { debugger; }");
}

void tst_QJSValue::prettyPrinter()
{
    QFETCH(QString, function);
    QFETCH(QString, expected);
    QJSEngine eng;
    QJSValue val = eng.evaluate(QLatin1Char('(') + function + QLatin1Char(')'));
    QVERIFY(val.isCallable());
    QString actual = val.toString();
    QSKIP("Function::toString() doesn't give the whole function on v4");
    int count = qMin(actual.size(), expected.size());
    for (int i = 0; i < count; ++i) {
        QCOMPARE(actual.at(i), expected.at(i));
    }
    QCOMPARE(actual.size(), expected.size());
}

void tst_QJSValue::engineDeleted()
{
    QJSEngine *eng = new QJSEngine;
    QObject *temp = new QObject(); // Owned by JS engine, as newQObject() sets JS ownership explicitly
    QJSValue v1 = eng->toScriptValue(123);
    QVERIFY(v1.isNumber());
    QJSValue v2 = eng->toScriptValue(QString("ciao"));
    QVERIFY(v2.isString());
    QJSValue v3 = eng->newObject();
    QVERIFY(v3.isObject());
    QJSValue v4 = eng->newQObject(temp);
    QVERIFY(v4.isQObject());
    QJSValue v5 = "Hello";
    QVERIFY(v2.isString());

    delete eng;

    QVERIFY(v1.isUndefined());
    QVERIFY(!v1.engine());
    QVERIFY(v2.isUndefined());
    QVERIFY(!v2.engine());
    QVERIFY(v3.isUndefined());
    QVERIFY(!v3.engine());
    QVERIFY(v4.isUndefined());
    QVERIFY(!v4.engine());
    QVERIFY(v5.isString()); // was not bound to engine
    QVERIFY(!v5.engine());

    QVERIFY(v3.property("foo").isUndefined());
}

void tst_QJSValue::valueOfWithClosure()
{
    QJSEngine eng;
    // valueOf()
    {
        QJSValue obj = eng.evaluate("o = {}; (function(foo) { o.valueOf = function() { return foo; } })(123); o");
        QVERIFY(obj.isObject());
        QCOMPARE(obj.toInt(), 123);
    }
    // toString()
    {
        QJSValue obj = eng.evaluate("o = {}; (function(foo) { o.toString = function() { return foo; } })('ciao'); o");
        QVERIFY(obj.isObject());
        QCOMPARE(obj.toString(), QString::fromLatin1("ciao"));
    }
}

void tst_QJSValue::nestedObjectToVariant_data()
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
        QTest::newRow("{}")
            << QString::fromLatin1("({})")
            << QVariant(m);
    }
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

void tst_QJSValue::nestedObjectToVariant()
{
    QJSEngine eng;
    QFETCH(QString, program);
    QFETCH(QVariant, expected);
    QJSValue o = eng.evaluate(program);
    QVERIFY(!o.isError());
    QVERIFY(o.isObject());
    QCOMPARE(o.toVariant(), expected);
}

QTEST_MAIN(tst_QJSValue)
