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

#include <QtTest/QtTest>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlengine.h>
#include "../../shared/util.h"

Q_DECLARE_METATYPE(QMetaMethod::MethodType)

class MyQmlObject : public QObject
{
    Q_OBJECT
};
QML_DECLARE_TYPE(MyQmlObject)

class tst_QQmlMetaObject : public QQmlDataTest
{
    Q_OBJECT
private slots:
    void initTestCase();

    void property_data();
    void property();
    void method_data();
    void method();

private:
    MyQmlObject myQmlObject;
};

void tst_QQmlMetaObject::initTestCase()
{
    QQmlDataTest::initTestCase();

    qmlRegisterType<MyQmlObject>("Qt.test", 1,0, "MyQmlObject");
}

void tst_QQmlMetaObject::property_data()
{
    QTest::addColumn<QString>("testFile");
    QTest::addColumn<QByteArray>("cppTypeName");
    QTest::addColumn<int>("cppType");
    QTest::addColumn<bool>("isDefault");
    QTest::addColumn<QVariant>("expectedValue");
    QTest::addColumn<bool>("isWritable");
    QTest::addColumn<QVariant>("newValue");

    QTest::newRow("int") << "property.int.qml"
            << QByteArray("int") << int(QMetaType::Int)
            << false // default
            << QVariant(19) << true << QVariant(42);
    QTest::newRow("bool") << "property.bool.qml"
            << QByteArray("bool") << int(QMetaType::Bool)
            << true // default
            << QVariant(true) << true << QVariant(false);
    QTest::newRow("double") << "property.double.qml"
             << QByteArray("double") << int(QMetaType::Double)
             << false // default
             << QVariant(double(1234567890.))
             << true // writable
             << QVariant(double(1.23456789));
    QTest::newRow("real") << "property.real.qml"
            << QByteArray("double") << int(QMetaType::Double)
            << false // default
            << QVariant(double(1234567890.))
            << true // writable
            << QVariant(double(1.23456789));
    QTest::newRow("string") << "property.string.qml"
            << QByteArray("QString") << int(QMetaType::QString)
            << true // default
            << QVariant(QString::fromLatin1("dog"))
            << true // writable
            << QVariant(QString::fromLatin1("food"));
    QTest::newRow("url") << "property.url.qml"
            << QByteArray("QUrl") << int(QMetaType::QUrl)
            << false // default
            << QVariant(QUrl("http://foo.bar"))
            << true //writable
            << QVariant(QUrl("http://bar.baz"));
    QTest::newRow("color") << "property.color.qml"
            << QByteArray("QColor") << int(QMetaType::QColor)
            << true // default
            << QVariant(QColor("#ff0000"))
            << true // writable
            << QVariant(QColor("#00ff00"));
    QTest::newRow("date") << "property.date.qml"
            << QByteArray("QDateTime") << int(QMetaType::QDateTime)
            << false // default
            << QVariant(QDateTime(QDate(2012, 2, 7)))
            << true // writable
            << QVariant(QDateTime(QDate(2010, 7, 2)));
    QTest::newRow("variant") << "property.variant.qml"
            << QByteArray("QVariant") << int(QMetaType::QVariant)
            << true // default
            << QVariant(QPointF(12, 34))
            << true // writable
            << QVariant(QSizeF(45, 67));
    QTest::newRow("var") << "property.var.qml"
            << QByteArray("QVariant") << int(QMetaType::QVariant)
            << false // default
            << QVariant(QVariantList() << 5 << true << "ciao")
            << true // writable
            << QVariant(QVariantList() << 17.0);
    QTest::newRow("QtObject") << "property.QtObject.qml"
            << QByteArray("QObject*") << int(QMetaType::QObjectStar)
            << false // default
            << QVariant()
            << true // writable
            << QVariant::fromValue(static_cast<QObject*>(this));
    QTest::newRow("list<QtObject>") << "property.list.QtObject.qml"
            << QByteArray("QQmlListProperty<QObject>")
            << qMetaTypeId<QQmlListProperty<QObject> >()
            << false // default
            << QVariant()
            << false // writable
            << QVariant();
    QTest::newRow("MyQmlObject") << "property.MyQmlObject.qml"
            << QByteArray("MyQmlObject*") << qMetaTypeId<MyQmlObject*>()
            << false // default
            << QVariant()
            << true // writable
            << QVariant::fromValue(&myQmlObject);
    QTest::newRow("list<MyQmlObject>") << "property.list.MyQmlObject.qml"
            << QByteArray("QQmlListProperty<MyQmlObject>")
            << qMetaTypeId<QQmlListProperty<MyQmlObject> >()
            << false // default
            << QVariant()
            << false // writable
            << QVariant();
    QTest::newRow("alias") << "property.alias.qml"
            << QByteArray("QString") << int(QMetaType::QString)
            << false // default
            << QVariant(QString::fromLatin1("Joe"))
            << true // writable
            << QVariant(QString::fromLatin1("Bob"));
    QTest::newRow("alias-2") << "property.alias.2.qml"
            << QByteArray("QObject*") << int(QMetaType::QObjectStar)
            << false // default
            << QVariant()
            << false // writable
            << QVariant();
    QTest::newRow("alias-3") << "property.alias.3.qml"
            << QByteArray("QString") << int(QMetaType::QString)
            << false // default
            << QVariant(QString::fromLatin1("Arial"))
            << true // writable
            << QVariant(QString::fromLatin1("Helvetica"));
}

