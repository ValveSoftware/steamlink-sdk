/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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

#include "tst_webchannel.h"

#include <qwebchannel.h>
#include <qwebchannel_p.h>
#include <qmetaobjectpublisher_p.h>

#include <QtTest>
#ifdef WEBCHANNEL_TESTS_CAN_USE_JS_ENGINE
#include <QJSEngine>
#endif

QT_USE_NAMESPACE

#ifdef WEBCHANNEL_TESTS_CAN_USE_JS_ENGINE
class TestJSEngine;

class TestEngineTransport : public QWebChannelAbstractTransport
{
    Q_OBJECT
public:
    TestEngineTransport(TestJSEngine *);
    void sendMessage(const QJsonObject &message) Q_DECL_OVERRIDE;

    Q_INVOKABLE void channelSetupReady();
    Q_INVOKABLE void send(const QByteArray &message);
private:
    TestJSEngine *m_testEngine;
};

class ConsoleLogger : public QObject
{
    Q_OBJECT
public:
    ConsoleLogger(QObject *parent = 0);

    Q_INVOKABLE void log(const QString &text);
    Q_INVOKABLE void error(const QString &text);

    int errorCount() const { return m_errCount; }
    int logCount() const { return m_logCount; }
    QString lastError() const { return m_lastError; }

private:
    int m_errCount;
    int m_logCount;
    QString m_lastError;

};



ConsoleLogger::ConsoleLogger(QObject *parent)
    : QObject(parent)
    , m_errCount(0)
    , m_logCount(0)
{
}

void ConsoleLogger::log(const QString &text)
{
    m_logCount++;
    qDebug("LOG: %s", qPrintable(text));
}

void ConsoleLogger::error(const QString &text)
{
    m_errCount++;
    m_lastError = text;
    qWarning("ERROR: %s", qPrintable(text));
}


// A test JS engine with convenience integration with WebChannel.
class TestJSEngine : public QJSEngine
{
    Q_OBJECT
public:
    TestJSEngine();

    TestEngineTransport *transport() const;
    ConsoleLogger *logger() const;
    void initWebChannelJS();

signals:
    void channelSetupReady(TestEngineTransport *transport);

private:
    TestEngineTransport *m_transport;
    ConsoleLogger *m_logger;
};

TestEngineTransport::TestEngineTransport(TestJSEngine *engine)
    : QWebChannelAbstractTransport(engine)
    , m_testEngine(engine)
{
}

void TestEngineTransport::sendMessage(const QJsonObject &message)
{
    QByteArray json = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QJSValue callback = m_testEngine->evaluate(QStringLiteral("transport.onmessage"));
    Q_ASSERT(callback.isCallable());
    QJSValue arg = m_testEngine->newObject();
    QJSValue data = m_testEngine->evaluate(QString::fromLatin1("JSON.parse('%1');").arg(QString::fromUtf8(json)));
    Q_ASSERT(!data.isError());
    arg.setProperty(QStringLiteral("data"), data);
    QJSValue val = callback.call((QJSValueList() << arg));
    Q_ASSERT(!val.isError());
}

void TestEngineTransport::channelSetupReady()
{
    emit m_testEngine->channelSetupReady(m_testEngine->transport());
}

void TestEngineTransport::send(const QByteArray &message)
{
    QJsonDocument doc(QJsonDocument::fromJson(message));
    emit messageReceived(doc.object(), this);
}


