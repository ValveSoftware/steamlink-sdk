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


void tst_QScriptValueGenerated::isValid_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isValid_array[] = {
    "QScriptValue(QScriptValue::UndefinedValue)",
    "QScriptValue(QScriptValue::NullValue)",
    "QScriptValue(true)",
    "QScriptValue(false)",
    "QScriptValue(int(122))",
    "QScriptValue(uint(124))",
    "QScriptValue(0)",
    "QScriptValue(0.0)",
    "QScriptValue(123.0)",
    "QScriptValue(6.37e-8)",
    "QScriptValue(-6.37e-8)",
    "QScriptValue(0x43211234)",
    "QScriptValue(0x10000)",
    "QScriptValue(0x10001)",
    "QScriptValue(qSNaN())",
    "QScriptValue(qQNaN())",
    "QScriptValue(qInf())",
    "QScriptValue(-qInf())",
    "QScriptValue(\"NaN\")",
    "QScriptValue(\"Infinity\")",
    "QScriptValue(\"-Infinity\")",
    "QScriptValue(\"ciao\")",
    "QScriptValue(QString::fromLatin1(\"ciao\"))",
    "QScriptValue(QString(\"\"))",
    "QScriptValue(QString())",
    "QScriptValue(QString(\"0\"))",
    "QScriptValue(QString(\"123\"))",
    "QScriptValue(QString(\"12.4\"))",
    "QScriptValue(0, QScriptValue::UndefinedValue)",
    "QScriptValue(0, QScriptValue::NullValue)",
    "QScriptValue(0, true)",
    "QScriptValue(0, false)",
    "QScriptValue(0, int(122))",
    "QScriptValue(0, uint(124))",
    "QScriptValue(0, 0)",
    "QScriptValue(0, 0.0)",
    "QScriptValue(0, 123.0)",
    "QScriptValue(0, 6.37e-8)",
    "QScriptValue(0, -6.37e-8)",
    "QScriptValue(0, 0x43211234)",
    "QScriptValue(0, 0x10000)",
    "QScriptValue(0, 0x10001)",
    "QScriptValue(0, qSNaN())",
    "QScriptValue(0, qQNaN())",
    "QScriptValue(0, qInf())",
    "QScriptValue(0, -qInf())",
    "QScriptValue(0, \"NaN\")",
    "QScriptValue(0, \"Infinity\")",
    "QScriptValue(0, \"-Infinity\")",
    "QScriptValue(0, \"ciao\")",
    "QScriptValue(0, QString::fromLatin1(\"ciao\"))",
    "QScriptValue(0, QString(\"\"))",
    "QScriptValue(0, QString())",
    "QScriptValue(0, QString(\"0\"))",
    "QScriptValue(0, QString(\"123\"))",
    "QScriptValue(0, QString(\"12.3\"))",
    "QScriptValue(engine, QScriptValue::UndefinedValue)",
    "QScriptValue(engine, QScriptValue::NullValue)",
    "QScriptValue(engine, true)",
    "QScriptValue(engine, false)",
    "QScriptValue(engine, int(122))",
    "QScriptValue(engine, uint(124))",
    "QScriptValue(engine, 0)",
    "QScriptValue(engine, 0.0)",
    "QScriptValue(engine, 123.0)",
    "QScriptValue(engine, 6.37e-8)",
    "QScriptValue(engine, -6.37e-8)",
    "QScriptValue(engine, 0x43211234)",
    "QScriptValue(engine, 0x10000)",
    "QScriptValue(engine, 0x10001)",
    "QScriptValue(engine, qSNaN())",
    "QScriptValue(engine, qQNaN())",
    "QScriptValue(engine, qInf())",
    "QScriptValue(engine, -qInf())",
    "QScriptValue(engine, \"NaN\")",
    "QScriptValue(engine, \"Infinity\")",
    "QScriptValue(engine, \"-Infinity\")",
    "QScriptValue(engine, \"ciao\")",
    "QScriptValue(engine, QString::fromLatin1(\"ciao\"))",
    "QScriptValue(engine, QString(\"\"))",
    "QScriptValue(engine, QString())",
    "QScriptValue(engine, QString(\"0\"))",
    "QScriptValue(engine, QString(\"123\"))",
    "QScriptValue(engine, QString(\"1.23\"))",
    "engine->evaluate(\"[]\")",
    "engine->evaluate(\"{}\")",
    "engine->evaluate(\"Object.prototype\")",
    "engine->evaluate(\"Date.prototype\")",
    "engine->evaluate(\"Array.prototype\")",
    "engine->evaluate(\"Function.prototype\")",
    "engine->evaluate(\"Error.prototype\")",
    "engine->evaluate(\"Object\")",
    "engine->evaluate(\"Array\")",
    "engine->evaluate(\"Number\")",
    "engine->evaluate(\"Function\")",
    "engine->evaluate(\"(function() { return 1; })\")",
    "engine->evaluate(\"(function() { return 'ciao'; })\")",
    "engine->evaluate(\"(function() { throw new Error('foo'); })\")",
    "engine->evaluate(\"/foo/\")",
    "engine->evaluate(\"new Object()\")",
    "engine->evaluate(\"new Array()\")",
    "engine->evaluate(\"new Error()\")",
    "engine->evaluate(\"new Boolean(true)\")",
    "engine->evaluate(\"new Boolean(false)\")",
    "engine->evaluate(\"new Number(123)\")",
    "engine->evaluate(\"new RegExp('foo', 'gim')\")",
    "engine->evaluate(\"new String('ciao')\")",
    "engine->evaluate(\"a = new Object(); a.foo = 22; a.foo\")",
    "engine->evaluate(\"Undefined\")",
    "engine->evaluate(\"Null\")",
    "engine->evaluate(\"True\")",
    "engine->evaluate(\"False\")",
    "engine->evaluate(\"undefined\")",
    "engine->evaluate(\"null\")",
    "engine->evaluate(\"true\")",
    "engine->evaluate(\"false\")",
    "engine->evaluate(\"122\")",
    "engine->evaluate(\"124\")",
    "engine->evaluate(\"0\")",
    "engine->evaluate(\"0.0\")",
    "engine->evaluate(\"123.0\")",
    "engine->evaluate(\"6.37e-8\")",
    "engine->evaluate(\"-6.37e-8\")",
    "engine->evaluate(\"0x43211234\")",
    "engine->evaluate(\"0x10000\")",
    "engine->evaluate(\"0x10001\")",
    "engine->evaluate(\"NaN\")",
    "engine->evaluate(\"Infinity\")",
    "engine->evaluate(\"-Infinity\")",
    "engine->evaluate(\"'ciao'\")",
    "engine->evaluate(\"''\")",
    "engine->evaluate(\"'0'\")",
    "engine->evaluate(\"'123'\")",
    "engine->evaluate(\"'12.4'\")",
    "engine->nullValue()",
    "engine->undefinedValue()",
    "engine->newObject()",
    "engine->newArray()",
    "engine->newArray(10)",
    "engine->newDate(QDateTime())",
    "engine->newQMetaObject(&QObject::staticMetaObject)",
    "engine->newRegExp(\"foo\", \"gim\")",
    "engine->newVariant(QVariant())",
    "engine->newVariant(QVariant(123))",
    "engine->newVariant(QVariant(false))",
    "engine->newQObject(0)",
    "engine->newQObject(engine)"
};

