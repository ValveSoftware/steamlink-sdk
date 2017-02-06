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

#include <qtest.h>
#include <QtQml/qjsvalue.h>
#include <QtQml/qjsengine.h>

class tst_QJSValue : public QObject
{
    Q_OBJECT

public:
    tst_QJSValue();
    virtual ~tst_QJSValue();

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
    void undefinedConstructorWithEngine();
    void copyConstructor_data();
    void copyConstructor();
    void call_data();
    void call();
    void construct_data();
    void construct();
#if 0 // no data
    void data();
    void setData();
    void data_noData_data();
    void data_noData();
#endif
    void equalsSelf_data();
    void equalsSelf();
#if 0 // no less then
    void lessThanSelf_data();
    void lessThanSelf();
#endif
    void strictlyEqualsSelf_data();
    void strictlyEqualsSelf();
    void isArray_data();
    void isArray();
    void isBool_data();
    void isBool();
    void isDate_data();
    void isDate();
    void isError_data();
    void isError();
    void isCallable_data();
    void isCallable();
    void isNull_data();
    void isNull();
    void isNumber_data();
    void isNumber();
    void isObject_data();
    void isObject();
#if 0 // no qmetaobject
    void isQMetaObject_data();
    void isQMetaObject();
#endif
    void isQObject_data();
    void isQObject();
    void isRegExp_data();
    void isRegExp();
    void isString_data();
    void isString();
    void isUndefined_data();
    void isUndefined();
    void isVariant_data();
    void isVariant();
    void toBool_data();
    void toBool();
    void toDateTime_data();
    void toDateTime();
    void toInt_data();
    void toInt();
    void toNumber_data();
    void toNumber();
    void toRegExp_data();
    void toRegExp();
    void toString_data();
    void toString();
    void toUInt_data();
    void toUInt();
#if 0 // no qmetaobject
    void toQMetaObject_data();
    void toQMetaObject();
#endif
    void toQObject_data();
    void toQObject();
    void toVariant_data();
    void toVariant();
    void property_data();
    void property();
#if 0  // no string handle
    void propertyById_data();
    void propertyById();
#endif
    void propertyByIndex();
    void setProperty_data();
    void setProperty();
#if 0  // no string handle
    void setPropertyById_data();
    void setPropertyById();
#endif
    void setPropertyByIndex();
#if 0 // no propertyFlags for now
    void propertyFlags_data();
    void propertyFlags();
    void propertyFlagsById_data();
    void propertyFlagsById();
#endif
    void prototype_data();
    void prototype();
    void setPrototype();
#if 0 // no script class
    void scriptClass_data();
    void scriptClass();
    void setScriptClass();
#endif
#if 0 // no string handle
    void readMetaProperty();
    void writeMetaProperty();
#endif

private:
    void defineStandardTestValues();
    void newEngine()
    {
        delete m_engine;
        m_engine = new QJSEngine;
    }

    QJSEngine *m_engine;
};

tst_QJSValue::tst_QJSValue()
    : m_engine(0)
{
}

tst_QJSValue::~tst_QJSValue()
{
    delete m_engine;
}

void tst_QJSValue::init()
{
}

void tst_QJSValue::cleanup()
{
}

void tst_QJSValue::boolConstructor()
{
    QBENCHMARK {
        QJSValue val(true);
    }
}

void tst_QJSValue::floatConstructor()
{
    QBENCHMARK {
        QJSValue val(123.0);
    }
}

void tst_QJSValue::numberConstructor()
{
    QBENCHMARK {
        (void)QJSValue(123);
    }
}

void tst_QJSValue::stringConstructor()
{
    QString str = QString::fromLatin1("ciao");
    QBENCHMARK {
        (void)QJSValue(str);
    }
}

void tst_QJSValue::nullConstructor()
{
    QBENCHMARK {
        QJSValue val(QJSValue::NullValue);
    }
}

void tst_QJSValue::undefinedConstructor()
{
    QBENCHMARK {
        QJSValue val(QJSValue::UndefinedValue);
    }
}

