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
#include <QtWidgets>
#include <QtScript>

#define ITERATION_COUNT 1e4

struct CustomType
{
    int a;
};
Q_DECLARE_METATYPE(CustomType)

class PropertyTestObject : public QObject
{
    Q_OBJECT
    Q_ENUMS(EnumType)
    Q_FLAGS(FlagsType)
    Q_PROPERTY(bool boolProperty READ boolProperty WRITE setBoolProperty)
    Q_PROPERTY(int intProperty READ intProperty WRITE setIntProperty)
    Q_PROPERTY(double doubleProperty READ doubleProperty WRITE setDoubleProperty)
    Q_PROPERTY(QString stringProperty READ stringProperty WRITE setStringProperty)
    Q_PROPERTY(QVariant variantProperty READ variantProperty WRITE setVariantProperty)
    Q_PROPERTY(QObject* qobjectProperty READ qobjectProperty WRITE setQObjectProperty)
    Q_PROPERTY(CustomType customProperty READ customProperty WRITE setCustomProperty)
    Q_PROPERTY(EnumType enumProperty READ enumProperty WRITE setEnumProperty)
    Q_PROPERTY(FlagsType flagsProperty READ flagsProperty WRITE setFlagsProperty)
public:
    enum EnumType {
        NoEnumValue = 0,
        FirstEnumValue = 1,
        SecondEnumValue = 2,
        ThirdEnumValue = 4
    };
    Q_DECLARE_FLAGS(FlagsType, EnumType)

    PropertyTestObject(QObject *parent = 0)
        : QObject(parent),
        m_boolProperty(false),
        m_intProperty(123),
        m_doubleProperty(123),
        m_stringProperty("hello"),
        m_variantProperty(double(123)),
        m_qobjectProperty(this),
        m_enumProperty(SecondEnumValue),
        m_flagsProperty(FirstEnumValue | ThirdEnumValue)
        { }

    bool boolProperty() const
        { return m_boolProperty; }
    void setBoolProperty(bool value)
        { m_boolProperty = value; }

    int intProperty() const
        { return m_intProperty; }
    void setIntProperty(int value)
        { m_intProperty = value; }

    int doubleProperty() const
        { return m_doubleProperty; }
    void setDoubleProperty(double value)
        { m_doubleProperty = value; }

    QString stringProperty() const
        { return m_stringProperty; }
    void setStringProperty(const QString &value)
        { m_stringProperty = value; }

    QVariant variantProperty() const
        { return m_variantProperty; }
    void setVariantProperty(const QVariant &value)
        { m_variantProperty = value; }

    QObject *qobjectProperty() const
        { return m_qobjectProperty; }
    void setQObjectProperty(QObject *qobject)
        { m_qobjectProperty = qobject;  }

    CustomType customProperty() const
        { return m_customProperty; }
    void setCustomProperty(const CustomType &value)
        { m_customProperty = value; }

    EnumType enumProperty() const
        { return m_enumProperty; }
    void setEnumProperty(EnumType value)
        { m_enumProperty = value; }

    FlagsType flagsProperty() const
        { return m_flagsProperty; }
    void setFlagsProperty(FlagsType value)
        { m_flagsProperty = value; }

private:
    bool m_boolProperty;
    int m_intProperty;
    double m_doubleProperty;
    QString m_stringProperty;
    QVariant m_variantProperty;
    QObject *m_qobjectProperty;
    CustomType m_customProperty;
    EnumType m_enumProperty;
    FlagsType m_flagsProperty;
};

class SlotTestObject : public QObject
{
    Q_OBJECT
public:
    SlotTestObject(QObject *parent = 0)
        : QObject(parent),
          m_string(QString::fromLatin1("hello")),
          m_variant(123)
        { }

public Q_SLOTS:
    void voidSlot() { }
    void boolSlot(bool) { }
    void intSlot(int) { }
    void doubleSlot(double) { }
    void stringSlot(const QString &) { }
    void variantSlot(const QVariant &) { }
    void qobjectSlot(QObject *) { }
    void customTypeSlot(const CustomType &) { }

    bool returnBoolSlot() { return true; }
    int returnIntSlot() { return 123; }
    double returnDoubleSlot() { return 123.0; }
    QString returnStringSlot() { return m_string; }
    QVariant returnVariantSlot() { return m_variant; }
    QObject *returnQObjectSlot() { return this; }
    CustomType returnCustomTypeSlot() { return m_custom; }

    void fourDoubleSlot(double, double, double, double) { }
    void sixDoubleSlot(double, double, double, double, double, double) { }
    void eightDoubleSlot(double, double, double, double, double, double, double, double) { }

    void fourStringSlot(const QString &, const QString &, const QString &, const QString &) { }
    void sixStringSlot(const QString &, const QString &, const QString &, const QString &,
                       const QString &, const QString &) { }
    void eightStringSlot(const QString &, const QString &, const QString &, const QString &,
        const QString &, const QString &, const QString &, const QString &) { }

private:
    QString m_string;
    QVariant m_variant;
    CustomType m_custom;
};

