/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtTest/QtTest>
#include <QtCore/qobject.h>
#include <QtCore/qthread.h>

#include <Enginio/enginioclient.h>
#include <Enginio/enginioreply.h>
#include <Enginio/enginioidentity.h>

#include <Enginio/private/enginiobackendconnection_p.h>

#include "../common/common.h"

class tst_Notifications: public QObject
{
    Q_OBJECT

    QString _backendName;
    EnginioTests::EnginioBackendManager _backendManager;
    QByteArray _backendId;

public slots:
    void error(EnginioReply *reply) {
        qDebug() << "\n\n### ERROR";
        qDebug() << reply->errorString();
        reply->dumpDebugInfo();
        qDebug() << "\n###\n";
    }

private slots:
    void initTestCase();
    void cleanupTestCase();
    void populateWithData();
    void update_objects();
    void remove_objects();

private:
    void createObjectSchema();
};

void tst_Notifications::createObjectSchema()
{
    QJsonObject customObject1;
    customObject1["name"] = EnginioTests::CUSTOM_OBJECT1;
    QJsonObject customObject2;
    customObject2["name"] = EnginioTests::CUSTOM_OBJECT2;

    QJsonObject intValue;
    intValue["name"] = QStringLiteral("intValue");
    intValue["type"] = QStringLiteral("number");
    intValue["indexed"] = false;
    QJsonObject stringValue;
    stringValue["name"] = QStringLiteral("stringValue");
    stringValue["type"] = QStringLiteral("string");
    stringValue["indexed"] = true;

    QJsonObject settings;
    settings["websocket"] = true;
    QJsonArray properties;
    properties.append(intValue);
    properties.append(stringValue);
    customObject1["properties"] = properties;
    customObject2["properties"] = properties;
    customObject1["settings"] = settings;
    customObject2["settings"] = settings;

    QVERIFY(_backendManager.createObjectType(_backendName, EnginioTests::TESTAPP_ENV, customObject1));
    QVERIFY(_backendManager.createObjectType(_backendName, EnginioTests::TESTAPP_ENV, customObject2));
}

void tst_Notifications::initTestCase()
{
    if (EnginioTests::TESTAPP_URL != QStringLiteral("https://staging.engin.io"))
        QSKIP("Notification tests can be executed only against staging");

    _backendName = QStringLiteral("Notifications") + QString::number(QDateTime::currentMSecsSinceEpoch());
    QVERIFY(_backendManager.createBackend(_backendName));

    QJsonObject apiKeys = _backendManager.backendApiKeys(_backendName, EnginioTests::TESTAPP_ENV);

    _backendId = apiKeys["backendId"].toString().toUtf8();

    QVERIFY(!_backendId.isEmpty());

    createObjectSchema();
}

void tst_Notifications::cleanupTestCase()
{
    if (EnginioTests::TESTAPP_URL != QStringLiteral("https://staging.engin.io"))
        QSKIP("Notification tests can be executed only against staging");
    QVERIFY(_backendManager.removeBackend(_backendName));
}

