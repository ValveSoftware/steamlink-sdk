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
#include "../../shared/util.h"
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlpropertymap.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QSignalSpy>
#include <QDebug>

class tst_QQmlPropertyMap : public QObject
{
    Q_OBJECT
public:
    tst_QQmlPropertyMap() {}

private slots:
    void initTestCase();

    void insert();
    void operatorInsert();
    void operatorValue();
    void clear();
    void changed();
    void count();
    void controlledWrite();

    void crashBug();
    void QTBUG_17868();
    void metaObjectAccessibility();
    void QTBUG_31226();
    void QTBUG_29836();
    void QTBUG_35233();
    void disallowExtending();
    void QTBUG_35906();
    void QTBUG_48136();
};

class LazyPropertyMap : public QQmlPropertyMap, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(int someFixedProperty READ someFixedProperty WRITE setSomeFixedProperty NOTIFY someFixedPropertyChanged)
public:
    LazyPropertyMap()
        : QQmlPropertyMap(this, /*parent*/0)
        , value(0)
    {}

    virtual void classBegin() {}
    virtual void componentComplete() {
        insert(QStringLiteral("lateProperty"), QStringLiteral("lateValue"));
    }

    int someFixedProperty() const { return value; }
    void setSomeFixedProperty(int v) { value = v; emit someFixedPropertyChanged(); }

signals:
    void someFixedPropertyChanged();

private:
    int value;
};

void tst_QQmlPropertyMap::initTestCase()
{
    qmlRegisterType<LazyPropertyMap>("QTBUG_35233", 1, 0, "LazyPropertyMap");
}

void tst_QQmlPropertyMap::insert()
{
    QQmlPropertyMap map;
    map.insert(QLatin1String("key1"),100);
    map.insert(QLatin1String("key2"),200);
    QCOMPARE(map.keys().count(), 2);
    QVERIFY(map.contains(QLatin1String("key1")));

    QCOMPARE(map.value(QLatin1String("key1")), QVariant(100));
    QCOMPARE(map.value(QLatin1String("key2")), QVariant(200));

    map.insert(QLatin1String("key1"),"Hello World");
    QCOMPARE(map.value(QLatin1String("key1")), QVariant("Hello World"));

    //inserting property names same with existing method(signal, slot, method) names is not allowed
    //QQmlPropertyMap has an invokable keys() method
    QTest::ignoreMessage(QtWarningMsg, "Creating property with name \"keys\" is not permitted, conflicts with internal symbols.");
    map.insert(QLatin1String("keys"), 1);
    QCOMPARE(map.keys().count(), 2);
    QVERIFY(!map.contains(QLatin1String("keys")));
    QVERIFY(map.value(QLatin1String("keys")).isNull());

    //QQmlPropertyMap has a deleteLater() slot
    QTest::ignoreMessage(QtWarningMsg, "Creating property with name \"deleteLater\" is not permitted, conflicts with internal symbols.");
    map.insert(QLatin1String("deleteLater"), 1);
    QCOMPARE(map.keys().count(), 2);
    QVERIFY(!map.contains(QLatin1String("deleteLater")));
    QVERIFY(map.value(QLatin1String("deleteLater")).isNull());

    //QQmlPropertyMap has an valueChanged() signal
    QTest::ignoreMessage(QtWarningMsg, "Creating property with name \"valueChanged\" is not permitted, conflicts with internal symbols.");
    map.insert(QLatin1String("valueChanged"), 1);
    QCOMPARE(map.keys().count(), 2);
    QVERIFY(!map.contains(QLatin1String("valueChanged")));
    QVERIFY(map.value(QLatin1String("valueChanged")).isNull());

    //but 'valueChange' should be ok
    map.insert(QLatin1String("valueChange"), 1);
    QCOMPARE(map.keys().count(), 3);
    QVERIFY(map.contains(QLatin1String("valueChange")));
    QCOMPARE(map.value(QLatin1String("valueChange")), QVariant(1));

    //'valueCHANGED' should be ok, too
    map.insert(QLatin1String("valueCHANGED"), 1);
    QCOMPARE(map.keys().count(), 4);
    QVERIFY(map.contains(QLatin1String("valueCHANGED")));
    QCOMPARE(map.value(QLatin1String("valueCHANGED")), QVariant(1));
}