class SignalTestObject : public QObject
{
    Q_OBJECT
public:
    SignalTestObject(QObject *parent = 0)
        : QObject(parent)
        { }

    void emitVoidSignal()
        { emit voidSignal(); }
    void emitBoolSignal(bool value)
        { emit boolSignal(value); }
    void emitIntSignal(int value)
        { emit intSignal(value); }
    void emitDoubleSignal(double value)
        { emit doubleSignal(value); }
    void emitStringSignal(const QString &value)
        { emit stringSignal(value); }
    void emitVariantSignal(const QVariant &value)
        { emit variantSignal(value); }
    void emitQObjectSignal(QObject *object)
        { emit qobjectSignal(object); }
    void emitCustomTypeSignal(const CustomType &value)
        { emit customTypeSignal(value); }

Q_SIGNALS:
    void voidSignal();
    void boolSignal(bool);
    void intSignal(int);
    void doubleSignal(double);
    void stringSignal(const QString &);
    void variantSignal(const QVariant &);
    void qobjectSignal(QObject *);
    void customTypeSignal(const CustomType &);
};

class OverloadedSlotTestObject : public QObject
{
    Q_OBJECT
public:
    OverloadedSlotTestObject(QObject *parent = 0)
        : QObject(parent)
        { }

public Q_SLOTS:
    void overloadedSlot() { }
    void overloadedSlot(bool) { }
    void overloadedSlot(double) { }
    void overloadedSlot(const QString &) { }
};

class QtScriptablePropertyTestObject
    : public PropertyTestObject, public QScriptable
{
};

class QtScriptableSlotTestObject
    : public SlotTestObject, public QScriptable
{
};

class tst_QScriptQObject : public QObject
{
    Q_OBJECT

public:
    tst_QScriptQObject();
    virtual ~tst_QScriptQObject();

private slots:
    void initTestCase();

    void readMetaProperty_data();
    void readMetaProperty();

    void writeMetaProperty_data();
    void writeMetaProperty();

    void readDynamicProperty_data();
    void readDynamicProperty();

    void writeDynamicProperty_data();
    void writeDynamicProperty();

    void readMethodByName_data();
    void readMethodByName();

    void readMethodBySignature_data();
    void readMethodBySignature();

    void readChild_data();
    void readChild();

    void readOneOfManyChildren_data();
    void readOneOfManyChildren();

    void readPrototypeProperty_data();
    void readPrototypeProperty();

    void readScriptProperty_data();
    void readScriptProperty();

    void readNoSuchProperty_data();
    void readNoSuchProperty();

    void readAllMetaProperties();

    void callSlot_data();
    void callSlot();

    void callOverloadedSlot_data();
    void callOverloadedSlot();

    void voidSignalHandler();
    void boolSignalHandler();
    void intSignalHandler();
    void doubleSignalHandler();
    void stringSignalHandler();
    void variantSignalHandler();
    void qobjectSignalHandler();
    void customTypeSignalHandler();

    void emitSignal_data();
    void emitSignal();

    void readButtonMetaProperty_data();
    void readButtonMetaProperty();

    void writeButtonMetaProperty_data();
    void writeButtonMetaProperty();

    void readDynamicButtonProperty_data();
    void readDynamicButtonProperty();

    void writeDynamicButtonProperty_data();
    void writeDynamicButtonProperty();

    void readButtonMethodByName_data();
    void readButtonMethodByName();

    void readButtonMethodBySignature_data();
    void readButtonMethodBySignature();

    void readButtonChild_data();
    void readButtonChild();

    void readButtonPrototypeProperty_data();
    void readButtonPrototypeProperty();

    void readButtonScriptProperty_data();
    void readButtonScriptProperty();

    void readNoSuchButtonProperty_data();
    void readNoSuchButtonProperty();

    void callButtonMethod_data();
    void callButtonMethod();

    void readAllButtonMetaProperties();

    void readQScriptableMetaProperty_data();
    void readQScriptableMetaProperty();

    void writeQScriptableMetaProperty_data();
    void writeQScriptableMetaProperty();

    void callQScriptableSlot_data();
    void callQScriptableSlot();

private:
    void readMetaProperty_dataHelper(const QMetaObject *mo);
    void readMethodByName_dataHelper(const QMetaObject *mo);
    void readMethodBySignature_dataHelper(const QMetaObject *mo);
    void readAllMetaPropertiesHelper(QObject *o);

    void readPropertyHelper(QScriptEngine &engine, const QScriptValue &object,
        const QString &propertyName, const QString &argTemplate = ".%0");
    void writePropertyHelper(QScriptEngine &engine, const QScriptValue &object,
        const QString &propertyName, const QScriptValue &value,
        const QString &argTemplate = ".%0");

    void callMethodHelper(QScriptEngine &engine, QObject *object,
                          const QString &propertyName, const QString &arguments);
    void signalHandlerHelper(QScriptEngine &engine, QObject *object, const char *signal);
};

tst_QScriptQObject::tst_QScriptQObject()
{
}

tst_QScriptQObject::~tst_QScriptQObject()
{
}