void tst_QQmlMetaObject::property()
{
    QFETCH(QString, testFile);
    QFETCH(QByteArray, cppTypeName);
    QFETCH(int, cppType);
    QFETCH(bool, isDefault);
    QFETCH(QVariant, expectedValue);
    QFETCH(bool, isWritable);
    QFETCH(QVariant, newValue);

    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl(testFile));
    QObject *object = component.create();
    QVERIFY(object != 0);

    const QMetaObject *mo = object->metaObject();
    QVERIFY(mo->superClass() != 0);
    QVERIFY(QByteArray(mo->className()).contains("_QML_"));
    QCOMPARE(mo->propertyOffset(), mo->superClass()->propertyCount());
    QCOMPARE(mo->propertyCount(), mo->superClass()->propertyCount() + 1);

    QMetaProperty prop = mo->property(mo->propertyOffset());
    QCOMPARE(prop.name(), "test");

    QCOMPARE(QByteArray(prop.typeName()), cppTypeName);
    if (prop.userType() < QMetaType::User)
        QCOMPARE(prop.type(), QVariant::Type(cppType));
    else
        QCOMPARE(prop.type(), QVariant::UserType);
    QCOMPARE(prop.userType(), cppType);

    QVERIFY(!prop.isConstant());
    QVERIFY(!prop.isDesignable());
    QVERIFY(!prop.isEnumType());
    QVERIFY(!prop.isFinal());
    QVERIFY(!prop.isFlagType());
    QVERIFY(prop.isReadable());
    QVERIFY(!prop.isResettable());
    QVERIFY(prop.isScriptable());
    QVERIFY(!prop.isStored());
    QVERIFY(!prop.isUser());
    QVERIFY(prop.isValid());
    QCOMPARE(prop.isWritable(), isWritable);

    QCOMPARE(mo->classInfoOffset(), mo->superClass()->classInfoCount());
    QCOMPARE(mo->classInfoCount(), mo->superClass()->classInfoCount() + (isDefault ? 1 : 0));
    if (isDefault) {
        QMetaClassInfo info = mo->classInfo(mo->classInfoOffset());
        QCOMPARE(info.name(), "DefaultProperty");
        QCOMPARE(info.value(), "test");
    }

    QCOMPARE(mo->methodOffset(), mo->superClass()->methodCount());
    QCOMPARE(mo->methodCount(), mo->superClass()->methodCount() + 1); // the signal

    QVERIFY(prop.notifySignalIndex() != -1);
    QMetaMethod signal = prop.notifySignal();
    QCOMPARE(signal.methodType(), QMetaMethod::Signal);
    QCOMPARE(signal.name(), QByteArray("testChanged"));
    QCOMPARE(signal.methodSignature(), QByteArray("testChanged()"));
    QCOMPARE(signal.access(), QMetaMethod::Public);
    QCOMPARE(signal.parameterCount(), 0);
    QCOMPARE(signal.parameterTypes(), QList<QByteArray>());
    QCOMPARE(signal.parameterNames(), QList<QByteArray>());
    QCOMPARE(signal.tag(), "");
    QCOMPARE(signal.typeName(), "void");
    QCOMPARE(signal.returnType(), int(QMetaType::Void));

    QSignalSpy changedSpy(object, SIGNAL(testChanged()));
    QObject::connect(object, SIGNAL(testChanged()), object, SLOT(deleteLater()));

    QVariant value = prop.read(object);
    if (value.userType() == qMetaTypeId<QJSValue>())
        value = value.value<QJSValue>().toVariant();
    if (expectedValue.isValid())
        QCOMPARE(value, expectedValue);
    else
        QVERIFY(value.isValid());
    QCOMPARE(changedSpy.count(), 0);

    if (isWritable) {
        QVERIFY(prop.write(object, newValue));
        QCOMPARE(changedSpy.count(), 1);
        QVariant value = prop.read(object);
        if (value.userType() == qMetaTypeId<QJSValue>())
            value = value.value<QJSValue>().toVariant();
        QCOMPARE(value, newValue);
    } else {
        QVERIFY(!prop.write(object, prop.read(object)));
        QCOMPARE(changedSpy.count(), 0);
    }

    delete object;
}