void tst_QJSValue::boolConstructorWithEngine()
{
    newEngine();
    QBENCHMARK {
        m_engine->toScriptValue(true);
    }
}

void tst_QJSValue::floatConstructorWithEngine()
{
    newEngine();
    QBENCHMARK {
        m_engine->toScriptValue(123.0);
    }
}

void tst_QJSValue::intConstructorWithEngine()
{
    newEngine();
    QBENCHMARK {
        m_engine->toScriptValue(123);
    }
}

void tst_QJSValue::stringConstructorWithEngine()
{
    newEngine();
    QString str = QString::fromLatin1("ciao");
    QBENCHMARK {
        m_engine->toScriptValue(str);
    }
}

void tst_QJSValue::undefinedConstructorWithEngine()
{
    newEngine();
    QVariant var;
    QBENCHMARK {
        m_engine->toScriptValue(var);
    }
}

void tst_QJSValue::copyConstructor_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::copyConstructor()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        QJSValue copy(val);
    }
}

void tst_QJSValue::call_data()
{
    newEngine();
    QTest::addColumn<QString>("code");
    QTest::newRow("empty function") << QString::fromLatin1("(function(){})");
    QTest::newRow("function returning number") << QString::fromLatin1("(function(){ return 123; })");
    QTest::newRow("closure") << QString::fromLatin1("(function(a, b){ return function() { return a + b; }; })(1, 2)");
}

void tst_QJSValue::call()
{
    QFETCH(QString, code);
    QJSValue fun = m_engine->evaluate(code);
    QVERIFY(fun.isCallable());
    QBENCHMARK {
        (void)fun.call();
    }
}

void tst_QJSValue::construct_data()
{
    newEngine();
    QTest::addColumn<QString>("code");
    QTest::newRow("empty function") << QString::fromLatin1("(function(){})");
    QTest::newRow("simple constructor") << QString::fromLatin1("(function(){ this.x = 10; this.y = 20; })");
}

void tst_QJSValue::construct()
{
    QFETCH(QString, code);
    QJSValue fun = m_engine->evaluate(code);
    QVERIFY(fun.isCallable());
    QBENCHMARK {
        (void)fun.callAsConstructor();
    }
}

#if 0
void tst_QJSValue::data()
{
    newEngine();
    QJSValue obj = m_engine->newObject();
    obj.setData(QJSValue(m_engine, 123));
    QBENCHMARK {
        obj.data();
    }
}

void tst_QJSValue::setData()
{
    newEngine();
    QJSValue obj = m_engine->newObject();
    QJSValue val(m_engine, 123);
    QBENCHMARK {
        obj.setData(val);
    }
}

void tst_QJSValue::data_noData_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::data_noData()
{
    QFETCH(QJSValue, val);
    QVERIFY(!val.data().isValid());
    QBENCHMARK {
        val.data();
    }
}
#endif

void tst_QJSValue::equalsSelf_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::equalsSelf()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.equals(val);
    }
}

#if 0
void tst_QJSValue::lessThanSelf_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::lessThanSelf()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.lessThan(val);
    }
}
#endif

void tst_QJSValue::strictlyEqualsSelf_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::strictlyEqualsSelf()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.strictlyEquals(val);
    }
}

void tst_QJSValue::isArray_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isArray()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isArray();
    }
}

void tst_QJSValue::isBool_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isBool()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isBool();
    }
}

void tst_QJSValue::isDate_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isDate()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isDate();
    }
}

void tst_QJSValue::isError_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isError()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isError();
    }
}

void tst_QJSValue::isCallable_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isCallable()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isCallable();
    }
}

void tst_QJSValue::isNull_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isNull()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isNull();
    }
}

void tst_QJSValue::isNumber_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isNumber()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isNumber();
    }
}

void tst_QJSValue::isObject_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isObject()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isObject();
    }
}