void tst_QScriptQObject::initTestCase()
{
    qMetaTypeId<CustomType>();
}

void tst_QScriptQObject::readMetaProperty_dataHelper(const QMetaObject *mo)
{
    QTest::addColumn<QString>("propertyName");

    for (int i = 0; i < mo->propertyCount(); ++i) {
        QMetaProperty prop = mo->property(i);
        if (!qstrcmp(prop.name(), "default"))
            continue; // skip reserved word
        QTest::newRow(prop.name()) << prop.name();
    }
}

void tst_QScriptQObject::readMethodByName_dataHelper(const QMetaObject *mo)
{
    QTest::addColumn<QString>("propertyName");

    QSet<QByteArray> uniqueNames;
    for (int i = 0; i < mo->methodCount(); ++i) {
        QMetaMethod method = mo->method(i);
        if (method.access() == QMetaMethod::Private)
            continue;
        QByteArray signature = method.methodSignature();
        QByteArray name = signature.left(signature.indexOf('('));
        if (uniqueNames.contains(name))
            continue;
        QTest::newRow(name) << QString::fromLatin1(name);
        uniqueNames.insert(name);
    }
}

void tst_QScriptQObject::readMethodBySignature_dataHelper(const QMetaObject *mo)
{
    QTest::addColumn<QString>("propertyName");

    for (int i = 0; i < mo->methodCount(); ++i) {
        QMetaMethod method = mo->method(i);
        if (method.access() == QMetaMethod::Private)
            continue;
        QTest::newRow(method.methodSignature().constData()) << QString::fromLatin1(method.methodSignature().constData());
    }
}

void tst_QScriptQObject::readAllMetaPropertiesHelper(QObject *o)
{
    QString code = QString::fromLatin1(
        "(function() {\n"
        "  for (var i = 0; i < 100; ++i) {\n");
    const QMetaObject *mo = o->metaObject();
    for (int i = 0; i < mo->propertyCount(); ++i) {
        QMetaProperty prop = mo->property(i);
        if (!qstrcmp(prop.name(), "default"))
            continue; // skip reserved word
        code.append(QString::fromLatin1("    this.%0;\n").arg(prop.name()));
    }
    code.append(
        "  }\n"
        "})");

    QScriptEngine engine;
    QScriptValue fun = engine.evaluate(code);
    QVERIFY(fun.isFunction());

    QScriptValue wrapper = engine.newQObject(o);
    QBENCHMARK {
        fun.call(wrapper);
    }
    QVERIFY(!engine.hasUncaughtException());
}

void tst_QScriptQObject::readPropertyHelper(
    QScriptEngine &engine, const QScriptValue &object,
    const QString &propertyName, const QString &argTemplate)
{
    QString code = QString::fromLatin1(
        "(function() {\n"
        "  for (var i = 0; i < %0; ++i)\n"
        "    this%1;\n"
        "})").arg(ITERATION_COUNT).arg(argTemplate.arg(propertyName));
    QScriptValue fun = engine.evaluate(code);
    QVERIFY(fun.isFunction());

    QBENCHMARK {
        fun.call(object);
    }
    QVERIFY(!engine.hasUncaughtException());
}

void tst_QScriptQObject::writePropertyHelper(
    QScriptEngine &engine, const QScriptValue &object,
    const QString &propertyName, const QScriptValue &value,
    const QString &argTemplate)
{
    QVERIFY(value.isValid());
    QString code = QString::fromLatin1(
        "(function(v) {\n"
        "  for (var i = 0; i < %0; ++i)\n"
        "    this%1 = v;\n"
        "})").arg(ITERATION_COUNT).arg(argTemplate.arg(propertyName));
    QScriptValue fun = engine.evaluate(code);
    QVERIFY(fun.isFunction());

    QScriptValueList args;
    args << value;
    QBENCHMARK {
        fun.call(object, args);
    }
    QVERIFY(!engine.hasUncaughtException());
}

void tst_QScriptQObject::callMethodHelper(
    QScriptEngine &engine, QObject *object,
    const QString &propertyName, const QString &arguments)
{
    QScriptValue wrapper = engine.newQObject(object);
    QScriptValue method = wrapper.property(propertyName);
    QVERIFY(method.isFunction());

    // Generate code that calls the function directly; in this way
    // only function call performance is measured, not function lookup
    // as well.
    QString code = QString::fromLatin1(
        "(function(f) {\n"
        "  for (var i = 0; i < %0; ++i)\n"
        "    f(%1);\n"
        "})").arg(ITERATION_COUNT).arg(arguments);
    QScriptValue fun = engine.evaluate(code);
    QVERIFY(fun.isFunction());

    QScriptValueList args;
    args << method;
    QBENCHMARK {
        fun.call(wrapper, args);
    }
    QVERIFY(!engine.hasUncaughtException());
}

void tst_QScriptQObject::signalHandlerHelper(
    QScriptEngine &engine, QObject *object, const char *signal)
{
    QScriptValue handler = engine.evaluate("(function(a) { return a; })");
    QVERIFY(handler.isFunction());
    QVERIFY(qScriptConnect(object, signal, QScriptValue(), handler));
}