void tst_QQmlMetaObject::method_data()
{
    QTest::addColumn<QString>("testFile");
    QTest::addColumn<QString>("signature");
    QTest::addColumn<QMetaMethod::MethodType>("methodType");
    QTest::addColumn<int>("returnType");
    QTest::addColumn<QString>("returnTypeName");
    QTest::addColumn<QList<int> >("parameterTypes");
    QTest::addColumn<QList<QByteArray> >("parameterTypeNames");
    QTest::addColumn<QList<QByteArray> >("parameterNames");

    QTest::newRow("testFunction()") << "method.1.qml"
            << "testFunction()"
            << QMetaMethod::Slot
            << int(QMetaType::QVariant) << "QVariant"
            << QList<int>()
            << QList<QByteArray>()
            << QList<QByteArray>();
    QTest::newRow("testFunction(foo)") << "method.2.qml"
            << "testFunction(QVariant)"
            << QMetaMethod::Slot
            << int(QMetaType::QVariant) << "QVariant"
            << (QList<int>() << QMetaType::QVariant)
            << (QList<QByteArray>() << "QVariant")
            << (QList<QByteArray>() << "foo");
    QTest::newRow("testFunction(foo, bar, baz)") << "method.3.qml"
            << "testFunction(QVariant,QVariant,QVariant)"
            << QMetaMethod::Slot
            << int(QMetaType::QVariant) << "QVariant"
            << (QList<int>() << QMetaType::QVariant << QMetaType::QVariant << QMetaType::QVariant)
            << (QList<QByteArray>() << "QVariant" << "QVariant" << "QVariant")
            << (QList<QByteArray>() << "foo" << "bar" << "baz");
    QTest::newRow("testSignal") << "signal.1.qml"
            << "testSignal()"
            << QMetaMethod::Signal
            << int(QMetaType::Void) << "void"
            << QList<int>()
            << QList<QByteArray>()
            << QList<QByteArray>();
    QTest::newRow("testSignal(string foo)") << "signal.2.qml"
            << "testSignal(QString)"
            << QMetaMethod::Signal
            << int(QMetaType::Void) << "void"
            << (QList<int>() << QMetaType::QString)
            << (QList<QByteArray>() << "QString")
            << (QList<QByteArray>() << "foo");
    QTest::newRow("testSignal(int foo, bool bar, real baz)") << "signal.3.qml"
            << "testSignal(int,bool,double)"
            << QMetaMethod::Signal
            << int(QMetaType::Void) << "void"
            << (QList<int>() << QMetaType::Int << QMetaType::Bool << QMetaType::Double)
            << (QList<QByteArray>() << "int" << "bool" << "double")
            << (QList<QByteArray>() << "foo" << "bar" << "baz");
    QTest::newRow("testSignal(variant foo, var bar)") << "signal.4.qml"
            << "testSignal(QVariant,QVariant)"
            << QMetaMethod::Signal
            << int(QMetaType::Void) << "void"
            << (QList<int>() << QMetaType::QVariant << QMetaType::QVariant)
            << (QList<QByteArray>() << "QVariant" << "QVariant")
            << (QList<QByteArray>() << "foo" << "bar");
    QTest::newRow("testSignal(color foo, date bar, url baz)") << "signal.5.qml"
            << "testSignal(QColor,QDateTime,QUrl)"
            << QMetaMethod::Signal
            << int(QMetaType::Void) << "void"
            << (QList<int>() << QMetaType::QColor << QMetaType::QDateTime << QMetaType::QUrl)
            << (QList<QByteArray>() << "QColor" << "QDateTime" << "QUrl")
            << (QList<QByteArray>() << "foo" << "bar" << "baz");
    QTest::newRow("testSignal(double foo)") << "signal.6.qml"
            << "testSignal(double)"
            << QMetaMethod::Signal
            << int(QMetaType::Void) << "void"
            << (QList<int>() << QMetaType::Double)
            << (QList<QByteArray>() << "double")
            << (QList<QByteArray>() << "foo");
}