void tst_QScriptValueGenerated::isValid_makeData(const char* expr)
{
    static const QSet<QString> isValid =
        charArrayToQStringSet(isValid_array, int(sizeof(isValid_array) / sizeof(const char *)));
    newRow(expr) << isValid.contains(expr);
}

void tst_QScriptValueGenerated::isValid_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isValid(), expected);
    QCOMPARE(value.isValid(), expected);
}

DEFINE_TEST_FUNCTION(isValid)


void tst_QScriptValueGenerated::isBool_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isBool_array[] = {
    "QScriptValue(true)",
    "QScriptValue(false)",
    "QScriptValue(0, true)",
    "QScriptValue(0, false)",
    "QScriptValue(engine, true)",
    "QScriptValue(engine, false)",
    "engine->evaluate(\"true\")",
    "engine->evaluate(\"false\")"
};

void tst_QScriptValueGenerated::isBool_makeData(const char* expr)
{
    static const QSet<QString> isBool =
            charArrayToQStringSet(isBool_array, int(sizeof(isBool_array) / sizeof(const char *)));
    newRow(expr) << isBool.contains(expr);
}

void tst_QScriptValueGenerated::isBool_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isBool(), expected);
    QCOMPARE(value.isBool(), expected);
}

DEFINE_TEST_FUNCTION(isBool)


