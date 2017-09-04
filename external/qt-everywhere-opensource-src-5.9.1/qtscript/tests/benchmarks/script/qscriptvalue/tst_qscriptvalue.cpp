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

#include <qtest.h>
#include <QtScript>

Q_DECLARE_METATYPE(QScriptValue)

class tst_QScriptValue : public QObject
{
    Q_OBJECT

public:
    tst_QScriptValue();
    virtual ~tst_QScriptValue();

public slots:
    void init();
    void cleanup();

private slots:
    void boolConstructor();
    void floatConstructor();
    void numberConstructor();
    void stringConstructor();
    void nullConstructor();
    void undefinedConstructor();
    void boolConstructorWithEngine();
    void floatConstructorWithEngine();
    void intConstructorWithEngine();
    void stringConstructorWithEngine();
    void nullConstructorWithEngine();
    void undefinedConstructorWithEngine();
    void copyConstructor_data();
    void copyConstructor();
    void call_data();
    void call();
    void construct_data();
    void construct();
    void data();
    void setData();
    void data_noData_data();
    void data_noData();
    void engine_data();
    void engine();
    void equalsSelf_data();
    void equalsSelf();
    void lessThanSelf_data();
    void lessThanSelf();
    void strictlyEqualsSelf_data();
    void strictlyEqualsSelf();
    void instanceOf();
    void isArray_data();
    void isArray();
    void isBool_data();
    void isBool();
    void isDate_data();
    void isDate();
    void isError_data();
    void isError();
    void isFunction_data();
    void isFunction();
    void isNull_data();
    void isNull();
    void isNumber_data();
    void isNumber();
    void isObject_data();
    void isObject();
    void isQMetaObject_data();
    void isQMetaObject();
    void isQObject_data();
    void isQObject();
    void isRegExp_data();
    void isRegExp();
    void isString_data();
    void isString();
    void isUndefined_data();
    void isUndefined();
    void isValid_data();
    void isValid();
    void isVariant_data();
    void isVariant();
    void toBool_data();
    void toBool();
    void toDateTime_data();
    void toDateTime();
    void toInt32_data();
    void toInt32();
    void toInteger_data();
    void toInteger();
    void toNumber_data();
    void toNumber();
    void toRegExp_data();
    void toRegExp();
    void toString_data();
    void toString();
    void toUInt16_data();
    void toUInt16();
    void toUInt32_data();
    void toUInt32();
    void toQMetaObject_data();
    void toQMetaObject();
    void toQObject_data();
    void toQObject();
    void toVariant_data();
    void toVariant();
    void property_data();
    void property();
    void propertyById_data();
    void propertyById();
    void propertyByIndex();
    void setProperty_data();
    void setProperty();
    void setPropertyById_data();
    void setPropertyById();
    void setPropertyByIndex();
    void propertyFlags_data();
    void propertyFlags();
    void propertyFlagsById_data();
    void propertyFlagsById();
    void prototype_data();
    void prototype();
    void setPrototype();
    void scriptClass_data();
    void scriptClass();
    void setScriptClass();
    void readMetaProperty();
    void writeMetaProperty();

private:
    void defineStandardTestValues();
    void newEngine()
    {
        delete m_engine;
        m_engine = new QScriptEngine;
    }

    QScriptEngine *m_engine;
};

tst_QScriptValue::tst_QScriptValue()
    : m_engine(0)
{
}

tst_QScriptValue::~tst_QScriptValue()
{
    delete m_engine;
}

void tst_QScriptValue::init()
{
}

void tst_QScriptValue::cleanup()
{
}

void tst_QScriptValue::boolConstructor()
{
    QBENCHMARK {
        QScriptValue val(true);
    }
}

void tst_QScriptValue::floatConstructor()
{
    QBENCHMARK {
        QScriptValue val(123.0);
    }
}

void tst_QScriptValue::numberConstructor()
{
    QBENCHMARK {
        (void)QScriptValue(123);
    }
}

void tst_QScriptValue::stringConstructor()
{
    QString str = QString::fromLatin1("ciao");
    QBENCHMARK {
        (void)QScriptValue(str);
    }
}

void tst_QScriptValue::nullConstructor()
{
    QBENCHMARK {
        QScriptValue val(QScriptValue::NullValue);
    }
}

void tst_QScriptValue::undefinedConstructor()
{
    QBENCHMARK {
        QScriptValue val(QScriptValue::UndefinedValue);
    }
}