void tst_QQmlPropertyMap::operatorInsert()
{
    QQmlPropertyMap map;
    map[QLatin1String("key1")] = 100;
    map[QLatin1String("key2")] = 200;
    QCOMPARE(map.keys().count(), 2);

    QCOMPARE(map.value(QLatin1String("key1")), QVariant(100));
    QCOMPARE(map.value(QLatin1String("key2")), QVariant(200));

    map[QLatin1String("key1")] = "Hello World";
    QCOMPARE(map.value(QLatin1String("key1")), QVariant("Hello World"));
}

void tst_QQmlPropertyMap::operatorValue()
{
    QQmlPropertyMap map;
    map.insert(QLatin1String("key1"),100);
    map.insert(QLatin1String("key2"),200);
    QCOMPARE(map.count(), 2);
    QVERIFY(map.contains(QLatin1String("key1")));

    const QQmlPropertyMap &constMap = map;

    QCOMPARE(constMap.value(QLatin1String("key1")), QVariant(100));
    QCOMPARE(constMap.value(QLatin1String("key2")), QVariant(200));
    QCOMPARE(constMap[QLatin1String("key1")], constMap.value(QLatin1String("key1")));
    QCOMPARE(constMap[QLatin1String("key2")], constMap.value(QLatin1String("key2")));
}

void tst_QQmlPropertyMap::clear()
{
    QQmlPropertyMap map;
    map.insert(QLatin1String("key1"),100);
    QCOMPARE(map.keys().count(), 1);

    QCOMPARE(map.value(QLatin1String("key1")), QVariant(100));

    map.clear(QLatin1String("key1"));
    QCOMPARE(map.keys().count(), 1);
    QVERIFY(map.contains(QLatin1String("key1")));
    QCOMPARE(map.value(QLatin1String("key1")), QVariant());
}

void tst_QQmlPropertyMap::changed()
{
    QQmlPropertyMap map;
    QSignalSpy spy(&map, SIGNAL(valueChanged(QString,QVariant)));
    map.insert(QLatin1String("key1"),100);
    map.insert(QLatin1String("key2"),200);
    QCOMPARE(spy.count(), 0);

    map.clear(QLatin1String("key1"));
    QCOMPARE(spy.count(), 0);

    //make changes in QML
    QQmlEngine engine;
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty(QLatin1String("testdata"), &map);
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nText { text: { testdata.key1 = 'Hello World'; 'X' } }",
            QUrl::fromLocalFile(""));
    QVERIFY(component.isReady());
    QQuickText *txt = qobject_cast<QQuickText*>(component.create());
    QVERIFY(txt);
    QCOMPARE(txt->text(), QString('X'));
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.count(), 2);
    QCOMPARE(arguments.at(0).toString(),QLatin1String("key1"));
    QCOMPARE(arguments.at(1).value<QVariant>(),QVariant("Hello World"));
    QCOMPARE(map.value(QLatin1String("key1")), QVariant("Hello World"));
}

void tst_QQmlPropertyMap::count()
{
    QQmlPropertyMap map;
    QCOMPARE(map.isEmpty(), true);
    map.insert(QLatin1String("key1"),100);
    map.insert(QLatin1String("key2"),200);
    QCOMPARE(map.count(), 2);
    QCOMPARE(map.isEmpty(), false);

    map.insert(QLatin1String("key3"),"Hello World");
    QCOMPARE(map.count(), 3);

    //clearing doesn't remove the key
    map.clear(QLatin1String("key3"));
    QCOMPARE(map.count(), 3);
    QCOMPARE(map.size(), map.count());
}

class MyPropertyMap : public QQmlPropertyMap
{
    Q_OBJECT
protected:
    virtual QVariant updateValue(const QString &key, const QVariant &src)
    {
        if (key == QLatin1String("key1")) {
            // 'key1' must be all uppercase
            const QString original(src.toString());
            return QVariant(original.toUpper());
        }

        return src;
    }
};