void tst_QQmlMetaObject::method()
{
    QFETCH(QString, testFile);
    QFETCH(QString, signature);
    QFETCH(QMetaMethod::MethodType, methodType);
    QFETCH(int, returnType);
    QFETCH(QString, returnTypeName);
    QFETCH(QList<int>, parameterTypes);
    QFETCH(QList<QByteArray>, parameterTypeNames);
    QFETCH(QList<QByteArray>, parameterNames);

    QCOMPARE(parameterTypes.size(), parameterTypeNames.size());
    QCOMPARE(parameterTypeNames.size(), parameterNames.size());

    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl(testFile));
    QObject *object = component.create();
    QVERIFY(object != 0);

    const QMetaObject *mo = object->metaObject();
    QVERIFY(mo->superClass() != 0);
    QVERIFY(QByteArray(mo->className()).contains("_QML_"));
    QCOMPARE(mo->methodOffset(), mo->superClass()->methodCount());
    QCOMPARE(mo->methodCount(), mo->superClass()->methodCount() + 1);

    QMetaMethod method = mo->method(mo->methodOffset());
    QCOMPARE(method.methodType(), methodType);
    QCOMPARE(QString::fromUtf8(method.methodSignature().constData()), signature);
    QCOMPARE(method.access(), QMetaMethod::Public);

    QString computedName = signature.left(signature.indexOf('('));
    QCOMPARE(QString::fromUtf8(method.name()), computedName);

    QCOMPARE(method.parameterCount(), parameterTypes.size());
    for (int i = 0; i < parameterTypes.size(); ++i)
        QCOMPARE(method.parameterType(i), parameterTypes.at(i));
    QCOMPARE(method.parameterTypes(), parameterTypeNames);
    QCOMPARE(method.tag(), "");

    QCOMPARE(QString::fromUtf8(method.typeName()), returnTypeName);
    QCOMPARE(method.returnType(), returnType);

    delete object;
}

QTEST_MAIN(tst_QQmlMetaObject)

#include "tst_qqmlmetaobject.moc"