void tst_QScriptValueGenerated::isBoolean_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char * isBoolean_array[] = {
    "QScriptValue(true)",
    "QScriptValue(false)",
    "QScriptValue(0, true)",
    "QScriptValue(0, false)",
    "QScriptValue(engine, true)",
    "QScriptValue(engine, false)",
    "engine->evaluate(\"true\")",
    "engine->evaluate(\"false\")"
};

void tst_QScriptValueGenerated::isBoolean_makeData(const char* expr)
{
    static const QSet<QString> isBoolean =
        charArrayToQStringSet(isBoolean_array, int(sizeof(isBoolean_array) / sizeof(const char *)));
    newRow(expr) << isBoolean.contains(expr);
}

void tst_QScriptValueGenerated::isBoolean_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isBoolean(), expected);
    QCOMPARE(value.isBoolean(), expected);
}

DEFINE_TEST_FUNCTION(isBoolean)


void tst_QScriptValueGenerated::isNumber_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isNumber_array[] = {
    "QScriptValue(int(122))",
    "QScriptValue(uint(124))",
    "QScriptValue(0)",
    "QScriptValue(0.0)",
    "QScriptValue(123.0)",
    "QScriptValue(6.37e-8)",
    "QScriptValue(-6.37e-8)",
    "QScriptValue(0x43211234)",
    "QScriptValue(0x10000)",
    "QScriptValue(0x10001)",
    "QScriptValue(qSNaN())",
    "QScriptValue(qQNaN())",
    "QScriptValue(qInf())",
    "QScriptValue(-qInf())",
    "QScriptValue(0, int(122))",
    "QScriptValue(0, uint(124))",
    "QScriptValue(0, 0)",
    "QScriptValue(0, 0.0)",
    "QScriptValue(0, 123.0)",
    "QScriptValue(0, 6.37e-8)",
    "QScriptValue(0, -6.37e-8)",
    "QScriptValue(0, 0x43211234)",
    "QScriptValue(0, 0x10000)",
    "QScriptValue(0, 0x10001)",
    "QScriptValue(0, qSNaN())",
    "QScriptValue(0, qQNaN())",
    "QScriptValue(0, qInf())",
    "QScriptValue(0, -qInf())",
    "QScriptValue(engine, int(122))",
    "QScriptValue(engine, uint(124))",
    "QScriptValue(engine, 0)",
    "QScriptValue(engine, 0.0)",
    "QScriptValue(engine, 123.0)",
    "QScriptValue(engine, 6.37e-8)",
    "QScriptValue(engine, -6.37e-8)",
    "QScriptValue(engine, 0x43211234)",
    "QScriptValue(engine, 0x10000)",
    "QScriptValue(engine, 0x10001)",
    "QScriptValue(engine, qSNaN())",
    "QScriptValue(engine, qQNaN())",
    "QScriptValue(engine, qInf())",
    "QScriptValue(engine, -qInf())",
    "engine->evaluate(\"a = new Object(); a.foo = 22; a.foo\")",
    "engine->evaluate(\"122\")",
    "engine->evaluate(\"124\")",
    "engine->evaluate(\"0\")",
    "engine->evaluate(\"0.0\")",
    "engine->evaluate(\"123.0\")",
    "engine->evaluate(\"6.37e-8\")",
    "engine->evaluate(\"-6.37e-8\")",
    "engine->evaluate(\"0x43211234\")",
    "engine->evaluate(\"0x10000\")",
    "engine->evaluate(\"0x10001\")",
    "engine->evaluate(\"NaN\")",
    "engine->evaluate(\"Infinity\")",
    "engine->evaluate(\"-Infinity\")"
};

void tst_QScriptValueGenerated::isNumber_makeData(const char* expr)
{
    static const QSet<QString> isNumber =
        charArrayToQStringSet(isNumber_array, int(sizeof(isNumber_array) / sizeof(const char *)));
    newRow(expr) << isNumber.contains(expr);
}