void tst_Notifications::populateWithData()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    EnginioBackendConnection connection;
    QSignalSpy notificationSpy(&connection, SIGNAL(dataReceived(QJsonObject)));

    QJsonObject filter;
    filter["event"] = QStringLiteral("create");
    connection.connectToBackend(&client, filter);
    QTRY_VERIFY(connection.isConnected());

    QSignalSpy pongSpy(&connection, SIGNAL(pong()));
    connection.ping();
    QTRY_VERIFY(pongSpy.count());

    qDebug("Populating backend with data");
    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));
    int iterations = 10;
    int createdObjectCount = 0;
    QSet<QString> requestIds;

    {
        QJsonObject obj;
        obj["objectType"] = QString::fromUtf8("objects.").append(EnginioTests::CUSTOM_OBJECT1);
        EnginioReply *reply = client.query(obj);
        QTRY_VERIFY(reply->isFinished());
        CHECK_NO_ERROR(reply);
        QVERIFY2(!reply->data().isEmpty(), "Empty data in EnginioReply.");
        QVERIFY2(!reply->data()["results"].isUndefined(), "Undefined results array in EnginioReply data.");
        bool create = reply->data()["results"].toArray().isEmpty();
        spy.clear();

        if (create) {
            for (int i = 0; i < iterations; ++i)
            {
                obj["intValue"] = i;
                obj["stringValue"] = QString::fromUtf8("Query object");
                client.create(obj);
            }

            QTRY_COMPARE_WITH_TIMEOUT(spy.count(), iterations, 10000);
            QCOMPARE(spyError.count(), 0);
            createdObjectCount += iterations;

            for (int i = 0; i < spy.count(); ++i)
            {
                EnginioReply *ereply = spy[i][0].value<EnginioReply*>();
                QVERIFY(!ereply->requestId().isEmpty());
                QString apiRequestId = ereply->requestId();
                QVERIFY(!requestIds.contains(apiRequestId));
                requestIds.insert(apiRequestId);
            }
        }

        spy.clear();
    }
    {
        QJsonObject obj;
        obj["objectType"] = QString::fromUtf8("objects.").append(EnginioTests::CUSTOM_OBJECT2);
        EnginioReply *reply = client.query(obj);
        QTRY_VERIFY(reply->isFinished());
        CHECK_NO_ERROR(reply);
        QVERIFY2(!reply->data().isEmpty(), "Empty data in EnginioReply.");
        QVERIFY2(!reply->data()["results"].isUndefined(), "Undefined results array in EnginioReply data.");
        bool create = reply->data()["results"].toArray().isEmpty();
        spy.clear();

        if (create) {
            for (int i = 0; i < iterations; ++i)
            {
                obj["intValue"] = i;
                obj["stringValue"] = QString::fromUtf8("object test");
                client.create(obj);
            }

            QTRY_COMPARE_WITH_TIMEOUT(spy.count(), iterations, 10000);
            QCOMPARE(spyError.count(), 0);
            createdObjectCount += iterations;

            for (int i = 0; i < spy.count(); ++i)
            {
                EnginioReply *ereply = spy[i][0].value<EnginioReply*>();
                QVERIFY(!ereply->requestId().isEmpty());
                QString apiRequestId = ereply->requestId();
                QVERIFY(!requestIds.contains(apiRequestId));
                requestIds.insert(apiRequestId);
            }
        }
    }

    QTRY_COMPARE_WITH_TIMEOUT(notificationSpy.count(), createdObjectCount, 10000);

    for (int i = 0; i < notificationSpy.count(); ++i)
    {
        QJsonObject message = notificationSpy[i][0].value<QJsonObject>();
        QVERIFY(!message.isEmpty());
        QVERIFY(message["messageType"].isString());
        QVERIFY(message["messageType"].toString() == QStringLiteral("data"));
        QVERIFY(message["event"].isString());
        QVERIFY(message["event"].toString() == filter["event"].toString());

        QVERIFY(message["origin"].isObject());
        QJsonObject origin = message["origin"].toObject();
        QVERIFY(origin["apiRequestId"].isString());
        QString apiRequestId = origin["apiRequestId"].toString();
        QVERIFY(requestIds.contains(apiRequestId));
    }

    notificationSpy.clear();
    EnginioBackendConnection::WebSocketCloseStatus closeStatus = EnginioBackendConnection::GoingAwayCloseStatus;
    connection.close(closeStatus);
    QTRY_VERIFY_WITH_TIMEOUT(!connection.isConnected(), 10000);
    QCOMPARE(notificationSpy.count(), 1);

    QJsonObject message = notificationSpy[0][0].value<QJsonObject>();

    // We initiated the closing, so the server should send us
    // closeStatus in the closing message.
    QVERIFY(!message.isEmpty());
    QVERIFY(message["messageType"].isString());
    QVERIFY(message["messageType"].toString() == QStringLiteral("close"));
    QVERIFY(message["status"].isDouble());
    QVERIFY(message["status"].toDouble() == closeStatus);
}

void tst_Notifications::update_objects()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    EnginioBackendConnection connection;
    QSignalSpy notificationSpy(&connection, SIGNAL(dataReceived(QJsonObject)));

    QJsonObject filter;
    filter["event"] = QStringLiteral("update");
    connection.connectToBackend(&client, filter);
    QTRY_VERIFY(connection.isConnected());

    QSignalSpy pongSpy(&connection, SIGNAL(pong()));
    connection.ping();
    QTRY_VERIFY(pongSpy.count());

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject query;
    query["objectType"] = QString::fromUtf8("objects.").append(EnginioTests::CUSTOM_OBJECT1);
    const EnginioReply* reply = client.query(query);
    QVERIFY(reply);

    QTRY_VERIFY(reply->isFinished());
    CHECK_NO_ERROR(reply);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reply);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QJsonArray array = data["results"].toArray();

    spy.clear();

    foreach (const QJsonValue value, array)
    {
        QVERIFY(value.isObject());
        QJsonObject obj = value.toObject();
        obj["stringValue"] = QString::fromUtf8("Update");
        client.update(obj);
    }

    QTRY_COMPARE(spy.count(), array.count());
    QCOMPARE(spyError.count(), 0);

    QSet<QString> requestIds;

    for (int i = 0; i < spy.count(); ++i)
    {
        EnginioReply *ereply = spy[i][0].value<EnginioReply*>();
        QVERIFY(!ereply->requestId().isEmpty());
        QString apiRequestId = ereply->requestId();
        QVERIFY(!requestIds.contains(apiRequestId));
        requestIds.insert(apiRequestId);
    }

    QTRY_COMPARE_WITH_TIMEOUT(notificationSpy.count(), array.count(), 10000);

    for (int i = 0; i < notificationSpy.count(); ++i)
    {
        QJsonObject message = notificationSpy[i][0].value<QJsonObject>();
        QVERIFY(!message.isEmpty());
        QVERIFY(message["messageType"].isString());
        QVERIFY(message["messageType"].toString() == QStringLiteral("data"));
        QVERIFY(message["event"].isString());
        QVERIFY(message["event"].toString() == filter["event"].toString());

        QVERIFY(message["origin"].isObject());
        QJsonObject origin = message["origin"].toObject();
        QVERIFY(origin["apiRequestId"].isString());
        QString apiRequestId = origin["apiRequestId"].toString();
        QVERIFY(requestIds.contains(apiRequestId));
    }

    notificationSpy.clear();
    EnginioBackendConnection::WebSocketCloseStatus closeStatus = EnginioBackendConnection::GoingAwayCloseStatus;
    connection.close(closeStatus);
    QTRY_VERIFY_WITH_TIMEOUT(!connection.isConnected(), 10000);
    QCOMPARE(notificationSpy.count(), 1);

    QJsonObject message = notificationSpy[0][0].value<QJsonObject>();

    // We initiated the closing, so the server should send us
    // closeStatus in the closing message.
    QVERIFY(!message.isEmpty());
    QVERIFY(message["messageType"].isString());
    QVERIFY(message["messageType"].toString() == QStringLiteral("close"));
    QVERIFY(message["status"].isDouble());
    QVERIFY(message["status"].toDouble() == closeStatus);
}