void tst_QScriptQObject::readMetaProperty_data()
{
    readMetaProperty_dataHelper(&PropertyTestObject::staticMetaObject);
}

// Reads a meta-object-defined property from JS. The purpose of this
// benchmark is to measure the overhead of reading a property from JS
// compared to calling the property getter directly from C++ without
// introspection or value conversion (that's the fastest we could
// possibly hope to get).
void tst_QScriptQObject::readMetaProperty()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    PropertyTestObject testObject;
    readPropertyHelper(engine, engine.newQObject(&testObject), propertyName);
}

void tst_QScriptQObject::writeMetaProperty_data()
{
    readMetaProperty_data();
}

// Writes a meta-object-defined property from JS. The purpose of this
// benchmark is to measure the overhead of writing a property from JS
// compared to calling the property setter directly from C++ without
// introspection or value conversion (that's the fastest we could
// possibly hope to get).
void tst_QScriptQObject::writeMetaProperty()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    PropertyTestObject testObject;
    QScriptValue wrapper = engine.newQObject(&testObject);
    QScriptValue value = wrapper.property(propertyName);
    writePropertyHelper(engine, wrapper, propertyName, value);
}

void tst_QScriptQObject::readDynamicProperty_data()
{
    QTest::addColumn<QVariant>("value");

    QTest::newRow("bool") << QVariant(false);
    QTest::newRow("int") << QVariant(123);
    QTest::newRow("double") << QVariant(double(123.0));
    QTest::newRow("string") << QVariant(QString::fromLatin1("hello"));
    QTest::newRow("QObject*") << qVariantFromValue((QObject*)this);
    QTest::newRow("CustomType") << qVariantFromValue(CustomType());
}

// Reads a dynamic property from JS. The purpose of this benchmark is
// to measure the overhead of reading a dynamic property from JS
// versus calling QObject::property(aDynamicProperty) directly from
// C++.
void tst_QScriptQObject::readDynamicProperty()
{
    QFETCH(QVariant, value);

    QObject testObject;
    const char *propertyName = "dynamicProperty";
    testObject.setProperty(propertyName, value);
    QVERIFY(testObject.dynamicPropertyNames().contains(propertyName));

    QScriptEngine engine;
    readPropertyHelper(engine, engine.newQObject(&testObject), propertyName);
}

void tst_QScriptQObject::writeDynamicProperty_data()
{
    readDynamicProperty_data();
}

// Writes an existing dynamic property from JS. The purpose of this
// benchmark is to measure the overhead of writing a dynamic property
// from JS versus calling QObject::setProperty(aDynamicProperty,
// aVariant) directly from C++.
void tst_QScriptQObject::writeDynamicProperty()
{
    QFETCH(QVariant, value);

    QObject testObject;
    const char *propertyName = "dynamicProperty";
    testObject.setProperty(propertyName, value);
    QVERIFY(testObject.dynamicPropertyNames().contains(propertyName));

    QScriptEngine engine;
    writePropertyHelper(engine, engine.newQObject(&testObject), propertyName,
                        qScriptValueFromValue(&engine, value));
}

void tst_QScriptQObject::readMethodByName_data()
{
    readMethodByName_dataHelper(&SlotTestObject::staticMetaObject);
}

// Reads a meta-object-defined method from JS by name. The purpose of
// this benchmark is to measure the overhead of resolving a method
// from JS (effectively, creating and returning a JS wrapper function
// object for a C++ method).
void tst_QScriptQObject::readMethodByName()
{
    readMetaProperty();
}

void tst_QScriptQObject::readMethodBySignature_data()
{
    readMethodBySignature_dataHelper(&SlotTestObject::staticMetaObject);
}

// Reads a meta-object-defined method from JS by signature. The
// purpose of this benchmark is to measure the overhead of resolving a
// method from JS (effectively, creating and returning a JS wrapper
// function object for a C++ method).
void tst_QScriptQObject::readMethodBySignature()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    SlotTestObject testObject;
    readPropertyHelper(engine, engine.newQObject(&testObject), propertyName, "['%0']");
}

void tst_QScriptQObject::readChild_data()
{
    QTest::addColumn<QString>("propertyName");

    QTest::newRow("child") << "child";
}

// Reads a child object from JS. The purpose of this benchmark is to
// measure the overhead of reading a child object from JS compared to
// calling e.g. qFindChild() directly from C++, when the test object
// is a plain QObject with only one child.
void tst_QScriptQObject::readChild()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    QObject testObject;
    QObject *child = new QObject(&testObject);
    child->setObjectName(propertyName);
    readPropertyHelper(engine, engine.newQObject(&testObject), propertyName);
}

void tst_QScriptQObject::readOneOfManyChildren_data()
{
    QTest::addColumn<QString>("propertyName");

    QTest::newRow("child0") << "child0";
    QTest::newRow("child50") << "child50";
    QTest::newRow("child99") << "child99";
}