void tst_QQmlPropertyMap::controlledWrite()
{
    MyPropertyMap map;
    QCOMPARE(map.isEmpty(), true);

    //make changes in QML
    QQmlEngine engine;
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty(QLatin1String("testdata"), &map);

    const char *qmlSource =
        "import QtQuick 2.0\n"
        "Item { Component.onCompleted: { testdata.key1 = 'Hello World'; testdata.key2 = 'Goodbye' } }";

    QQmlComponent component(&engine);
    component.setData(qmlSource, QUrl::fromLocalFile(""));
    QVERIFY(component.isReady());

    QObject *obj = component.create();
    QVERIFY(obj);
    delete obj;

    QCOMPARE(map.value(QLatin1String("key1")), QVariant("HELLO WORLD"));
    QCOMPARE(map.value(QLatin1String("key2")), QVariant("Goodbye"));
}

void tst_QQmlPropertyMap::crashBug()
{
    QQmlPropertyMap map;

    QQmlEngine engine;
    QQmlContext context(&engine);
    context.setContextProperty("map", &map);

    QQmlComponent c(&engine);
    c.setData("import QtQuick 2.0\nBinding { target: map; property: \"myProp\"; value: 10 + 23 }",QUrl());
    QObject *obj = c.create(&context);
    delete obj;
}

void tst_QQmlPropertyMap::QTBUG_17868()
{
    QQmlPropertyMap map;

    QQmlEngine engine;
    QQmlContext context(&engine);
    context.setContextProperty("map", &map);
    map.insert("key", 1);
    QQmlComponent c(&engine);
    c.setData("import QtQuick 2.0\nItem {property bool error:false; Component.onCompleted: {try{ map.keys(); }catch(e) {error=true;}}}",QUrl());
    QObject *obj = c.create(&context);
    QVERIFY(obj);
    QVERIFY(!obj->property("error").toBool());
    delete obj;

}

class MyEnhancedPropertyMap : public QQmlPropertyMap
{
    Q_OBJECT
public:
    MyEnhancedPropertyMap() : QQmlPropertyMap(this, 0), m_testSlotCalled(false) {}
    bool testSlotCalled() const { return m_testSlotCalled; }

signals:
    void testSignal();

public slots:
    void testSlot() { m_testSlotCalled = true; }

private:
    bool m_testSlotCalled;
};

void tst_QQmlPropertyMap::metaObjectAccessibility()
{
    QQmlTestMessageHandler messageHandler;

    QQmlEngine engine;

    MyEnhancedPropertyMap map;

    // Verify that signals and slots of QQmlPropertyMap-derived classes are accessible
    QObject::connect(&map, SIGNAL(testSignal()), &engine, SIGNAL(quit()));
    QObject::connect(&engine, SIGNAL(quit()), &map, SLOT(testSlot()));

    QCOMPARE(map.metaObject()->className(), "MyEnhancedPropertyMap");

    QVERIFY2(messageHandler.messages().isEmpty(), qPrintable(messageHandler.messageString()));
}

void tst_QQmlPropertyMap::QTBUG_31226()
{
    /* Instantiate a property map from QML, and verify that property changes
     * made from C++ are visible from QML */
    QQmlEngine engine;
    QQmlContext context(&engine);
    qmlRegisterType<QQmlPropertyMap>("QTBUG_31226", 1, 0, "PropertyMap");
    QQmlComponent c(&engine);
    c.setData("import QtQuick 2.0\nimport QTBUG_31226 1.0\n"
              "Item {\n"
              "  property string myProp\n"
              "  PropertyMap { id: qmlPropertyMap; objectName: \"qmlPropertyMap\" }\n"
              "  Timer { interval: 5; running: true; onTriggered: { myProp = qmlPropertyMap.greeting; } }\n"
              "}",
              QUrl());
    QObject *obj = c.create(&context);
    QVERIFY(obj);

    QQmlPropertyMap *qmlPropertyMap = obj->findChild<QQmlPropertyMap*>(QString("qmlPropertyMap"));
    QVERIFY(qmlPropertyMap);

    qmlPropertyMap->insert("greeting", QString("Hello world!"));
    QTRY_COMPARE(obj->property("myProp").toString(), QString("Hello world!"));

    delete obj;

}