void tst_Notifications::remove_objects()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    EnginioBackendConnection connection;
    QSignalSpy notificationSpy(&connection, SIGNAL(dataReceived(QJsonObject)));

    QJsonObject filter;
    filter["event"] = QStringLiteral("delete");
    connection.connectToBackend(&client, filter);
    QTRY_VERIFY(connection.isConnected());

    QSignalSpy pongSpy(&connection, SIGNAL(pong()));
    connection.ping();
    QTRY_VERIFY(pongSpy.count());

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject query;
    query["objectType"] = QString::fromUtf8("objects.").append(EnginioTests::CUSTOM_OBJECT1);
    const EnginioReply* reply = client.query(query);
    QVERIFY(reply);

    QTRY_VERIFY(reply->isFinished());
    CHECK_NO_ERROR(reply);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reply);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QJsonArray array = data["results"].toArray();

    spy.clear();

    foreach (const QJsonValue value, array)
    {
        QVERIFY(value.isObject());
        QJsonObject obj = value.toObject();
        query["id"] = obj["id"];
        client.remove(query);
    }

    QTRY_COMPARE(spy.count(), array.count());
    QCOMPARE(spyError.count(), 0);

    QSet<QString> requestIds;

    for (int i = 0; i < spy.count(); ++i)
    {
        EnginioReply *ereply = spy[i][0].value<EnginioReply*>();
        QVERIFY(!ereply->requestId().isEmpty());
        QString apiRequestId = ereply->requestId();
        QVERIFY(!requestIds.contains(apiRequestId));
        requestIds.insert(apiRequestId);
    }

    QTRY_COMPARE_WITH_TIMEOUT(notificationSpy.count(), array.count(), 10000);

    for (int i = 0; i < notificationSpy.count(); ++i)
    {
        QJsonObject message = notificationSpy[i][0].value<QJsonObject>();
        QVERIFY(!message.isEmpty());
        QVERIFY(message["messageType"].isString());
        QVERIFY(message["messageType"].toString() == QStringLiteral("data"));
        QVERIFY(message["event"].isString());
        QVERIFY(message["event"].toString() == filter["event"].toString());

        QVERIFY(message["origin"].isObject());
        QJsonObject origin = message["origin"].toObject();
        QVERIFY(origin["apiRequestId"].isString());
        QString apiRequestId = origin["apiRequestId"].toString();
        QVERIFY(requestIds.contains(apiRequestId));
    }

    notificationSpy.clear();
    EnginioBackendConnection::WebSocketCloseStatus closeStatus = EnginioBackendConnection::GoingAwayCloseStatus;
    connection.close(closeStatus);
    QTRY_VERIFY_WITH_TIMEOUT(!connection.isConnected(), 10000);
    QCOMPARE(notificationSpy.count(), 1);

    QJsonObject message = notificationSpy[0][0].value<QJsonObject>();

    // We initiated the closing, so the server should send us
    // closeStatus in the closing message.
    QVERIFY(!message.isEmpty());
    QVERIFY(message["messageType"].isString());
    QVERIFY(message["messageType"].toString() == QStringLiteral("close"));
    QVERIFY(message["status"].isDouble());
    QVERIFY(message["status"].toDouble() == closeStatus);
}

QTEST_MAIN(tst_Notifications)
#include "tst_notifications.moc"