void tst_QScriptValueGenerated::isNumber_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isNumber(), expected);
    QCOMPARE(value.isNumber(), expected);
}

DEFINE_TEST_FUNCTION(isNumber)


void tst_QScriptValueGenerated::isFunction_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isFunction_array[] = {
    "engine->evaluate(\"Function.prototype\")",
    "engine->evaluate(\"Object\")",
    "engine->evaluate(\"Array\")",
    "engine->evaluate(\"Number\")",
    "engine->evaluate(\"Function\")",
    "engine->evaluate(\"(function() { return 1; })\")",
    "engine->evaluate(\"(function() { return 'ciao'; })\")",
    "engine->evaluate(\"(function() { throw new Error('foo'); })\")",
    "engine->evaluate(\"/foo/\")",
    "engine->evaluate(\"new RegExp('foo', 'gim')\")",
    "engine->newQMetaObject(&QObject::staticMetaObject)",
    "engine->newRegExp(\"foo\", \"gim\")"
};

void tst_QScriptValueGenerated::isFunction_makeData(const char* expr)
{
    static const QSet<QString> isFunction
        = charArrayToQStringSet(isFunction_array, int(sizeof(isFunction_array) / sizeof(const char *)));
    newRow(expr) << isFunction.contains(expr);
}

void tst_QScriptValueGenerated::isFunction_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isFunction(), expected);
    QCOMPARE(value.isFunction(), expected);
}

DEFINE_TEST_FUNCTION(isFunction)


void tst_QScriptValueGenerated::isNull_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isNull_array[] = {
    "QScriptValue(QScriptValue::NullValue)",
    "QScriptValue(0, QScriptValue::NullValue)",
    "QScriptValue(engine, QScriptValue::NullValue)",
    "engine->evaluate(\"null\")",
    "engine->nullValue()",
    "engine->newQObject(0)"
};

void tst_QScriptValueGenerated::isNull_makeData(const char* expr)
{
    static const QSet<QString> isNull =
        charArrayToQStringSet(isNull_array, int(sizeof(isNull_array) / sizeof(const char *)));
    newRow(expr) << isNull.contains(expr);
}

void tst_QScriptValueGenerated::isNull_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isNull(), expected);
    QCOMPARE(value.isNull(), expected);
}

DEFINE_TEST_FUNCTION(isNull)


void tst_QScriptValueGenerated::isString_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isString_array[] = {
    "QScriptValue(\"NaN\")",
    "QScriptValue(\"Infinity\")",
    "QScriptValue(\"-Infinity\")",
    "QScriptValue(\"ciao\")",
    "QScriptValue(QString::fromLatin1(\"ciao\"))",
    "QScriptValue(QString(\"\"))",
    "QScriptValue(QString())",
    "QScriptValue(QString(\"0\"))",
    "QScriptValue(QString(\"123\"))",
    "QScriptValue(QString(\"12.4\"))",
    "QScriptValue(0, \"NaN\")",
    "QScriptValue(0, \"Infinity\")",
    "QScriptValue(0, \"-Infinity\")",
    "QScriptValue(0, \"ciao\")",
    "QScriptValue(0, QString::fromLatin1(\"ciao\"))",
    "QScriptValue(0, QString(\"\"))",
    "QScriptValue(0, QString())",
    "QScriptValue(0, QString(\"0\"))",
    "QScriptValue(0, QString(\"123\"))",
    "QScriptValue(0, QString(\"12.3\"))",
    "QScriptValue(engine, \"NaN\")",
    "QScriptValue(engine, \"Infinity\")",
    "QScriptValue(engine, \"-Infinity\")",
    "QScriptValue(engine, \"ciao\")",
    "QScriptValue(engine, QString::fromLatin1(\"ciao\"))",
    "QScriptValue(engine, QString(\"\"))",
    "QScriptValue(engine, QString())",
    "QScriptValue(engine, QString(\"0\"))",
    "QScriptValue(engine, QString(\"123\"))",
    "QScriptValue(engine, QString(\"1.23\"))",
    "engine->evaluate(\"'ciao'\")",
    "engine->evaluate(\"''\")",
    "engine->evaluate(\"'0'\")",
    "engine->evaluate(\"'123'\")",
    "engine->evaluate(\"'12.4'\")"
};

