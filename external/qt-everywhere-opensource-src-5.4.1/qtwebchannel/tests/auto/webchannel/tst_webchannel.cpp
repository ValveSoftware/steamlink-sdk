/****************************************************************************
**
** Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "tst_webchannel.h"

#include <qwebchannel.h>
#include <qwebchannel_p.h>
#include <qmetaobjectpublisher_p.h>

#include <QtTest>

QT_USE_NAMESPACE

TestWebChannel::TestWebChannel(QObject *parent)
    : QObject(parent)
    , m_dummyTransport(new DummyTransport(this))
    , m_lastInt(0)
    , m_lastDouble(0)
{
}

TestWebChannel::~TestWebChannel()
{

}

void TestWebChannel::setInt(int i)
{
    m_lastInt = i;
}

void TestWebChannel::setDouble(double d)
{
    m_lastDouble = d;
}

void TestWebChannel::setVariant(const QVariant &v)
{
    m_lastVariant = v;
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
    channel.d_func()->publisher->initializeClients();

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
    const QJsonObject info = channel.d_func()->publisher->classInfoForObject(&obj);

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
}

void TestWebChannel::testDisconnect()
{
    QWebChannel channel;
    channel.connectTo(m_dummyTransport);
    channel.disconnectFrom(m_dummyTransport);
    m_dummyTransport->emitMessageReceived(QJsonObject());
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
            channel.d_func()->publisher->classInfoForObject(object);
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
        publisher->initializeClients();

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
    channel.d_func()->publisher->initializeClients();

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

QTEST_MAIN(TestWebChannel)