#if 0
void tst_QJSValue::isQMetaObject_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isQMetaObject()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isQMetaObject();
    }
}
#endif

void tst_QJSValue::isQObject_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isQObject()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isQObject();
    }
}

void tst_QJSValue::isRegExp_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isRegExp()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isRegExp();
    }
}

void tst_QJSValue::isString_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isString()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isString();
    }
}

void tst_QJSValue::isUndefined_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isUndefined()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isUndefined();
    }
}

void tst_QJSValue::isVariant_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::isVariant()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.isVariant();
    }
}

void tst_QJSValue::toBool_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::toBool()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.toBool();
    }
}

void tst_QJSValue::toDateTime_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::toDateTime()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.toDateTime();
    }
}

void tst_QJSValue::toInt_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::toInt()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.toInt();
    }
}

void tst_QJSValue::toNumber_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::toNumber()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.toNumber();
    }
}

void tst_QJSValue::toRegExp_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::toRegExp()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        qjsvalue_cast<QRegExp>(val);
    }
}

void tst_QJSValue::toString_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::toString()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        (void)val.toString();
    }
}

#if 0
void tst_QJSValue::toQMetaObject_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::toQMetaObject()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.toQMetaObject();
    }
}
#endif

void tst_QJSValue::toQObject_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::toQObject()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        (void)val.toQObject();
    }
}

void tst_QJSValue::toUInt_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::toUInt()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.toUInt();
    }
}

void tst_QJSValue::toVariant_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::toVariant()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.toVariant();
    }
}
void tst_QJSValue::property_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<bool>("create");
    QTest::newRow("foo") << QString::fromLatin1("foo") << true;
    QTest::newRow("hasOwnProperty") << QString::fromLatin1("hasOwnProperty") << false; // From Object.prototype.
    QTest::newRow("noSuchProperty") << QString::fromLatin1("noSuchProperty") << false;
}

void tst_QJSValue::property()
{
    QFETCH(QString, propertyName);
    QFETCH(bool, create);
    newEngine();
    QJSValue obj = m_engine->newObject();
    if (create)
        obj.setProperty(propertyName, 123);
    QBENCHMARK {
        (void)obj.property(propertyName);
    }
}

#if 0
void tst_QJSValue::propertyById_data()
{
    property_data();
}

void tst_QJSValue::propertyById()
{
    QFETCH(QString, propertyName);
    QFETCH(bool, create);
    newEngine();
    QJSValue obj = m_engine->newObject();
    QJSString id = m_engine->toStringHandle(propertyName);
    if (create)
        obj.setProperty(id, 123);
    QBENCHMARK {
        obj.property(id);
    }
}
#endif

void tst_QJSValue::propertyByIndex()
{
    newEngine();
    QJSValue obj = m_engine->newObject();
    obj.setProperty(123, 456);
    QBENCHMARK {
        obj.property(123);
    }
}

void tst_QJSValue::setProperty_data()
{
    newEngine();
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QJSValue>("val");
    QTest::newRow("foo") << QString::fromLatin1("foo") << QJSValue(123);
    QTest::newRow("bar") << QString::fromLatin1("bar") << m_engine->toScriptValue(123);
    QTest::newRow("baz") << QString::fromLatin1("baz") << QJSValue();
    QTest::newRow("toString") << QString::fromLatin1("toString") << m_engine->toScriptValue(true);
}

void tst_QJSValue::setProperty()
{
    QFETCH(QString, propertyName);
    QFETCH(QJSValue, val);
    QJSValue obj = m_engine->newObject();
    QBENCHMARK {
        obj.setProperty(propertyName, val);
    }
}

#if 0
void tst_QJSValue::setPropertyById_data()
{
    setProperty_data();
}

void tst_QJSValue::setPropertyById()
{
    QFETCH(QString, propertyName);
    QFETCH(QJSValue, val);
    QJSValue obj = m_engine->newObject();
    QJSString id = m_engine->toStringHandle(propertyName);
    QBENCHMARK {
        obj.setProperty(id, val);
    }
}
#endif