TestJSEngine::TestJSEngine()
    : m_transport(new TestEngineTransport(this))
    , m_logger(new ConsoleLogger(this))
{
    globalObject().setProperty("transport", newQObject(m_transport));
    globalObject().setProperty("console", newQObject(m_logger));

    QString webChannelJSPath(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
    QFile webChannelJS(webChannelJSPath);
    if (!webChannelJS.open(QFile::ReadOnly))
        qFatal("Error opening qwebchannel.js");
    QString source(QString::fromUtf8(webChannelJS.readAll()));
    evaluate(source, webChannelJSPath);
}

TestEngineTransport *TestJSEngine::transport() const
{
    return m_transport;
}

ConsoleLogger *TestJSEngine::logger() const
{
    return m_logger;
}

void TestJSEngine::initWebChannelJS()
{
    globalObject().setProperty(QStringLiteral("channel"), newObject());
    QJSValue channel = evaluate(QStringLiteral("channel = new QWebChannel(transport, function(channel) { transport.channelSetupReady();});"));
    Q_ASSERT(!channel.isError());
}

#endif // WEBCHANNEL_TESTS_CAN_USE_JS_ENGINE


TestWebChannel::TestWebChannel(QObject *parent)
    : QObject(parent)
    , m_dummyTransport(new DummyTransport(this))
    , m_lastInt(0)
    , m_lastBool(false)
    , m_lastDouble(0)
{
}

TestWebChannel::~TestWebChannel()
{

}

int TestWebChannel::readInt() const
{
    return m_lastInt;
}

void TestWebChannel::setInt(int i)
{
    m_lastInt = i;
    emit lastIntChanged();
}

bool TestWebChannel::readBool() const
{
    return m_lastBool;
}

void TestWebChannel::setBool(bool b)
{
    m_lastBool = b;
    emit lastBoolChanged();
}

double TestWebChannel::readDouble() const
{
    return m_lastDouble;
}

void TestWebChannel::setDouble(double d)
{
    m_lastDouble = d;
    emit lastDoubleChanged();
}

QVariant TestWebChannel::readVariant() const
{
    return m_lastVariant;
}

void TestWebChannel::setVariant(const QVariant &v)
{
    m_lastVariant = v;
    emit lastVariantChanged();
}

QJsonValue TestWebChannel::readJsonValue() const
{
    return m_lastJsonValue;
}

void TestWebChannel::setJsonValue(const QJsonValue& v)
{
    m_lastJsonValue = v;
    emit lastJsonValueChanged();
}

QJsonObject TestWebChannel::readJsonObject() const
{
    return m_lastJsonObject;
}

void TestWebChannel::setJsonObject(const QJsonObject& v)
{
    m_lastJsonObject = v;
    emit lastJsonObjectChanged();
}

QJsonArray TestWebChannel::readJsonArray() const
{
    return m_lastJsonArray;
}

void TestWebChannel::setJsonArray(const QJsonArray& v)
{
    m_lastJsonArray = v;
    emit lastJsonArrayChanged();
}

void TestWebChannel::testRegisterObjects()
{
    QWebChannel channel;
    QObject plain;

    QHash<QString, QObject*> objects;
    objects[QStringLiteral("plain")] = &plain;
    objects[QStringLiteral("channel")] = &channel;
    objects[QStringLiteral("publisher")] = channel.d_func()->publisher;
    objects[QStringLiteral("test")] = this;

    channel.registerObjects(objects);
}

void TestWebChannel::testDeregisterObjects()
{
    QWebChannel channel;
    TestObject testObject;
    testObject.setObjectName("myTestObject");


    channel.registerObject(testObject.objectName(), &testObject);

    channel.connectTo(m_dummyTransport);
    channel.d_func()->publisher->initializeClient(m_dummyTransport);

    QJsonObject connectMessage =
            QJsonDocument::fromJson(("{\"type\": 7,"
                                    "\"object\": \"myTestObject\","
                                    "\"signal\": " + QString::number(testObject.metaObject()->indexOfSignal("sig1()"))
                                    + "}").toLatin1()).object();
    channel.d_func()->publisher->handleMessage(connectMessage, m_dummyTransport);

    emit testObject.sig1();
    channel.deregisterObject(&testObject);
    emit testObject.sig1();
}

void TestWebChannel::testInfoForObject()
{
    TestObject obj;
    obj.setObjectName("myTestObject");

    QWebChannel channel;
    const QJsonObject info = channel.d_func()->publisher->classInfoForObject(&obj, m_dummyTransport);

    QCOMPARE(info.keys(), QStringList() << "enums" << "methods" << "properties" << "signals");

    { // enums
        QJsonObject fooEnum;
        fooEnum["Asdf"] = TestObject::Asdf;
        fooEnum["Bar"] = TestObject::Bar;
        QJsonObject expected;
        expected["Foo"] = fooEnum;
        QCOMPARE(info["enums"].toObject(), expected);
    }

    { // methods & slots
        QJsonArray expected;
        {
            QJsonArray method;
            method.append(QStringLiteral("deleteLater"));
            method.append(obj.metaObject()->indexOfMethod("deleteLater()"));
            expected.append(method);
        }
        {
            QJsonArray method;
            method.append(QStringLiteral("slot1"));
            method.append(obj.metaObject()->indexOfMethod("slot1()"));
            expected.append(method);
        }
        {
            QJsonArray method;
            method.append(QStringLiteral("slot2"));
            method.append(obj.metaObject()->indexOfMethod("slot2(QString)"));
            expected.append(method);
        }
        {
            QJsonArray method;
            method.append(QStringLiteral("setReturnedObject"));
            method.append(obj.metaObject()->indexOfMethod("setReturnedObject(TestObject*)"));
            expected.append(method);
        }
        {
            QJsonArray method;
            method.append(QStringLiteral("setObjectProperty"));
            method.append(obj.metaObject()->indexOfMethod("setObjectProperty(QObject*)"));
            expected.append(method);
        }
        {
            QJsonArray method;
            method.append(QStringLiteral("setProp"));
            method.append(obj.metaObject()->indexOfMethod("setProp(QString)"));
            expected.append(method);
        }
        {
            QJsonArray method;
            method.append(QStringLiteral("fire"));
            method.append(obj.metaObject()->indexOfMethod("fire()"));
            expected.append(method);
        }
        {
            QJsonArray method;
            method.append(QStringLiteral("method1"));
            method.append(obj.metaObject()->indexOfMethod("method1()"));
            expected.append(method);
        }
        QCOMPARE(info["methods"].toArray(), expected);
    }

    { // signals
        QJsonArray expected;
        {
            QJsonArray signal;
            signal.append(QStringLiteral("destroyed"));
            signal.append(obj.metaObject()->indexOfMethod("destroyed(QObject*)"));
            expected.append(signal);
        }
        {
            QJsonArray signal;
            signal.append(QStringLiteral("sig1"));
            signal.append(obj.metaObject()->indexOfMethod("sig1()"));
            expected.append(signal);
        }
        {
            QJsonArray signal;
            signal.append(QStringLiteral("sig2"));
            signal.append(obj.metaObject()->indexOfMethod("sig2(QString)"));
            expected.append(signal);
        }
        {
            QJsonArray signal;
            signal.append(QStringLiteral("replay"));
            signal.append(obj.metaObject()->indexOfMethod("replay()"));
            expected.append(signal);
        }
        QCOMPARE(info["signals"].toArray(), expected);
    }

    { // properties
        QJsonArray expected;
        {
            QJsonArray property;
            property.append(obj.metaObject()->indexOfProperty("objectName"));
            property.append(QStringLiteral("objectName"));
            {
                QJsonArray signal;
                signal.append(1);
                signal.append(obj.metaObject()->indexOfMethod("objectNameChanged(QString)"));
                property.append(signal);
            }
            property.append(obj.objectName());
            expected.append(property);
        }
        {
            QJsonArray property;
            property.append(obj.metaObject()->indexOfProperty("foo"));
            property.append(QStringLiteral("foo"));
            {
                QJsonArray signal;
                property.append(signal);
            }
            property.append(obj.foo());
            expected.append(property);
        }
        {
            QJsonArray property;
            property.append(obj.metaObject()->indexOfProperty("asdf"));
            property.append(QStringLiteral("asdf"));
            {
                QJsonArray signal;
                signal.append(1);
                signal.append(obj.metaObject()->indexOfMethod("asdfChanged()"));
                property.append(signal);
            }
            property.append(obj.asdf());
            expected.append(property);
        }
        {
            QJsonArray property;
            property.append(obj.metaObject()->indexOfProperty("bar"));
            property.append(QStringLiteral("bar"));
            {
                QJsonArray signal;
                signal.append(QStringLiteral("theBarHasChanged"));
                signal.append(obj.metaObject()->indexOfMethod("theBarHasChanged()"));
                property.append(signal);
            }
            property.append(obj.bar());
            expected.append(property);
        }
        {
            QJsonArray property;
            property.append(obj.metaObject()->indexOfProperty("objectProperty"));
            property.append(QStringLiteral("objectProperty"));
            {
                QJsonArray signal;
                signal.append(1);
                signal.append(obj.metaObject()->indexOfMethod("objectPropertyChanged()"));
                property.append(signal);
            }
            property.append(QJsonValue::fromVariant(QVariant::fromValue(obj.objectProperty())));
            expected.append(property);
        }
        {
            QJsonArray property;
            property.append(obj.metaObject()->indexOfProperty("returnedObject"));
            property.append(QStringLiteral("returnedObject"));
            {
                QJsonArray signal;
                signal.append(1);
                signal.append(obj.metaObject()->indexOfMethod("returnedObjectChanged()"));
                property.append(signal);
            }
            property.append(QJsonValue::fromVariant(QVariant::fromValue(obj.returnedObject())));
            expected.append(property);
        }
        {
            QJsonArray property;
            property.append(obj.metaObject()->indexOfProperty("prop"));
            property.append(QStringLiteral("prop"));
            {
                QJsonArray signal;
                signal.append(1);
                signal.append(obj.metaObject()->indexOfMethod("propChanged(QString)"));
                property.append(signal);
            }
            property.append(QJsonValue::fromVariant(QVariant::fromValue(obj.prop())));
            expected.append(property);
        }
        QCOMPARE(info["properties"].toArray(), expected);
    }
}

void TestWebChannel::testInvokeMethodConversion()
{
    QWebChannel channel;
    channel.connectTo(m_dummyTransport);

    QJsonArray args;
    args.append(QJsonValue(1000));

    {
        int method = metaObject()->indexOfMethod("setInt(int)");
        QVERIFY(method != -1);
        channel.d_func()->publisher->invokeMethod(this, method, args);
        QCOMPARE(m_lastInt, args.at(0).toInt());
    }
    {
        int method = metaObject()->indexOfMethod("setBool(bool)");
        QVERIFY(method != -1);
        QJsonArray args;
        args.append(QJsonValue(!m_lastBool));
        channel.d_func()->publisher->invokeMethod(this, method, args);
        QCOMPARE(m_lastBool, args.at(0).toBool());
    }
    {
        int method = metaObject()->indexOfMethod("setDouble(double)");
        QVERIFY(method != -1);
        channel.d_func()->publisher->invokeMethod(this, method, args);
        QCOMPARE(m_lastDouble, args.at(0).toDouble());
    }
    {
        int method = metaObject()->indexOfMethod("setVariant(QVariant)");
        QVERIFY(method != -1);
        channel.d_func()->publisher->invokeMethod(this, method, args);
        QCOMPARE(m_lastVariant, args.at(0).toVariant());
    }
    {
        int method = metaObject()->indexOfMethod("setJsonValue(QJsonValue)");
        QVERIFY(method != -1);
        channel.d_func()->publisher->invokeMethod(this, method, args);
        QCOMPARE(m_lastJsonValue, args.at(0));
    }
    {
        int method = metaObject()->indexOfMethod("setJsonObject(QJsonObject)");
        QVERIFY(method != -1);
        QJsonObject object;
        object["foo"] = QJsonValue(123);
        object["bar"] = QJsonValue(4.2);
        args[0] = object;
        channel.d_func()->publisher->invokeMethod(this, method, args);
        QCOMPARE(m_lastJsonObject, object);
    }
    {
        int method = metaObject()->indexOfMethod("setJsonArray(QJsonArray)");
        QVERIFY(method != -1);
        QJsonArray array;
        array << QJsonValue(123);
        array <<  QJsonValue(4.2);
        args[0] = array;
        channel.d_func()->publisher->invokeMethod(this, method, args);
        QCOMPARE(m_lastJsonArray, array);
    }
}

void TestWebChannel::testSetPropertyConversion()
{
    QWebChannel channel;
    channel.connectTo(m_dummyTransport);

    {
        int property = metaObject()->indexOfProperty("lastInt");
        QVERIFY(property != -1);
        channel.d_func()->publisher->setProperty(this, property, QJsonValue(42));
        QCOMPARE(m_lastInt, 42);
    }
    {
        int property = metaObject()->indexOfProperty("lastBool");
        QVERIFY(property != -1);
        bool newValue = !m_lastBool;
        channel.d_func()->publisher->setProperty(this, property, QJsonValue(newValue));
        QCOMPARE(m_lastBool, newValue);
    }
    {
        int property = metaObject()->indexOfProperty("lastDouble");
        QVERIFY(property != -1);
        channel.d_func()->publisher->setProperty(this, property, QJsonValue(-4.2));
        QCOMPARE(m_lastDouble, -4.2);
    }
    {
        int property = metaObject()->indexOfProperty("lastVariant");
        QVERIFY(property != -1);
        QVariant variant("foo bar asdf");
        channel.d_func()->publisher->setProperty(this, property, QJsonValue::fromVariant(variant));
        QCOMPARE(m_lastVariant, variant);
    }
    {
        int property = metaObject()->indexOfProperty("lastJsonValue");
        QVERIFY(property != -1);
        QJsonValue value("asdf asdf");
        channel.d_func()->publisher->setProperty(this, property, value);
        QCOMPARE(m_lastJsonValue, value);
    }
    {
        int property = metaObject()->indexOfProperty("lastJsonArray");
        QVERIFY(property != -1);
        QJsonArray array;
        array << QJsonValue(-123);
        array <<  QJsonValue(-42);
        channel.d_func()->publisher->setProperty(this, property, array);
        QCOMPARE(m_lastJsonArray, array);
    }
    {
        int property = metaObject()->indexOfProperty("lastJsonObject");
        QVERIFY(property != -1);
        QJsonObject object;
        object["foo"] = QJsonValue(-123);
        object["bar"] = QJsonValue(-4.2);
        channel.d_func()->publisher->setProperty(this, property, object);
        QCOMPARE(m_lastJsonObject, object);
    }
}

void TestWebChannel::testDisconnect()
{
    QWebChannel channel;
    channel.connectTo(m_dummyTransport);
    channel.disconnectFrom(m_dummyTransport);
    m_dummyTransport->emitMessageReceived(QJsonObject());
}

void TestWebChannel::testWrapRegisteredObject()
{
    QWebChannel channel;
    TestObject obj;
    obj.setObjectName("myTestObject");

    channel.registerObject(obj.objectName(), &obj);
    channel.connectTo(m_dummyTransport);
    channel.d_func()->publisher->initializeClient(m_dummyTransport);

    QJsonObject objectInfo = channel.d_func()->publisher->wrapResult(QVariant::fromValue(&obj), m_dummyTransport).toObject();

    QCOMPARE(2, objectInfo.length());
    QVERIFY(objectInfo.contains("id"));
    QVERIFY(objectInfo.contains("__QObject*__"));
    QVERIFY(objectInfo.value("__QObject*__").isBool() && objectInfo.value("__QObject*__").toBool());

    QString returnedId = objectInfo.value("id").toString();

    QCOMPARE(&obj, channel.d_func()->publisher->registeredObjects.value(obj.objectName()));
    QCOMPARE(obj.objectName(), channel.d_func()->publisher->registeredObjectIds.value(&obj));
    QCOMPARE(obj.objectName(), returnedId);
}

void TestWebChannel::testRemoveUnusedTransports()
{
    QWebChannel channel;
    DummyTransport *dummyTransport = new DummyTransport(this);
    TestObject obj;

    channel.connectTo(dummyTransport);
    channel.d_func()->publisher->initializeClient(dummyTransport);

    QMetaObjectPublisher *pub = channel.d_func()->publisher;
    pub->wrapResult(QVariant::fromValue(&obj), dummyTransport);

    QCOMPARE(pub->wrappedObjects.size(), 1);
    QCOMPARE(pub->registeredObjectIds.size(), 1);

    channel.disconnectFrom(dummyTransport);
    delete dummyTransport;

    QCOMPARE(pub->wrappedObjects.size(), 0);
    QCOMPARE(pub->registeredObjectIds.size(), 0);
}

void TestWebChannel::testPassWrappedObjectBack()
{
    QWebChannel channel;
    TestObject registeredObj;
    TestObject returnedObjMethod;
    TestObject returnedObjProperty;

    registeredObj.setObjectName("registeredObject");

    channel.registerObject(registeredObj.objectName(), &registeredObj);
    channel.connectTo(m_dummyTransport);
    channel.d_func()->publisher->initializeClient(m_dummyTransport);

    QMetaObjectPublisher *pub = channel.d_func()->publisher;
    QJsonObject returnedObjMethodInfo = pub->wrapResult(QVariant::fromValue(&returnedObjMethod), m_dummyTransport).toObject();
    QJsonObject returnedObjPropertyInfo = pub->wrapResult(QVariant::fromValue(&returnedObjProperty), m_dummyTransport).toObject();

    QJsonArray argsMethod;
    QJsonObject argMethod0;
    argMethod0["id"] = returnedObjMethodInfo["id"];
    argsMethod << argMethod0;
    QJsonObject argProperty;
    argProperty["id"] = returnedObjPropertyInfo["id"];

    pub->invokeMethod(&registeredObj, registeredObj.metaObject()->indexOfSlot("setReturnedObject(TestObject*)"), argsMethod);
    QCOMPARE(registeredObj.mReturnedObject, &returnedObjMethod);
    pub->setProperty(&registeredObj, registeredObj.metaObject()->indexOfProperty("returnedObject"), argProperty);
    QCOMPARE(registeredObj.mReturnedObject, &returnedObjProperty);
}

void TestWebChannel::testInfiniteRecursion()
{
    QWebChannel channel;
    TestObject obj;
    obj.setObjectProperty(&obj);
    obj.setObjectName("myTestObject");

    channel.connectTo(m_dummyTransport);
    channel.d_func()->publisher->initializeClient(m_dummyTransport);

    QJsonObject objectInfo = channel.d_func()->publisher->wrapResult(QVariant::fromValue(&obj), m_dummyTransport).toObject();
}

void TestWebChannel::testAsyncObject()
{
    QWebChannel channel;
    channel.connectTo(m_dummyTransport);

    QThread thread;
    thread.start();

    TestObject obj;
    obj.moveToThread(&thread);

    QJsonArray args;
    args.append(QJsonValue("message"));

    int method = obj.metaObject()->indexOfMethod("setProp(QString)");
    QVERIFY(method != -1);

    {
        QSignalSpy spy(&obj, &TestObject::propChanged);
        channel.d_func()->publisher->invokeMethod(&obj, method, args);
        QVERIFY(spy.wait());
        QCOMPARE(spy.at(0).at(0).toString(), args.at(0).toString());
    }

    channel.registerObject("myObj", &obj);
    channel.d_func()->publisher->initializeClient(m_dummyTransport);

    QJsonObject connectMessage;
    connectMessage["type"] = 7;
    connectMessage["object"] = "myObj";
    connectMessage["signal"] = obj.metaObject()->indexOfSignal("replay()");
    channel.d_func()->publisher->handleMessage(connectMessage, m_dummyTransport);

    {
        QSignalSpy spy(&obj, &TestObject::replay);
        QMetaObject::invokeMethod(&obj, "fire");
        QVERIFY(spy.wait());
        channel.deregisterObject(&obj);
        QMetaObject::invokeMethod(&obj, "fire");
        QVERIFY(spy.wait());
    }

    thread.quit();
    thread.wait();
}

static QHash<QString, QObject*> createObjects(QObject *parent)
{
    const int num = 100;
    QHash<QString, QObject*> objects;
    objects.reserve(num);
    for (int i = 0; i < num; ++i) {
        objects[QStringLiteral("obj%1").arg(i)] = new BenchObject(parent);
    }
    return objects;
}

void TestWebChannel::benchClassInfo()
{
    QWebChannel channel;
    channel.connectTo(m_dummyTransport);

    QObject parent;
    const QHash<QString, QObject*> objects = createObjects(&parent);

    QBENCHMARK {
        foreach (const QObject *object, objects) {
            channel.d_func()->publisher->classInfoForObject(object, m_dummyTransport);
        }
    }
}

void TestWebChannel::benchInitializeClients()
{
    QWebChannel channel;
    channel.connectTo(m_dummyTransport);

    QObject parent;
    channel.registerObjects(createObjects(&parent));

    QMetaObjectPublisher *publisher = channel.d_func()->publisher;
    QBENCHMARK {
        publisher->initializeClient(m_dummyTransport);

        publisher->propertyUpdatesInitialized = false;
        publisher->signalToPropertyMap.clear();
        publisher->signalHandler.clear();
    }
}

void TestWebChannel::benchPropertyUpdates()
{
    QWebChannel channel;
    channel.connectTo(m_dummyTransport);

    QObject parent;
    const QHash<QString, QObject*> objects = createObjects(&parent);
    QVector<BenchObject*> objectList;
    objectList.reserve(objects.size());
    foreach (QObject *obj, objects) {
        objectList << qobject_cast<BenchObject*>(obj);
    }

    channel.registerObjects(objects);
    channel.d_func()->publisher->initializeClient(m_dummyTransport);

    QBENCHMARK {
        foreach (BenchObject *obj, objectList) {
            obj->change();
        }

        channel.d_func()->publisher->clientIsIdle = true;
        channel.d_func()->publisher->sendPendingPropertyUpdates();
    }
}

void TestWebChannel::benchRegisterObjects()
{
    QWebChannel channel;
    channel.connectTo(m_dummyTransport);

    QObject parent;
    const QHash<QString, QObject*> objects = createObjects(&parent);

    QBENCHMARK {
        channel.registerObjects(objects);
    }
}

void TestWebChannel::benchRemoveTransport()
{
    QWebChannel channel;
    QList<DummyTransport*> dummyTransports;
    for (int i = 500; i > 0; i--)
        dummyTransports.append(new DummyTransport(this));

    QList<QSharedPointer<TestObject>> objs;
    QMetaObjectPublisher *pub = channel.d_func()->publisher;

    foreach (DummyTransport *transport, dummyTransports) {
        channel.connectTo(transport);
        channel.d_func()->publisher->initializeClient(transport);

        /* 30 objects per transport */
        for (int i = 30; i > 0; i--) {
            QSharedPointer<TestObject> obj = QSharedPointer<TestObject>::create();
            objs.append(obj);
            pub->wrapResult(QVariant::fromValue(obj.data()), transport);
        }
    }

    QBENCHMARK_ONCE {
        for (auto transport : dummyTransports)
            pub->transportRemoved(transport);
    }

    qDeleteAll(dummyTransports);
}

#ifdef WEBCHANNEL_TESTS_CAN_USE_JS_ENGINE

class SubclassedTestObject : public TestObject
{
    Q_OBJECT
    Q_PROPERTY(QString bar READ bar WRITE setBar NOTIFY theBarHasChanged)
public:
    void setBar(const QString &newBar);
signals:
    void theBarHasChanged();
};

void SubclassedTestObject::setBar(const QString &newBar)
{
    if (!newBar.isNull())
        emit theBarHasChanged();
}

class TestSubclassedFunctor {
public:
    TestSubclassedFunctor(TestJSEngine *engine)
        : m_engine(engine)
    {
    }

    void operator()() {
        QCOMPARE(m_engine->logger()->errorCount(), 0);
    }

private:
    TestJSEngine *m_engine;
};
#endif // WEBCHANNEL_TESTS_CAN_USE_JS_ENGINE

void TestWebChannel::qtbug46548_overriddenProperties()
{
#ifndef WEBCHANNEL_TESTS_CAN_USE_JS_ENGINE
    QSKIP("A JS engine is required for this test to make sense.");
#else
    SubclassedTestObject obj;
    obj.setObjectName("subclassedTestObject");

    QWebChannel webChannel;
    webChannel.registerObject(obj.objectName(), &obj);
    TestJSEngine engine;
    webChannel.connectTo(engine.transport());
    QSignalSpy spy(&engine, &TestJSEngine::channelSetupReady);
    connect(&engine, &TestJSEngine::channelSetupReady, TestSubclassedFunctor(&engine));
    engine.initWebChannelJS();
    if (!spy.count())
        spy.wait();
    QCOMPARE(spy.count(), 1);
    QJSValue subclassedTestObject = engine.evaluate("channel.objects[\"subclassedTestObject\"]");
    QVERIFY(subclassedTestObject.isObject());

#endif // WEBCHANNEL_TESTS_CAN_USE_JS_ENGINE
}

QTEST_MAIN(TestWebChannel)

#include "tst_webchannel.moc"