// Reads a child object from JS for an object that has many
// children. The purpose of this benchmark is to measure the overhead
// of reading a child object from JS compared to calling
// e.g. qFindChild() directly from C++.
void tst_QScriptQObject::readOneOfManyChildren()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    QObject testObject;
    for (int i = 0; i < 100; ++i) {
        QObject *child = new QObject(&testObject);
        child->setObjectName(QString::fromLatin1("child%0").arg(i));
    }
    readPropertyHelper(engine, engine.newQObject(&testObject), propertyName);
}

void tst_QScriptQObject::readPrototypeProperty_data()
{
    QTest::addColumn<QString>("propertyName");

    // Inherited from Object.prototype.
    QTest::newRow("hasOwnProperty") << "hasOwnProperty";
    QTest::newRow("isPrototypeOf") << "isPrototypeOf";
    QTest::newRow("propertyIsEnumerable") << "propertyIsEnumerable";
    QTest::newRow("valueOf") << "valueOf";
}

// Reads a property that's inherited from a prototype object. The
// purpose of this benchmark is to measure the overhead of resolving a
// prototype property (i.e., how long it takes the binding to
// determine that the QObject doesn't have the property itself).
void tst_QScriptQObject::readPrototypeProperty()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    PropertyTestObject testObject;
    readPropertyHelper(engine, engine.newQObject(&testObject), propertyName);
}

void tst_QScriptQObject::readScriptProperty_data()
{
    QTest::addColumn<QString>("propertyName");

    QTest::newRow("scriptProperty") << "scriptProperty";
}

// Reads a JS (non-Qt) property of a wrapper object. The purpose of
// this benchmark is to measure the overhead of reading a property
// that only exists on the wrapper object, not on the underlying
// QObject.
void tst_QScriptQObject::readScriptProperty()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    PropertyTestObject testObject;
    QScriptValue wrapper = engine.newQObject(&testObject);
    wrapper.setProperty(propertyName, 123);
    QVERIFY(wrapper.property(propertyName).isValid());
    QVERIFY(!testObject.property(propertyName.toLatin1()).isValid());

    readPropertyHelper(engine, wrapper, propertyName);
}

void tst_QScriptQObject::readNoSuchProperty_data()
{
    QTest::addColumn<QString>("propertyName");

    QTest::newRow("noSuchProperty") << "noSuchProperty";
}

// Reads a non-existing (undefined) property of a wrapper object. The
// purpose of this benchmark is to measure the overhead of reading a
// property that doesn't exist (i.e., how long it takes the binding to
// determine this).
void tst_QScriptQObject::readNoSuchProperty()
{
    readMetaProperty();
}

// Reads all meta-object-defined properties from JS. The purpose of
// this benchmark is to measure the overhead of reading different
// properties in sequence, not just the same one repeatedly (like
// readMetaProperty() does).
void tst_QScriptQObject::readAllMetaProperties()
{
    PropertyTestObject testObject;
    readAllMetaPropertiesHelper(&testObject);
}

void tst_QScriptQObject::callSlot_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QString>("arguments");

    QTest::newRow("voidSlot()") << "voidSlot" << "";

    QTest::newRow("boolSlot(true)") << "boolSlot" << "true";
    QTest::newRow("intSlot(123)") << "intSlot" << "123";
    QTest::newRow("doubleSlot(123)") << "doubleSlot" << "123";
    QTest::newRow("stringSlot('hello')") << "stringSlot" << "'hello'";
    QTest::newRow("variantSlot(123)") << "variantSlot" << "123";
    QTest::newRow("qobjectSlot(this)") << "qobjectSlot" << "this"; // assumes 'this' is a QObject

    QTest::newRow("returnBoolSlot()") << "returnBoolSlot" << "";
    QTest::newRow("returnIntSlot()") << "returnIntSlot" << "";
    QTest::newRow("returnDoubleSlot()") << "returnDoubleSlot" << "";
    QTest::newRow("returnStringSlot()") << "returnStringSlot" << "";
    QTest::newRow("returnVariantSlot()") << "returnVariantSlot" << "";
    QTest::newRow("returnQObjectSlot()") << "returnQObjectSlot" << "";
    QTest::newRow("returnCustomTypeSlot()") << "returnCustomTypeSlot" << "";

    // Implicit conversion.
    QTest::newRow("boolSlot(0)") << "boolSlot" << "0";
    QTest::newRow("intSlot('123')") << "intSlot" << "'123'";
    QTest::newRow("doubleSlot('123')") << "doubleSlot" << "'123'";
    QTest::newRow("stringSlot(123)") << "stringSlot" << "123";

    // Many arguments.
    QTest::newRow("fourDoubleSlot(1,2,3,4)") << "fourDoubleSlot" << "1,2,3,4";
    QTest::newRow("sixDoubleSlot(1,2,3,4,5,6)") << "sixDoubleSlot" << "1,2,3,4,5,6";
    QTest::newRow("eightDoubleSlot(1,2,3,4,5,6,7,8)") << "eightDoubleSlot" << "1,2,3,4,5,6,7,8";

    QTest::newRow("fourStringSlot('a','b','c','d')") << "fourStringSlot" << "'a','b','c','d'";
    QTest::newRow("sixStringSlot('a','b','c','d','e','f')") << "sixStringSlot" << "'a','b','c','d','e','f'";
    QTest::newRow("eightStringSlot('a','b','c','d','e','f','g','h')") << "eightStringSlot" << "'a','b','c','d','e','f','g','h'";
}