void tst_QScriptValueGenerated::isString_makeData(const char* expr)
{
    static const QSet<QString> isString =
        charArrayToQStringSet(isString_array, int(sizeof(isString_array) / sizeof(const char *)));
    newRow(expr) << isString.contains(expr);
}

void tst_QScriptValueGenerated::isString_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isString(), expected);
    QCOMPARE(value.isString(), expected);
}

DEFINE_TEST_FUNCTION(isString)


void tst_QScriptValueGenerated::isUndefined_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isUndefined_array[] = {
    "QScriptValue(QScriptValue::UndefinedValue)",
    "QScriptValue(0, QScriptValue::UndefinedValue)",
    "QScriptValue(engine, QScriptValue::UndefinedValue)",
    "engine->evaluate(\"{}\")",
    "engine->evaluate(\"undefined\")",
    "engine->undefinedValue()"
};

void tst_QScriptValueGenerated::isUndefined_makeData(const char* expr)
{
    static const QSet<QString> isUndefined =
        charArrayToQStringSet(isUndefined_array, int(sizeof(isUndefined_array) / sizeof(const char *)));
    newRow(expr) << isUndefined.contains(expr);
}

void tst_QScriptValueGenerated::isUndefined_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isUndefined(), expected);
    QCOMPARE(value.isUndefined(), expected);
}

DEFINE_TEST_FUNCTION(isUndefined)


void tst_QScriptValueGenerated::isVariant_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isVariant_array[] = {
    "engine->newVariant(QVariant())",
    "engine->newVariant(QVariant(123))",
    "engine->newVariant(QVariant(false))"
};

void tst_QScriptValueGenerated::isVariant_makeData(const char* expr)
{
    static QSet<QString> isVariant =
        charArrayToQStringSet(isVariant_array, int(sizeof(isVariant_array) / sizeof(const char *)));
    newRow(expr) << isVariant.contains(expr);
}

void tst_QScriptValueGenerated::isVariant_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isVariant(), expected);
    QCOMPARE(value.isVariant(), expected);
}

DEFINE_TEST_FUNCTION(isVariant)


void tst_QScriptValueGenerated::isQObject_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValueGenerated::isQObject_makeData(const char* expr)
{
    newRow(expr) << (qstrcmp(expr, "engine->newQObject(engine)") == 0);
}

void tst_QScriptValueGenerated::isQObject_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isQObject(), expected);
    QCOMPARE(value.isQObject(), expected);
}

DEFINE_TEST_FUNCTION(isQObject)


void tst_QScriptValueGenerated::isQMetaObject_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValueGenerated::isQMetaObject_makeData(const char* expr)
{
    newRow(expr) << (qstrcmp(expr, "engine->newQMetaObject(&QObject::staticMetaObject)") ==  0);
}

void tst_QScriptValueGenerated::isQMetaObject_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isQMetaObject(), expected);
    QCOMPARE(value.isQMetaObject(), expected);
}

DEFINE_TEST_FUNCTION(isQMetaObject)


void tst_QScriptValueGenerated::isObject_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isObject_array[] = {
    "engine->evaluate(\"[]\")",
    "engine->evaluate(\"Object.prototype\")",
    "engine->evaluate(\"Date.prototype\")",
    "engine->evaluate(\"Array.prototype\")",
    "engine->evaluate(\"Function.prototype\")",
    "engine->evaluate(\"Error.prototype\")",
    "engine->evaluate(\"Object\")",
    "engine->evaluate(\"Array\")",
    "engine->evaluate(\"Number\")",
    "engine->evaluate(\"Function\")",
    "engine->evaluate(\"(function() { return 1; })\")",
    "engine->evaluate(\"(function() { return 'ciao'; })\")",
    "engine->evaluate(\"(function() { throw new Error('foo'); })\")",
    "engine->evaluate(\"/foo/\")",
    "engine->evaluate(\"new Object()\")",
    "engine->evaluate(\"new Array()\")",
    "engine->evaluate(\"new Error()\")",
    "engine->evaluate(\"new Boolean(true)\")",
    "engine->evaluate(\"new Boolean(false)\")",
    "engine->evaluate(\"new Number(123)\")",
    "engine->evaluate(\"new RegExp('foo', 'gim')\")",
    "engine->evaluate(\"new String('ciao')\")",
    "engine->evaluate(\"Undefined\")",
    "engine->evaluate(\"Null\")",
    "engine->evaluate(\"True\")",
    "engine->evaluate(\"False\")",
    "engine->newObject()",
    "engine->newArray()",
    "engine->newArray(10)",
    "engine->newDate(QDateTime())",
    "engine->newQMetaObject(&QObject::staticMetaObject)",
    "engine->newRegExp(\"foo\", \"gim\")",
    "engine->newVariant(QVariant())",
    "engine->newVariant(QVariant(123))",
    "engine->newVariant(QVariant(false))",
    "engine->newQObject(engine)"
};