void tst_QQmlPropertyMap::QTBUG_29836()
{
    MyEnhancedPropertyMap map;
    QCOMPARE(map.testSlotCalled(), false);

    QQmlEngine engine;
    QQmlContext context(&engine);
    context.setContextProperty("enhancedMap", &map);
    QQmlComponent c(&engine);
    c.setData("import QtQuick 2.0\n"
              "Item {\n"
              "  Timer { interval: 5; running: true; onTriggered: enhancedMap.testSlot() }\n"
              "}",
              QUrl());
    QObject *obj = c.create(&context);
    QVERIFY(obj);

    QTRY_COMPARE(map.testSlotCalled(), true);

    delete obj;

}

void tst_QQmlPropertyMap::QTBUG_35233()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQml 2.0\n"
                      "import QTBUG_35233 1.0\n"
                      "QtObject {\n"
                      "    property QtObject testMap: LazyPropertyMap {\n"
                      "        id: map\n"
                      "    }\n"
                      "    property QtObject sibling: QtObject {\n"
                      "        objectName: \"sibling\"\n"
                      "        property string testValue: map.lateProperty\n"
                      "    }\n"
                      "}", QUrl());
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(!obj.isNull());

    QObject *sibling = obj->findChild<QObject*>("sibling");
    QVERIFY(sibling);
    QCOMPARE(sibling->property("testValue").toString(), QString("lateValue"));
}

void tst_QQmlPropertyMap::disallowExtending()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQml 2.0\n"
                      "import QTBUG_35233 1.0\n"
                      "LazyPropertyMap {\n"
                      "    id: blah\n"
                      "    someFixedProperty: 42\n"
                      "}\n", QUrl());
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(!obj.isNull());

    component.setData("import QtQml 2.0\n"
                      "import QTBUG_35233 1.0\n"
                      "LazyPropertyMap {\n"
                      "    id: blah\n"
                      "    property int someNewProperty;\n"
                      "}\n", QUrl());
    obj.reset(component.create());
    QVERIFY(obj.isNull());
    QCOMPARE(component.errors().count(), 1);
    QCOMPARE(component.errors().at(0).toString(), QStringLiteral("<Unknown File>:3:1: Fully dynamic types cannot declare new properties."));
}

void tst_QQmlPropertyMap::QTBUG_35906()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQml 2.0\n"
                      "import QTBUG_35233 1.0\n"
                      "QtObject {\n"
                      "    property int testValue: mapById.someFixedProperty\n"
                      "\n"
                      "    property QtObject maProperty: LazyPropertyMap {\n"
                      "        id: mapById\n"
                      "        someFixedProperty: 42\n"
                      "    }\n"
                      "}\n", QUrl());
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(!obj.isNull());
    QVariant value = obj->property("testValue");
    QCOMPARE(value.type(), QVariant::Int);
    QCOMPARE(value.toInt(), 42);
}

void tst_QQmlPropertyMap::QTBUG_48136()
{
    static const char key[] = "mykey";
    QQmlPropertyMap map;

    //
    // Test that the notify signal is emitted correctly
    //

    const int propIndex = map.metaObject()->indexOfProperty(key);
    const QMetaProperty prop = map.metaObject()->property(propIndex);
    QSignalSpy notifySpy(&map, QByteArray::number(QSIGNAL_CODE) + prop.notifySignal().methodSignature());

    map.insert(key, 42);
    QCOMPARE(notifySpy.count(), 1);
    map.insert(key, 43);
    QCOMPARE(notifySpy.count(), 2);
    map.insert(key, 43);
    QCOMPARE(notifySpy.count(), 2);
    map.insert(key, 44);
    QCOMPARE(notifySpy.count(), 3);

    //
    // Test that the valueChanged signal is emitted correctly
    //
    QSignalSpy valueChangedSpy(&map, &QQmlPropertyMap::valueChanged);
    map.setProperty(key, 44);
    QCOMPARE(valueChangedSpy.count(), 0);
    map.setProperty(key, 45);
    QCOMPARE(valueChangedSpy.count(), 1);
    map.setProperty(key, 45);
    QCOMPARE(valueChangedSpy.count(), 1);
}

QTEST_MAIN(tst_QQmlPropertyMap)

#include "tst_qqmlpropertymap.moc"