void tst_QScriptValue::boolConstructorWithEngine()
{
    newEngine();
    QBENCHMARK {
        QScriptValue val(m_engine, true);
    }
}

void tst_QScriptValue::floatConstructorWithEngine()
{
    newEngine();
    QBENCHMARK {
        QScriptValue val(m_engine, 123.0);
    }
}

void tst_QScriptValue::intConstructorWithEngine()
{
    newEngine();
    QBENCHMARK {
        (void)QScriptValue(m_engine, 123);
    }
}

void tst_QScriptValue::stringConstructorWithEngine()
{
    newEngine();
    QString str = QString::fromLatin1("ciao");
    QBENCHMARK {
        (void)QScriptValue(m_engine, str);
    }
}

void tst_QScriptValue::nullConstructorWithEngine()
{
    newEngine();
    QBENCHMARK {
        QScriptValue val(m_engine, QScriptValue::NullValue);
    }
}

void tst_QScriptValue::undefinedConstructorWithEngine()
{
    newEngine();
    QBENCHMARK {
        QScriptValue val(m_engine, QScriptValue::UndefinedValue);
    }
}

void tst_QScriptValue::copyConstructor_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::copyConstructor()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        QScriptValue copy(val);
    }
}

void tst_QScriptValue::call_data()
{
    newEngine();
    QTest::addColumn<QString>("code");
    QTest::newRow("empty function") << QString::fromLatin1("(function(){})");
    QTest::newRow("function returning number") << QString::fromLatin1("(function(){ return 123; })");
    QTest::newRow("closure") << QString::fromLatin1("(function(a, b){ return function() { return a + b; }; })(1, 2)");
}

void tst_QScriptValue::call()
{
    QFETCH(QString, code);
    QScriptValue fun = m_engine->evaluate(code);
    QVERIFY(fun.isFunction());
    QBENCHMARK {
        (void)fun.call();
    }
}

void tst_QScriptValue::construct_data()
{
    newEngine();
    QTest::addColumn<QString>("code");
    QTest::newRow("empty function") << QString::fromLatin1("(function(){})");
    QTest::newRow("simple constructor") << QString::fromLatin1("(function(){ this.x = 10; this.y = 20; })");
}

void tst_QScriptValue::construct()
{
    QFETCH(QString, code);
    QScriptValue fun = m_engine->evaluate(code);
    QVERIFY(fun.isFunction());
    QBENCHMARK {
        (void)fun.construct();
    }
}

void tst_QScriptValue::data()
{
    newEngine();
    QScriptValue obj = m_engine->newObject();
    obj.setData(QScriptValue(m_engine, 123));
    QBENCHMARK {
        obj.data();
    }
}

void tst_QScriptValue::setData()
{
    newEngine();
    QScriptValue obj = m_engine->newObject();
    QScriptValue val(m_engine, 123);
    QBENCHMARK {
        obj.setData(val);
    }
}

void tst_QScriptValue::data_noData_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::data_noData()
{
    QFETCH(QScriptValue, val);
    QVERIFY(!val.data().isValid());
    QBENCHMARK {
        val.data();
    }
}

void tst_QScriptValue::engine_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::engine()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.engine();
    }
}

void tst_QScriptValue::equalsSelf_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::equalsSelf()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.equals(val);
    }
}

void tst_QScriptValue::lessThanSelf_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::lessThanSelf()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.lessThan(val);
    }
}

void tst_QScriptValue::strictlyEqualsSelf_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::strictlyEqualsSelf()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.strictlyEquals(val);
    }
}

void tst_QScriptValue::instanceOf()
{
    newEngine();
    QScriptValue arrayCtor = m_engine->globalObject().property("Array");
    QScriptValue array = arrayCtor.construct();
    QVERIFY(array.instanceOf(arrayCtor));
    QBENCHMARK {
        array.instanceOf(arrayCtor);
    }
}

void tst_QScriptValue::isArray_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isArray()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isArray();
    }
}

void tst_QScriptValue::isBool_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isBool()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isBool();
    }
}

void tst_QScriptValue::isDate_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isDate()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isDate();
    }
}

void tst_QScriptValue::isError_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isError()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isError();
    }
}

void tst_QScriptValue::isFunction_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isFunction()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isFunction();
    }
}

void tst_QScriptValue::isNull_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isNull()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isNull();
    }
}

void tst_QScriptValue::isNumber_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isNumber()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isNumber();
    }
}

void tst_QScriptValue::isObject_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isObject()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isObject();
    }
}