void tst_QJSValue::setPropertyByIndex()
{
    newEngine();
    QJSValue obj = m_engine->newObject();
    QJSValue val(456);
    QBENCHMARK {
        obj.setProperty(123, 456);
    }
}

#if 0
void tst_QJSValue::propertyFlags_data()
{
    property_data();
}

void tst_QJSValue::propertyFlags()
{
    QFETCH(QString, propertyName);
    QFETCH(bool, create);
    newEngine();
    QJSValue obj = m_engine->newObject();
    if (create)
        obj.setProperty(propertyName, 123, QJSValue::SkipInEnumeration | QJSValue::ReadOnly);
    QBENCHMARK {
        (void)obj.propertyFlags(propertyName);
    }
}

void tst_QJSValue::propertyFlagsById_data()
{
    propertyFlags_data();
}

void tst_QJSValue::propertyFlagsById()
{
    QFETCH(QString, propertyName);
    QFETCH(bool, create);
    newEngine();
    QJSValue obj = m_engine->newObject();
    QJSString id = m_engine->toStringHandle(propertyName);
    if (create)
        obj.setProperty(id, 123, QJSValue::SkipInEnumeration | QJSValue::ReadOnly);
    QBENCHMARK {
        obj.propertyFlags(id);
    }
}
#endif

void tst_QJSValue::prototype_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::prototype()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.prototype();
    }
}

void tst_QJSValue::setPrototype()
{
    newEngine();
    QJSValue obj = m_engine->newObject();
    QJSValue proto = m_engine->newObject();
    QBENCHMARK {
        obj.setPrototype(proto);
    }
}

#if 0
void tst_QJSValue::scriptClass_data()
{
    defineStandardTestValues();
}

void tst_QJSValue::scriptClass()
{
    QFETCH(QJSValue, val);
    QBENCHMARK {
        val.scriptClass();
    }
}

void tst_QJSValue::setScriptClass()
{
    newEngine();
    QJSValue obj = m_engine->newObject();
    QJSClass cls(m_engine);
    QBENCHMARK {
        obj.setScriptClass(&cls);
    }
}

void tst_QJSValue::readMetaProperty()
{
    newEngine();
    QJSValue object = m_engine->newQObject(QCoreApplication::instance());
    QJSString propertyName = m_engine->toStringHandle("objectName");
    QBENCHMARK {
        for (int i = 0; i < 10000; ++i)
            object.property(propertyName);
    }
}

void tst_QJSValue::writeMetaProperty()
{
    newEngine();
    QJSValue object = m_engine->newQObject(QCoreApplication::instance());
    QJSString propertyName = m_engine->toStringHandle("objectName");
    QJSValue value(m_engine, "foo");
    QBENCHMARK {
        for (int i = 0; i < 10000; ++i)
            object.setProperty(propertyName, value);
    }
}
#endif

void tst_QJSValue::defineStandardTestValues()
{
    newEngine();
    QTest::addColumn<QJSValue>("val");
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
#if 0 // no qmetaobject
    QTest::newRow("qmetaobject") << m_engine->newQMetaObject(&QJSEngine::staticMetaObject);
#endif
    QTest::newRow("variant") << m_engine->toScriptValue(QPoint(10, 20));
#if 0 // no classes
    QTest::newRow("qscriptclassobject") << m_engine->newObject(new QJSClass(m_engine));
#endif

    QTest::newRow("invalid") << QJSValue();
    QTest::newRow("bool-no-engine") << QJSValue(true);
    QTest::newRow("number-no-engine") << QJSValue(123.0);
    QTest::newRow("string-no-engine") << QJSValue(QString::fromLatin1("hello"));
    QTest::newRow("null-no-engine") << QJSValue(QJSValue::NullValue);
    QTest::newRow("undefined-no-engine") << QJSValue(QJSValue::UndefinedValue);
}

QTEST_MAIN(tst_QJSValue)
#include "tst_qjsvalue.moc"