// Calls a slot from JS. The purpose of this benchmark is to measure
// the overhead of calling a slot from JS compared to calling the slot
// directly from C++ without introspection or value conversion (that's
// the fastest we could possibly hope to get). The slots themselves
// don't do any work.
void tst_QScriptQObject::callSlot()
{
    QFETCH(QString, propertyName);
    QFETCH(QString, arguments);

    QScriptEngine engine;
    SlotTestObject testObject;
    callMethodHelper(engine, &testObject, propertyName, arguments);
}

void tst_QScriptQObject::callOverloadedSlot_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QString>("arguments");

    QTest::newRow("overloadedSlot()") << "overloadedSlot" << "";
    QTest::newRow("overloadedSlot(true)") << "overloadedSlot" << "true";
    QTest::newRow("overloadedSlot(123)") << "overloadedSlot" << "123";
    QTest::newRow("overloadedSlot('hello')") << "overloadedSlot" << "'hello'";
}

// Calls an overloaded slot from JS. The purpose of this benchmark is
// to measure the overhead of calling an overloaded slot from JS
// compared to calling the overloaded slot directly from C++ without
// introspection or value conversion (that's the fastest we could
// possibly hope to get).
void tst_QScriptQObject::callOverloadedSlot()
{
    QFETCH(QString, propertyName);
    QFETCH(QString, arguments);

    QScriptEngine engine;
    OverloadedSlotTestObject testObject;
    callMethodHelper(engine, &testObject, propertyName, arguments);
}

// Benchmarks for JS signal handling. The purpose of these benchmarks
// is to measure the overhead of dispatching a Qt signal to JS code
// compared to a normal C++ signal-to-slot dispatch.

void tst_QScriptQObject::voidSignalHandler()
{
    SignalTestObject testObject;
    QScriptEngine engine;
    signalHandlerHelper(engine, &testObject, SIGNAL(voidSignal()));
    QBENCHMARK {
        for (int i = 0; i < ITERATION_COUNT; ++i)
            testObject.emitVoidSignal();
    }
}

void tst_QScriptQObject::boolSignalHandler()
{
    SignalTestObject testObject;
    QScriptEngine engine;
    signalHandlerHelper(engine, &testObject, SIGNAL(boolSignal(bool)));
    QBENCHMARK {
        for (int i = 0; i < ITERATION_COUNT; ++i)
            testObject.emitBoolSignal(true);
    }
}

void tst_QScriptQObject::intSignalHandler()
{
    SignalTestObject testObject;
    QScriptEngine engine;
    signalHandlerHelper(engine, &testObject, SIGNAL(intSignal(int)));
    QBENCHMARK {
        for (int i = 0; i < ITERATION_COUNT; ++i)
            testObject.emitIntSignal(123);
    }
}

void tst_QScriptQObject::doubleSignalHandler()
{
    SignalTestObject testObject;
    QScriptEngine engine;
    signalHandlerHelper(engine, &testObject, SIGNAL(doubleSignal(double)));
    QBENCHMARK {
        for (int i = 0; i < ITERATION_COUNT; ++i)
            testObject.emitDoubleSignal(123.0);
    }
}

void tst_QScriptQObject::stringSignalHandler()
{
    SignalTestObject testObject;
    QScriptEngine engine;
    signalHandlerHelper(engine, &testObject, SIGNAL(stringSignal(QString)));
    QString value = QString::fromLatin1("hello");
    QBENCHMARK {
        for (int i = 0; i < ITERATION_COUNT; ++i)
            testObject.emitStringSignal(value);
    }
}

void tst_QScriptQObject::variantSignalHandler()
{
    SignalTestObject testObject;
    QScriptEngine engine;
    signalHandlerHelper(engine, &testObject, SIGNAL(variantSignal(QVariant)));
    QVariant value = 123;
    QBENCHMARK {
        for (int i = 0; i < ITERATION_COUNT; ++i)
            testObject.emitVariantSignal(value);
    }
}

void tst_QScriptQObject::qobjectSignalHandler()
{
    SignalTestObject testObject;
    QScriptEngine engine;
    signalHandlerHelper(engine, &testObject, SIGNAL(qobjectSignal(QObject*)));
    QBENCHMARK {
        for (int i = 0; i < ITERATION_COUNT; ++i)
            testObject.emitQObjectSignal(this);
    }
}

void tst_QScriptQObject::customTypeSignalHandler()
{
    SignalTestObject testObject;
    QScriptEngine engine;
    signalHandlerHelper(engine, &testObject, SIGNAL(customTypeSignal(CustomType)));
    CustomType value;
    QBENCHMARK {
        for (int i = 0; i < ITERATION_COUNT; ++i)
            testObject.emitCustomTypeSignal(value);
    }
}