void tst_QScriptValue::isQMetaObject_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isQMetaObject()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isQMetaObject();
    }
}

void tst_QScriptValue::isQObject_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isQObject()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isQObject();
    }
}

void tst_QScriptValue::isRegExp_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isRegExp()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isRegExp();
    }
}

void tst_QScriptValue::isString_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isString()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isString();
    }
}

void tst_QScriptValue::isUndefined_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isUndefined()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isUndefined();
    }
}

void tst_QScriptValue::isValid_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isValid()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isValid();
    }
}

void tst_QScriptValue::isVariant_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::isVariant()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.isVariant();
    }
}

void tst_QScriptValue::toBool_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toBool()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.toBool();
    }
}

void tst_QScriptValue::toDateTime_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toDateTime()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.toDateTime();
    }
}

void tst_QScriptValue::toInt32_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toInt32()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.toInt32();
    }
}

void tst_QScriptValue::toInteger_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toInteger()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.toInteger();
    }
}

void tst_QScriptValue::toNumber_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toNumber()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.toNumber();
    }
}

void tst_QScriptValue::toRegExp_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toRegExp()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.toRegExp();
    }
}

void tst_QScriptValue::toString_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toString()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        (void)val.toString();
    }
}

void tst_QScriptValue::toQMetaObject_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toQMetaObject()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.toQMetaObject();
    }
}

void tst_QScriptValue::toQObject_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toQObject()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        (void)val.toQObject();
    }
}

void tst_QScriptValue::toUInt16_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toUInt16()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.toUInt16();
    }
}

void tst_QScriptValue::toUInt32_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toUInt32()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.toUInt32();
    }
}

void tst_QScriptValue::toVariant_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::toVariant()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.toVariant();
    }
}
void tst_QScriptValue::property_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<bool>("create");
    QTest::newRow("foo") << QString::fromLatin1("foo") << true;
    QTest::newRow("hasOwnProperty") << QString::fromLatin1("hasOwnProperty") << false; // From Object.prototype.
    QTest::newRow("noSuchProperty") << QString::fromLatin1("noSuchProperty") << false;
}

void tst_QScriptValue::property()
{
    QFETCH(QString, propertyName);
    QFETCH(bool, create);
    newEngine();
    QScriptValue obj = m_engine->newObject();
    if (create)
        obj.setProperty(propertyName, 123);
    QBENCHMARK {
        (void)obj.property(propertyName);
    }
}

void tst_QScriptValue::propertyById_data()
{
    property_data();
}

void tst_QScriptValue::propertyById()
{
    QFETCH(QString, propertyName);
    QFETCH(bool, create);
    newEngine();
    QScriptValue obj = m_engine->newObject();
    QScriptString id = m_engine->toStringHandle(propertyName);
    if (create)
        obj.setProperty(id, 123);
    QBENCHMARK {
        obj.property(id);
    }
}

void tst_QScriptValue::propertyByIndex()
{
    newEngine();
    QScriptValue obj = m_engine->newObject();
    obj.setProperty(123, 456);
    QBENCHMARK {
        obj.property(123);
    }
}

void tst_QScriptValue::setProperty_data()
{
    newEngine();
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QScriptValue>("val");
    QTest::newRow("foo") << QString::fromLatin1("foo") << QScriptValue(123);
    QTest::newRow("bar") << QString::fromLatin1("bar") << QScriptValue(m_engine, 123);
    QTest::newRow("baz") << QString::fromLatin1("baz") << QScriptValue();
    QTest::newRow("toString") << QString::fromLatin1("toString") << QScriptValue(m_engine, true);
}

void tst_QScriptValue::setProperty()
{
    QFETCH(QString, propertyName);
    QFETCH(QScriptValue, val);
    QScriptValue obj = m_engine->newObject();
    QBENCHMARK {
        obj.setProperty(propertyName, val);
    }
}

void tst_QScriptValue::setPropertyById_data()
{
    setProperty_data();
}

void tst_QScriptValue::setPropertyById()
{
    QFETCH(QString, propertyName);
    QFETCH(QScriptValue, val);
    QScriptValue obj = m_engine->newObject();
    QScriptString id = m_engine->toStringHandle(propertyName);
    QBENCHMARK {
        obj.setProperty(id, val);
    }
}

void tst_QScriptValue::setPropertyByIndex()
{
    newEngine();
    QScriptValue obj = m_engine->newObject();
    QScriptValue val(456);
    QBENCHMARK {
        obj.setProperty(123, 456);
    }
}

