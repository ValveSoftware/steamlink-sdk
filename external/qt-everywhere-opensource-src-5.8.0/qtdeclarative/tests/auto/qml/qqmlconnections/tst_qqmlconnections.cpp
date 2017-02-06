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
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <private/qqmlconnections_p.h>
#include <private/qquickitem_p.h>
#include "../../shared/util.h"
#include <QtQml/qqmlscriptstring.h>

class tst_qqmlconnections : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlconnections();

private slots:
    void defaultValues();
    void properties();
    void connection();
    void trimming();
    void targetChanged();
    void unknownSignals_data();
    void unknownSignals();
    void errors_data();
    void errors();
    void rewriteErrors();
    void singletonTypeTarget();
    void enableDisable_QTBUG_36350();
    void clearImplicitTarget();

private:
    QQmlEngine engine;
};

tst_qqmlconnections::tst_qqmlconnections()
{
}

void tst_qqmlconnections::defaultValues()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("test-connection3.qml"));
    QQmlConnections *item = qobject_cast<QQmlConnections*>(c.create());

    QVERIFY(item != 0);
    QVERIFY(!item->target());

    delete item;
}

void tst_qqmlconnections::properties()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("test-connection2.qml"));
    QQmlConnections *item = qobject_cast<QQmlConnections*>(c.create());

    QVERIFY(item != 0);

    QVERIFY(item != 0);
    QCOMPARE(item->target(), item);

    delete item;
}

void tst_qqmlconnections::connection()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("test-connection.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(c.create());

    QVERIFY(item != 0);

    QCOMPARE(item->property("tested").toBool(), false);
    QCOMPARE(item->width(), 50.);
    emit item->setWidth(100.);
    QCOMPARE(item->width(), 100.);
    QCOMPARE(item->property("tested").toBool(), true);

    delete item;
}

void tst_qqmlconnections::trimming()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("trimming.qml"));
    QObject *object = c.create();

    QVERIFY(object != 0);

    QCOMPARE(object->property("tested").toString(), QString(""));
    int index = object->metaObject()->indexOfSignal("testMe(int,QString)");
    QMetaMethod method = object->metaObject()->method(index);
    method.invoke(object,
                  Qt::DirectConnection,
                  Q_ARG(int, 5),
                  Q_ARG(QString, "worked"));
    QCOMPARE(object->property("tested").toString(), QString("worked5"));

    delete object;
}

// Confirm that target can be changed by one of our signal handlers
void tst_qqmlconnections::targetChanged()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("connection-targetchange.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(c.create());
    QVERIFY(item != 0);

    QQmlConnections *connections = item->findChild<QQmlConnections*>("connections");
    QVERIFY(connections);

    QQuickItem *item1 = item->findChild<QQuickItem*>("item1");
    QVERIFY(item1);

    item1->setWidth(200);

    QQuickItem *item2 = item->findChild<QQuickItem*>("item2");
    QVERIFY(item2);
    QCOMPARE(connections->target(), item2);

    // If we don't crash then we're OK

    delete item;
}

void tst_qqmlconnections::unknownSignals_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("error");

    QTest::newRow("basic") << "connection-unknownsignals.qml" << ":6:30: QML Connections: Cannot assign to non-existent property \"onFooBar\"";
    QTest::newRow("parent") << "connection-unknownsignals-parent.qml" << ":4:30: QML Connections: Cannot assign to non-existent property \"onFooBar\"";
    QTest::newRow("ignored") << "connection-unknownsignals-ignored.qml" << ""; // should be NO error
    QTest::newRow("notarget") << "connection-unknownsignals-notarget.qml" << ""; // should be NO error
}

void tst_qqmlconnections::unknownSignals()
{
    QFETCH(QString, file);
    QFETCH(QString, error);

    QUrl url = testFileUrl(file);
    if (!error.isEmpty()) {
        QTest::ignoreMessage(QtWarningMsg, (url.toString() + error).toLatin1());
    } else {
        // QTest has no way to insist no message (i.e. fail)
    }

    QQmlEngine engine;
    QQmlComponent c(&engine, url);
    QObject *object = c.create();
    QVERIFY(object != 0);

    // check that connection is created (they are all runtime errors)
    QQmlConnections *connections = object->findChild<QQmlConnections*>("connections");
    QVERIFY(connections);

    if (file == "connection-unknownsignals-ignored.qml")
        QVERIFY(connections->ignoreUnknownSignals());

    delete object;
}

void tst_qqmlconnections::errors_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("error");

    QTest::newRow("no \"on\"") << "error-property.qml" << "Cannot assign to non-existent property \"fakeProperty\"";
    QTest::newRow("3rd letter lowercase") << "error-property2.qml" << "Cannot assign to non-existent property \"onfakeProperty\"";
    QTest::newRow("child object") << "error-object.qml" << "Connections: nested objects not allowed";
    QTest::newRow("grouped object") << "error-syntax.qml" << "Connections: syntax error";
}

void tst_qqmlconnections::errors()
{
    QFETCH(QString, file);
    QFETCH(QString, error);

    QUrl url = testFileUrl(file);

    QQmlEngine engine;
    QQmlComponent c(&engine, url);
    QVERIFY(c.isError());
    QList<QQmlError> errors = c.errors();
    QCOMPARE(errors.count(), 1);
    QCOMPARE(errors.at(0).description(), error);
}

class TestObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ran READ ran WRITE setRan)

public:
    TestObject(QObject *parent = 0) : QObject(parent), m_ran(false) {}
    ~TestObject() {}

    bool ran() const { return m_ran; }
    void setRan(bool arg) { m_ran = arg; }

signals:
    void unnamedArgumentSignal(int a, qreal, QString c);
    void signalWithGlobalName(int parseInt);

private:
    bool m_ran;
};

void tst_qqmlconnections::rewriteErrors()
{
    qmlRegisterType<TestObject>("Test", 1, 0, "TestObject");
    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("rewriteError-unnamed.qml"));
        QTest::ignoreMessage(QtWarningMsg, (c.url().toString() + ":5:35: QML Connections: Signal uses unnamed parameter followed by named parameter.").toLatin1());
        TestObject *obj = qobject_cast<TestObject*>(c.create());
        QVERIFY(obj != 0);
        obj->unnamedArgumentSignal(1, .5, "hello");
        QCOMPARE(obj->ran(), false);

        delete obj;
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("rewriteError-global.qml"));
        QTest::ignoreMessage(QtWarningMsg, (c.url().toString() + ":5:35: QML Connections: Signal parameter \"parseInt\" hides global variable.").toLatin1());
        TestObject *obj = qobject_cast<TestObject*>(c.create());
        QVERIFY(obj != 0);

        obj->signalWithGlobalName(10);
        QCOMPARE(obj->ran(), false);

        delete obj;
    }
}


class MyTestSingletonType : public QObject
{
Q_OBJECT
Q_PROPERTY(int intProp READ intProp WRITE setIntProp NOTIFY intPropChanged)

public:
    MyTestSingletonType(QObject *parent = 0) : QObject(parent), m_intProp(0), m_changeCount(0) {}
    ~MyTestSingletonType() {}

    Q_INVOKABLE int otherMethod(int val) { return val + 4; }

    int intProp() const { return m_intProp; }
    void setIntProp(int val)
    {
        if (++m_changeCount % 3 == 0) emit otherSignal();
        m_intProp = val; emit intPropChanged();
    }

signals:
    void intPropChanged();
    void otherSignal();

private:
    int m_intProp;
    int m_changeCount;
};

static QObject *module_api_factory(QQmlEngine *engine, QJSEngine *scriptEngine)
{
   Q_UNUSED(engine)
   Q_UNUSED(scriptEngine)
   MyTestSingletonType *api = new MyTestSingletonType();
   return api;
}

// QTBUG-20937
void tst_qqmlconnections::singletonTypeTarget()
{
    qmlRegisterSingletonType<MyTestSingletonType>("MyTestSingletonType", 1, 0, "Api", module_api_factory);
    QQmlComponent component(&engine, testFileUrl("singletontype-target.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("moduleIntPropChangedCount").toInt(), 0);
    QCOMPARE(object->property("moduleOtherSignalCount").toInt(), 0);

    QMetaObject::invokeMethod(object, "setModuleIntProp");
    QCOMPARE(object->property("moduleIntPropChangedCount").toInt(), 1);
    QCOMPARE(object->property("moduleOtherSignalCount").toInt(), 0);

    QMetaObject::invokeMethod(object, "setModuleIntProp");
    QCOMPARE(object->property("moduleIntPropChangedCount").toInt(), 2);
    QCOMPARE(object->property("moduleOtherSignalCount").toInt(), 0);

    // the singleton Type emits otherSignal every 3 times the int property changes.
    QMetaObject::invokeMethod(object, "setModuleIntProp");
    QCOMPARE(object->property("moduleIntPropChangedCount").toInt(), 3);
    QCOMPARE(object->property("moduleOtherSignalCount").toInt(), 1);

    delete object;
}

void tst_qqmlconnections::enableDisable_QTBUG_36350()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("test-connection.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(c.create());
    QVERIFY(item != 0);

    QQmlConnections *connections = item->findChild<QQmlConnections*>("connections");
    QVERIFY(connections);

    connections->setEnabled(false);
    QCOMPARE(item->property("tested").toBool(), false);
    QCOMPARE(item->width(), 50.);
    emit item->setWidth(100.);
    QCOMPARE(item->width(), 100.);
    QCOMPARE(item->property("tested").toBool(), false); //Should not have received signal to change property

    connections->setEnabled(true); //Re-enable the connectSignals()
    QCOMPARE(item->property("tested").toBool(), false);
    QCOMPARE(item->width(), 100.);
    emit item->setWidth(50.);
    QCOMPARE(item->width(), 50.);
    QCOMPARE(item->property("tested").toBool(), true); //Should have received signal to change property

    delete item;
}

//QTBUG-56499
void tst_qqmlconnections::clearImplicitTarget()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("test-connection-implicit.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(c.create());

    QVERIFY(item != 0);

    // normal case: fire Connections
    item->setWidth(100.);
    QCOMPARE(item->property("tested").toBool(), true);

    item->setProperty("tested", false);
    // clear the implicit target
    QQmlConnections *connections = item->findChild<QQmlConnections*>();
    QVERIFY(connections);
    connections->setTarget(0);

    // target cleared: no longer fire Connections
    item->setWidth(150.);
    QCOMPARE(item->property("tested").toBool(), false);

    delete item;
}

QTEST_MAIN(tst_qqmlconnections)

#include "tst_qqmlconnections.moc"