void tst_QScriptQObject::emitSignal_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QString>("arguments");

    QTest::newRow("voidSignal()") << "voidSignal" << "";

    QTest::newRow("boolSignal(true)") << "boolSignal" << "true";
    QTest::newRow("intSignal(123)") << "intSignal" << "123";
    QTest::newRow("doubleSignal(123)") << "doubleSignal" << "123";
    QTest::newRow("stringSignal('hello')") << "stringSignal" << "'hello'";
    QTest::newRow("variantSignal(123)") << "variantSignal" << "123";
    QTest::newRow("qobjectSignal(this)") << "qobjectSignal" << "this"; // assumes 'this' is a QObject
}

void tst_QScriptQObject::emitSignal()
{
    QFETCH(QString, propertyName);
    QFETCH(QString, arguments);

    QScriptEngine engine;
    SignalTestObject testObject;
    callMethodHelper(engine, &testObject, propertyName, arguments);
}

void tst_QScriptQObject::readButtonMetaProperty_data()
{
    readMetaProperty_dataHelper(&QPushButton::staticMetaObject);
}

// Reads a meta-object-defined property from JS. The purpose of this
// benchmark is to measure the overhead of reading a property from JS
// compared to calling the property getter directly from C++ without
// introspection or value conversion (that's the fastest we could
// possibly hope to get).
void tst_QScriptQObject::readButtonMetaProperty()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    QPushButton pb;
    readPropertyHelper(engine, engine.newQObject(&pb), propertyName);
}

void tst_QScriptQObject::writeButtonMetaProperty_data()
{
    readButtonMetaProperty_data();
}

// Writes a meta-object-defined property from JS. The purpose of this
// benchmark is to measure the overhead of writing a property from JS
// compared to calling the property setter directly from C++ without
// introspection or value conversion (that's the fastest we could
// possibly hope to get).
void tst_QScriptQObject::writeButtonMetaProperty()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    QPushButton pb;
    QVariant value = pb.property(propertyName.toLatin1());
    writePropertyHelper(engine, engine.newQObject(&pb), propertyName,
                        qScriptValueFromValue(&engine, value));
}

void tst_QScriptQObject::readDynamicButtonProperty_data()
{
    readDynamicProperty_data();
}

// Reads a dynamic property from JS. The purpose of this benchmark is
// to measure the overhead of reading a dynamic property from JS
// versus calling QObject::property(aDynamicProperty) directly from
// C++.
void tst_QScriptQObject::readDynamicButtonProperty()
{
    QFETCH(QVariant, value);

    QPushButton pb;
    const char *propertyName = "dynamicProperty";
    pb.setProperty(propertyName, value);
    QVERIFY(pb.dynamicPropertyNames().contains(propertyName));

    QScriptEngine engine;
    readPropertyHelper(engine, engine.newQObject(&pb), propertyName);
}

void tst_QScriptQObject::writeDynamicButtonProperty_data()
{
    readDynamicButtonProperty_data();
}

// Writes an existing dynamic property from JS. The purpose of this
// benchmark is to measure the overhead of writing a dynamic property
// from JS versus calling QObject::setProperty(aDynamicProperty,
// aVariant) directly from C++.
void tst_QScriptQObject::writeDynamicButtonProperty()
{
    QFETCH(QVariant, value);

    QPushButton pb;
    const char *propertyName = "dynamicProperty";
    pb.setProperty(propertyName, value);
    QVERIFY(pb.dynamicPropertyNames().contains(propertyName));

    QScriptEngine engine;
    writePropertyHelper(engine, engine.newQObject(&pb), propertyName,
                        qScriptValueFromValue(&engine, value));
}

void tst_QScriptQObject::readButtonMethodByName_data()
{
    readMethodByName_dataHelper(&QPushButton::staticMetaObject);
}

// Reads a meta-object-defined method from JS by name. The purpose of
// this benchmark is to measure the overhead of resolving a method
// from JS (effectively, creating and returning a JS wrapper function
// object for a C++ method).
void tst_QScriptQObject::readButtonMethodByName()
{
    readButtonMetaProperty();
}

void tst_QScriptQObject::readButtonMethodBySignature_data()
{
    readMethodBySignature_dataHelper(&QPushButton::staticMetaObject);
}

// Reads a meta-object-defined method from JS by signature. The
// purpose of this benchmark is to measure the overhead of resolving a
// method from JS (effectively, creating and returning a JS wrapper
// function object for a C++ method).
void tst_QScriptQObject::readButtonMethodBySignature()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    QPushButton pb;
    readPropertyHelper(engine, engine.newQObject(&pb), propertyName, "['%0']");
}

void tst_QScriptQObject::readButtonChild_data()
{
    QTest::addColumn<QString>("propertyName");

    QTest::newRow("child") << "child";
}

// Reads a child object from JS. The purpose of this benchmark is to
// measure the overhead of reading a child object from JS compared to
// calling e.g. qFindChild() directly from C++.
void tst_QScriptQObject::readButtonChild()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    QPushButton pb;
    QObject *child = new QObject(&pb);
    child->setObjectName(propertyName);
    readPropertyHelper(engine, engine.newQObject(&pb), propertyName);
}