void tst_QScriptValue::propertyFlags_data()
{
    property_data();
}

void tst_QScriptValue::propertyFlags()
{
    QFETCH(QString, propertyName);
    QFETCH(bool, create);
    newEngine();
    QScriptValue obj = m_engine->newObject();
    if (create)
        obj.setProperty(propertyName, 123, QScriptValue::SkipInEnumeration | QScriptValue::ReadOnly);
    QBENCHMARK {
        (void)obj.propertyFlags(propertyName);
    }
}

void tst_QScriptValue::propertyFlagsById_data()
{
    propertyFlags_data();
}

void tst_QScriptValue::propertyFlagsById()
{
    QFETCH(QString, propertyName);
    QFETCH(bool, create);
    newEngine();
    QScriptValue obj = m_engine->newObject();
    QScriptString id = m_engine->toStringHandle(propertyName);
    if (create)
        obj.setProperty(id, 123, QScriptValue::SkipInEnumeration | QScriptValue::ReadOnly);
    QBENCHMARK {
        obj.propertyFlags(id);
    }
}

void tst_QScriptValue::prototype_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::prototype()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.prototype();
    }
}

void tst_QScriptValue::setPrototype()
{
    newEngine();
    QScriptValue obj = m_engine->newObject();
    QScriptValue proto = m_engine->newObject();
    QBENCHMARK {
        obj.setPrototype(proto);
    }
}

void tst_QScriptValue::scriptClass_data()
{
    defineStandardTestValues();
}

void tst_QScriptValue::scriptClass()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        val.scriptClass();
    }
}

void tst_QScriptValue::setScriptClass()
{
    newEngine();
    QScriptValue obj = m_engine->newObject();
    QScriptClass cls(m_engine);
    QBENCHMARK {
        obj.setScriptClass(&cls);
    }
}

void tst_QScriptValue::readMetaProperty()
{
    newEngine();
    QScriptValue object = m_engine->newQObject(QCoreApplication::instance());
    QScriptString propertyName = m_engine->toStringHandle("objectName");
    QBENCHMARK {
        for (int i = 0; i < 10000; ++i)
            object.property(propertyName);
    }
}

void tst_QScriptValue::writeMetaProperty()
{
    newEngine();
    QScriptValue object = m_engine->newQObject(QCoreApplication::instance());
    QScriptString propertyName = m_engine->toStringHandle("objectName");
    QScriptValue value(m_engine, "foo");
    QBENCHMARK {
        for (int i = 0; i < 10000; ++i)
            object.setProperty(propertyName, value);
    }
}

void tst_QScriptValue::defineStandardTestValues()
{
    newEngine();
    QTest::addColumn<QScriptValue>("val");
    QTest::newRow("bool") << m_engine->evaluate("true");
    QTest::newRow("number") << m_engine->evaluate("123");
    QTest::newRow("string") << m_engine->evaluate("'ciao'");
    QTest::newRow("null") << m_engine->evaluate("null");
    QTest::newRow("undefined") << m_engine->evaluate("undefined");
    QTest::newRow("object") << m_engine->evaluate("({foo:123})");
    QTest::newRow("array") << m_engine->evaluate("[10,20,30]");
    QTest::newRow("function") << m_engine->evaluate("(function foo(a, b, c) { return a + b + c; })");
    QTest::newRow("date") << m_engine->evaluate("new Date");
    QTest::newRow("regexp") << m_engine->evaluate("new RegExp('foo')");
    QTest::newRow("error") << m_engine->evaluate("new Error");

    QTest::newRow("qobject") << m_engine->newQObject(this);
    QTest::newRow("qmetaobject") << m_engine->newQMetaObject(&QScriptEngine::staticMetaObject);
    QTest::newRow("variant") << m_engine->newVariant(123);
    QTest::newRow("qscriptclassobject") << m_engine->newObject(new QScriptClass(m_engine));

    QTest::newRow("invalid") << QScriptValue();
    QTest::newRow("bool-no-engine") << QScriptValue(true);
    QTest::newRow("number-no-engine") << QScriptValue(123.0);
    QTest::newRow("string-no-engine") << QScriptValue(QString::fromLatin1("hello"));
    QTest::newRow("null-no-engine") << QScriptValue(QScriptValue::NullValue);
    QTest::newRow("undefined-no-engine") << QScriptValue(QScriptValue::UndefinedValue);
}

QTEST_MAIN(tst_QScriptValue)
#include "tst_qscriptvalue.moc"