void tst_QScriptValueGenerated::isObject_makeData(const char* expr)
{
    static const QSet<QString> isObject =
        charArrayToQStringSet(isObject_array, int(sizeof(isObject_array) / sizeof(const char *)));
    newRow(expr) << isObject.contains(expr);
}

void tst_QScriptValueGenerated::isObject_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isObject(), expected);
    QCOMPARE(value.isObject(), expected);
}

DEFINE_TEST_FUNCTION(isObject)


void tst_QScriptValueGenerated::isDate_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isDate_array[] = {
    "engine->evaluate(\"Date.prototype\")",
    "engine->newDate(QDateTime())"
};

void tst_QScriptValueGenerated::isDate_makeData(const char* expr)
{
    static const QSet<QString> isDate =
        charArrayToQStringSet(isDate_array, int(sizeof(isDate_array) / sizeof(const char *)));
    newRow(expr) << isDate.contains(expr);
}

void tst_QScriptValueGenerated::isDate_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isDate(), expected);
    QCOMPARE(value.isDate(), expected);
}

DEFINE_TEST_FUNCTION(isDate)


void tst_QScriptValueGenerated::isRegExp_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isRegExp_array[] = {
    "engine->evaluate(\"/foo/\")",
    "engine->evaluate(\"new RegExp('foo', 'gim')\")",
    "engine->newRegExp(\"foo\", \"gim\")"
};

void tst_QScriptValueGenerated::isRegExp_makeData(const char* expr)
{
    static const QSet<QString> isRegExp =
        charArrayToQStringSet(isRegExp_array, int(sizeof(isRegExp_array) / sizeof(const char *)));
    newRow(expr) << isRegExp.contains(expr);
}

void tst_QScriptValueGenerated::isRegExp_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isRegExp(), expected);
    QCOMPARE(value.isRegExp(), expected);
}

DEFINE_TEST_FUNCTION(isRegExp)


void tst_QScriptValueGenerated::isArray_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isArray_array[] = {
    "engine->evaluate(\"[]\")",
    "engine->evaluate(\"Array.prototype\")",
    "engine->evaluate(\"new Array()\")",
    "engine->newArray()",
    "engine->newArray(10)"
};

void tst_QScriptValueGenerated::isArray_makeData(const char* expr)
{
    static const QSet<QString> isArray =
        charArrayToQStringSet(isArray_array, int(sizeof(isArray_array) / sizeof(const char *)));
    newRow(expr) << isArray.contains(expr);
}

void tst_QScriptValueGenerated::isArray_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isArray(), expected);
    QCOMPARE(value.isArray(), expected);
}

DEFINE_TEST_FUNCTION(isArray)


void tst_QScriptValueGenerated::isError_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static const char *isError_array[] = {
    "engine->evaluate(\"Error.prototype\")",
    "engine->evaluate(\"new Error()\")",
    "engine->evaluate(\"Undefined\")",
    "engine->evaluate(\"Null\")",
    "engine->evaluate(\"True\")",
    "engine->evaluate(\"False\")"
};

void tst_QScriptValueGenerated::isError_makeData(const char* expr)
{
    static QSet<QString> isError =
        charArrayToQStringSet(isError_array, int(sizeof(isError_array) / sizeof(const char *)));
    newRow(expr) << isError.contains(expr);
}

void tst_QScriptValueGenerated::isError_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isError(), expected);
    QCOMPARE(value.isError(), expected);
}

DEFINE_TEST_FUNCTION(isError)