void tst_QScriptQObject::readButtonPrototypeProperty_data()
{
    readPrototypeProperty_data();
}

// Reads a property that's inherited from a prototype object. The
// purpose of this benchmark is to measure the overhead of resolving a
// prototype property (i.e., how long does it take the binding to
// determine that the QObject doesn't have the property itself).
void tst_QScriptQObject::readButtonPrototypeProperty()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    QPushButton pb;
    readPropertyHelper(engine, engine.newQObject(&pb), propertyName);
}

void tst_QScriptQObject::readButtonScriptProperty_data()
{
    readScriptProperty_data();
}

// Reads a JS (non-Qt) property of a wrapper object. The purpose of
// this benchmark is to measure the overhead of reading a property
// that only exists on the wrapper object, not on the underlying
// QObject.
void tst_QScriptQObject::readButtonScriptProperty()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    QPushButton pb;
    QScriptValue wrapper = engine.newQObject(&pb);
    wrapper.setProperty(propertyName, 123);
    QVERIFY(wrapper.property(propertyName).isValid());
    QVERIFY(!pb.property(propertyName.toLatin1()).isValid());

    readPropertyHelper(engine, wrapper, propertyName);
}

void tst_QScriptQObject::readNoSuchButtonProperty_data()
{
    readNoSuchProperty_data();
}

// Reads a non-existing (undefined) property of a wrapper object. The
// purpose of this benchmark is to measure the overhead of reading a
// property that doesn't exist (i.e., how long does it take the
// binding to determine that it doesn't exist).
void tst_QScriptQObject::readNoSuchButtonProperty()
{
    readButtonMetaProperty();
}

void tst_QScriptQObject::callButtonMethod_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QString>("arguments");

    QTest::newRow("click()") << "click" << "";
    QTest::newRow("animateClick(50)") << "animateClick" << "10";
    QTest::newRow("setChecked(true)") << "setChecked" << "true";
    QTest::newRow("close()") << "close" << "";
    QTest::newRow("setWindowTitle('foo')") << "setWindowTitle" << "'foo'";
}

// Calls a slot from JS. The purpose of this benchmark is to measure
// the overhead of calling a slot from JS compared to calling the slot
// directly from C++ without introspection or value conversion (that's
// the fastest we could possibly hope to get).
void tst_QScriptQObject::callButtonMethod()
{
    QFETCH(QString, propertyName);
    QFETCH(QString, arguments);

    QScriptEngine engine;
    QPushButton pb;
    callMethodHelper(engine, &pb, propertyName, arguments);
}

// Reads all meta-object-defined properties from JS. The purpose of
// this benchmark is to measure the overhead of reading different
// properties in sequence, not just the same one repeatedly (like
// readButtonMetaProperty() does).
void tst_QScriptQObject::readAllButtonMetaProperties()
{
    QPushButton pb;
    readAllMetaPropertiesHelper(&pb);
}

void tst_QScriptQObject::readQScriptableMetaProperty_data()
{
    readMetaProperty_dataHelper(&QtScriptablePropertyTestObject::staticMetaObject);
}

// Reads a meta-object-defined property from JS for an object that
// subclasses QScriptable. The purpose of this benchmark is to measure
// the overhead compared to reading a property of a non-QScriptable
// (see readMetaProperty()).
void tst_QScriptQObject::readQScriptableMetaProperty()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    QtScriptablePropertyTestObject testObject;
    readPropertyHelper(engine, engine.newQObject(&testObject), propertyName);
}

void tst_QScriptQObject::writeQScriptableMetaProperty_data()
{
    readMetaProperty_data();
}

// Writes a meta-object-defined property from JS for an object that
// subclasses QScriptable. The purpose of this benchmark is to measure
// the overhead compared to writing a property of a non-QScriptable
// object (see writeMetaProperty()).
void tst_QScriptQObject::writeQScriptableMetaProperty()
{
    QFETCH(QString, propertyName);

    QScriptEngine engine;
    QtScriptablePropertyTestObject testObject;
    QVariant value = testObject.property(propertyName.toLatin1());
    writePropertyHelper(engine, engine.newQObject(&testObject), propertyName,
                        qScriptValueFromValue(&engine, value));
}

void tst_QScriptQObject::callQScriptableSlot_data()
{
    callSlot_data();
}

// Calls a slot from JS for an object that subclasses QScriptable. The
// purpose of this benchmark is to measure the overhead compared to
// calling a slot of a non-QScriptable object (see callSlot()).
void tst_QScriptQObject::callQScriptableSlot()
{
    QFETCH(QString, propertyName);
    QFETCH(QString, arguments);

    QScriptEngine engine;
    QtScriptableSlotTestObject testObject;
    callMethodHelper(engine, &testObject, propertyName, arguments);
}

QTEST_MAIN(tst_QScriptQObject)
#include "tst_qscriptqobject.moc"
