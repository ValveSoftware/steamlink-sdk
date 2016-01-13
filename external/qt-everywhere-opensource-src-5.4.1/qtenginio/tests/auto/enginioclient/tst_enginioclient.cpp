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
#include <Enginio/enginiooauth2authentication.h>

#include "../common/common.h"

class tst_EnginioClient: public QObject
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
    void internal_createObjectType();
    void deleteReply();
    void create_todos();
    void update_todos();
    void query_todos();
    void query_todos_filter();
    void query_todos_limit();
    void query_todos_count();
    void query_todos_sort();
    void remove_todos();
    void update_todos_invalidId();
    void users_crud();
    void query_users();
    void query_users_filter();
    void query_users_sort();
    void query_usersgroup_crud();
    void query_usersgroup_limit();
    void query_usersgroup_count();
    void query_usersgroup_sort();
    void query_usersgroupmembers_limit();
    void query_usersgroupmembers_count();
    void query_usersgroupmembers_sort();
    void backendFakeReply();
    void acl();
    void creator_updater();
    void sharingNetworkManager();
    void lifeTimeOfNetworkManager();
    void fullTextSearch();
    void assignUserToGroup();
    void userAgent();

private:
    QString usergroupId(EnginioClient *client)
    {
        static QString id;
        static QString backendId = client->backendId();
        Q_ASSERT(backendId == client->backendId()); // client can be changed but we want to use the same backend id
        if (id.isEmpty()) {
            QJsonObject obj;
            obj["limit"] = 1;
            const EnginioReply *reqId = client->query(obj, Enginio::UsergroupOperation);
            Q_ASSERT(reqId);
            const int __step = 50;
            const int __timeoutValue = 5000;
            for (int __i = 0; __i < __timeoutValue && (reqId->data().isEmpty()); __i+=__step) {
                QTest::qWait(__step);
            }
            id = reqId->data()["results"].toArray().first().toObject()["id"].toString();
            Q_ASSERT(!id.isEmpty());
        }
        return id;
    }

    void prepareForSearch();
};

void tst_EnginioClient::initTestCase()
{
    if (EnginioTests::TESTAPP_URL.isEmpty())
        QFAIL("Needed environment variable ENGINIO_API_URL is not set!");

    _backendName = QStringLiteral("EnginioClient") + QString::number(QDateTime::currentMSecsSinceEpoch());
    QVERIFY(_backendManager.createBackend(_backendName));

    QJsonObject apiKeys = _backendManager.backendApiKeys(_backendName, EnginioTests::TESTAPP_ENV);
    _backendId = apiKeys["backendId"].toString().toUtf8();

    QVERIFY(!_backendId.isEmpty());

    prepareForSearch();
    EnginioTests::prepareTestUsersAndUserGroups(_backendId);

    // create some todos objects
    QJsonObject todos;
    todos["name"] = QStringLiteral("todos");

    QJsonObject title;
    title["name"] = QStringLiteral("title");
    title["type"] = QStringLiteral("string");
    title["indexed"] = true;
    QJsonObject completed;
    completed["name"] = QStringLiteral("completed");
    completed["type"] = QStringLiteral("boolean");
    completed["indexed"] = false;
    QJsonObject count;
    count["name"] = QStringLiteral("count");
    count["type"] = QStringLiteral("number");
    count["indexed"] = false;

    QJsonArray properties;
    properties.append(title);
    properties.append(completed);
    properties.append(count);
    todos["properties"] = properties;

    QVERIFY(_backendManager.createObjectType(_backendName, EnginioTests::TESTAPP_ENV, todos));
    {
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);

        QJsonObject object;
        object["objectType"] = QString::fromUtf8("objects.todos");
        object["title"] = QString::fromUtf8("init todo 1");
        object["completed"] = false;
        EnginioReply* reply1 = client.create(object);
        object["title"] = QString::fromUtf8("init todo 2");
        EnginioReply* reply2 = client.create(object);
        QTRY_VERIFY(reply1->isFinished());
        CHECK_NO_ERROR(reply1);
        QTRY_VERIFY(reply2->isFinished());
        CHECK_NO_ERROR(reply2);
    }
}

void tst_EnginioClient::cleanupTestCase()
{
    QVERIFY(_backendManager.removeBackend(_backendName));
}

void tst_EnginioClient::internal_createObjectType()
{
    EnginioTests::EnginioBackendManager backendManager;
    QJsonObject schema;
    schema["name"] = QStringLiteral("places");
    QJsonArray array;
    QJsonObject title;
    title["name"] = QStringLiteral("title");
    title["type"] = QStringLiteral("string");
    title["indexed"] = false;
    QJsonObject photo;
    photo["name"] = QStringLiteral("photo");
    photo["type"] = QStringLiteral("ref");
    photo["objectType"] = QStringLiteral("files");
    array.append(title);
    array.append(photo);
    schema["properties"] = array;
    QVERIFY(backendManager.createObjectType(_backendName, EnginioTests::TESTAPP_ENV, schema));
}

void tst_EnginioClient::query_todos()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply *)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));
    //![query-todo]
    QJsonObject query;
    query["objectType"] = QString::fromUtf8("objects.todos");
    EnginioReply *reply = client.query(query);
    //![query-todo]
    QVERIFY(reply);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reply);
    CHECK_NO_ERROR(response);
    QVERIFY(!response->data().isEmpty());
    QVERIFY(!response->data()["results"].isUndefined());

    {   // try to query the object with an id
        QJsonArray results = reply->data()["results"].toArray();
        QVERIFY(results.count() > 1); // the test assumes that querying by id will return less items.
        QString id = results[0].toObject()["id"].toString();
        query["id"] = id;
        reply = client.query(query);
        QTRY_VERIFY(reply->isFinished());
        CHECK_NO_ERROR(reply);
        QCOMPARE(reply->data()["id"].toString(), id);
    }
}

void tst_EnginioClient::query_todos_filter()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["objectType"] = QString::fromUtf8("objects.todos");
    obj["query"] = QJsonDocument::fromJson("{\"completed\": true}").object();
    const EnginioReply* reqId = client.query(obj);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(!data["results"].isUndefined());
    QJsonArray array = data["results"].toArray();
    foreach (QJsonValue value, array)
        QVERIFY(value.toObject()["completed"].toBool());
}

void tst_EnginioClient::query_todos_limit()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["objectType"] = QString::fromUtf8("objects.todos");
    obj["limit"] = 1;
    const EnginioReply* reqId = client.query(obj);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(data["results"].isArray());
    QCOMPARE(data["results"].toArray().count(), 1);
}

void tst_EnginioClient::query_todos_count()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["objectType"] = QString::fromUtf8("objects.todos");
    obj["count"] = true;
    const EnginioReply* reqId = client.query(obj);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(data.contains("count"));
}

void tst_EnginioClient::query_todos_sort()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["objectType"] = QString::fromUtf8("objects.todos");
    obj["limit"] = 400;
    obj["sort"] = QJsonDocument::fromJson(
                QByteArrayLiteral("[{\"sortBy\": \"title\", \"direction\": \"asc\"}, {\"sortBy\": \"createdAt\", \"direction\": \"asc\"}]")).array();
    const EnginioReply* reqId = client.query(obj);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    if (reqId->networkError())
        qDebug() << reqId->data();
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QJsonArray results = data["results"].toArray();
    QVERIFY(results.count());
    QString previousTitle, currentTitle;
    QString previousCreatedAt, currentCreatedAt;
    for (int i = 0; i < results.count(); ++i) {
        currentTitle = results[i].toObject()["title"].toString();
        currentCreatedAt = results[i].toObject()["createdAt"].toString();
        QVERIFY(currentTitle > previousTitle
                || (currentTitle == previousTitle && currentCreatedAt >= previousCreatedAt));
        previousTitle = currentTitle;
        previousCreatedAt = currentCreatedAt;
    }
}

void tst_EnginioClient::query_users()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    const EnginioReply* reqId = client.query(obj, Enginio::UserOperation);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(!data["results"].isUndefined());
    QVERIFY(data["results"].toArray().count());
}

void tst_EnginioClient::query_users_sort()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    {
        QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
        QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));
        QJsonObject obj;
        obj["sort"] = QJsonDocument::fromJson(QByteArrayLiteral("[{\"sortBy\": \"createdAt\", \"direction\": \"asc\"}]")).array();
        const EnginioReply *reqId = client.query(obj, Enginio::UserOperation);
        QVERIFY(reqId);

        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spyError.count(), 0);

        const EnginioReply *response = spy[0][0].value<EnginioReply*>();
        QCOMPARE(response, reqId);
        CHECK_NO_ERROR(response);
        QJsonObject data = response->data();
        QVERIFY(!data.isEmpty());
        QVERIFY(!data["results"].isUndefined());
        QJsonArray results = data["results"].toArray();
        QVERIFY(results.count());
        QString previous, current;
        for (int i = 0; i < results.count(); ++i) {
            current = results[i].toObject()["createdAt"].toString();
            QVERIFY(current >= previous);
            previous = current;
        }
    }
    {
        QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
        QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));
        QJsonObject obj;
        obj["sort"] = QJsonDocument::fromJson(QByteArrayLiteral("[{\"sortBy\": \"createdAt\", \"direction\": \"desc\"}]")).array();
        const EnginioReply *reqId = client.query(obj, Enginio::UserOperation);
        QVERIFY(reqId);

        QTRY_COMPARE(spy.count(), 1);
        if (reqId->networkError())
            qDebug() << reqId->data();
        QCOMPARE(spyError.count(), 0);

        const EnginioReply *response = spy[0][0].value<EnginioReply*>();
        QCOMPARE(response, reqId);
        CHECK_NO_ERROR(response);
        QJsonObject data = response->data();
        QVERIFY(!data.isEmpty());
        QVERIFY(!data["results"].isUndefined());
        QJsonArray results = data["results"].toArray();
        QVERIFY(results.count());
        QString previous, current;
        for (int i = 0; i < results.count(); ++i) {
            current = results[i].toObject()["createdAt"].toString();
            QVERIFY(current <= previous || previous.isEmpty());
            previous = current;
        }
    }
}

void tst_EnginioClient::query_usersgroup_crud()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QString groupName = QStringLiteral("users");
    QString tmpGroupName = QString::number(QDateTime::currentMSecsSinceEpoch());
    QString description = QStringLiteral("Temporary Usergroup");
    QJsonObject data;
    data["name"] = tmpGroupName;

    EnginioReply *reply = client.create(data, Enginio::UsergroupOperation);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);
    QVERIFY(reply);
    QVERIFY(!reply->data().isEmpty());
    QCOMPARE(reply->data()["name"].toString(), tmpGroupName);

    QVERIFY(!reply->data()["id"].isUndefined());
    spy.clear();

    QString id = reply->data()["id"].toString();
    QJsonObject query;
    query["query"] = data;

    reply = client.query(query, Enginio::UsergroupOperation);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);
    QVERIFY(reply);
    QVERIFY(!reply->data().isEmpty());
    QVERIFY(!reply->data()["results"].isUndefined());
    QJsonArray results = reply->data()["results"].toArray();
    QCOMPARE(results.count(), 1);
    QCOMPARE(results[0].toObject()["id"].toString(), id);
    spy.clear();


    data["id"] = id;
    data["description"] = description;
    reply = client.update(data, Enginio::UsergroupOperation);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);
    QVERIFY(reply);
    QVERIFY(!reply->data().isEmpty());
    QVERIFY(!reply->data()["name"].isUndefined());
    QCOMPARE(reply->data()["name"].toString(), tmpGroupName);
    QCOMPARE(reply->data()["description"].toString(), description);
    spy.clear();

    data = QJsonObject();
    data["id"] = id;
    reply = client.remove(data, Enginio::UsergroupOperation);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);
    QVERIFY(reply);
}

void tst_EnginioClient::query_usersgroup_limit()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["limit"] = 1;
    const EnginioReply *reqId = client.query(obj, Enginio::UsergroupOperation);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(!data["results"].isUndefined());
    QVERIFY(data["results"].toArray().count());
}

void tst_EnginioClient::query_usersgroup_count()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["count"] = 1;
    const EnginioReply *reqId = client.query(obj, Enginio::UsergroupOperation);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(data.contains("count"));
}

void tst_EnginioClient::query_usersgroup_sort()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["sort"] = QJsonDocument::fromJson(QByteArrayLiteral("[{\"sortBy\": \"createdAt\", \"direction\": \"asc\"}]")).array();
    const EnginioReply *reqId = client.query(obj, Enginio::UsergroupOperation);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(!data["results"].isUndefined());
    QJsonArray results = data["results"].toArray();
    QVERIFY(results.count());
    QString previous, current;
    for (int i = 0; i < results.count(); ++i) {
        current = results[i].toObject()["createdAt"].toString();
        QVERIFY(current >= previous);
        previous = current;
    }
}

void tst_EnginioClient::query_usersgroupmembers_limit()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString id = usergroupId(&client);
    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["limit"] = 1;
    obj["id"] = id;
    const EnginioReply *reqId = client.query(obj, Enginio::UsergroupMembersOperation);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(!data["results"].isUndefined());
    QVERIFY(data["results"].toArray().count() <= 1);
}

void tst_EnginioClient::query_usersgroupmembers_count()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString id = usergroupId(&client);
    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["count"] = 1;
    obj["id"] = id;
    const EnginioReply *reqId = client.query(obj, Enginio::UsergroupMembersOperation);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(data.contains("count"));
}

void tst_EnginioClient::query_usersgroupmembers_sort()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString id = usergroupId(&client);
    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["id"] = id;
    obj["sort"] = QJsonDocument::fromJson(QByteArrayLiteral("[{\"sortBy\": \"createdAt\", \"direction\": \"desc\"}]")).array();
    const EnginioReply *reqId = client.query(obj, Enginio::UsergroupMembersOperation);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(!data["results"].isUndefined());
    QJsonArray results = data["results"].toArray();
    QString previous, current;
    for (int i = 0; i < results.count(); ++i) {
        current = results[i].toObject()["createdAt"].toString();
        QVERIFY(current >= previous);
        previous = current;
    }
}
void tst_EnginioClient::query_users_filter()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj = QJsonDocument::fromJson("{\"query\":{\"username\": \"john\"}}").object();
    QVERIFY(!obj.isEmpty());
    const EnginioReply* reqId = client.query(obj, Enginio::UserOperation);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QVERIFY(!data["results"].isUndefined());
    QVERIFY(!data["results"].toArray().count());
}

void tst_EnginioClient::fullTextSearch()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId("52fb5b6de5bde556dc03b107");

    {
        QJsonObject searchQuery = QJsonDocument::fromJson(
                    "{\"objectTypes\": [\"objects.fullTextSearch1\"],"
                    "\"search\": {\"phrase\": \"cat\"}}").object();

        QVERIFY(!searchQuery.isEmpty());
        EnginioReply *reply = client.fullTextSearch(searchQuery);
        QVERIFY(reply);

        QTRY_VERIFY(reply->isFinished());
        CHECK_NO_ERROR(reply);

        QJsonObject data = reply->data();
        QVERIFY(!data.isEmpty());
        QVERIFY(!data["results"].isUndefined());
        QCOMPARE(data["results"].toArray().count(), 2);
    }
    {
        QJsonObject searchQuery = QJsonDocument::fromJson(
                    "{\"objectTypes\": [\"objects.fullTextSearch1\", \"objects.fullTextSearch2\"],"
                    "\"search\": {\"phrase\": \"cat OR dog\", \"properties\": [\"stringValue\"]}}").object();

        QVERIFY(!searchQuery.isEmpty());
        EnginioReply *reply = client.fullTextSearch(searchQuery);
        QVERIFY(reply);

        QTRY_VERIFY(reply->isFinished());
        CHECK_NO_ERROR(reply);

        QJsonObject data = reply->data();
        QVERIFY(!data.isEmpty());
        QVERIFY(!data["results"].isUndefined());
        QCOMPARE(data["results"].toArray().count(), 4);
    }
}

void tst_EnginioClient::assignUserToGroup()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString userId;
    QString groupId;

    // Preparations
    {
        // find a user
        QJsonObject query;
        EnginioReply *reply = client.query(query, Enginio::UserOperation);
        QTRY_VERIFY(reply->isFinished());
        CHECK_NO_ERROR(reply);

        QJsonArray results = reply->data()["results"].toArray();
        QVERIFY(results.count());

        QJsonObject user = results[0].toObject();
        QVERIFY(user.contains("id"));
        userId = user["id"].toString();
    }
    {
        //find a group
        QJsonObject query;
        EnginioReply *reply = client.query(query, Enginio::UsergroupOperation);
        QTRY_VERIFY(reply->isFinished());
        CHECK_NO_ERROR(reply);

        QJsonArray results = reply->data()["results"].toArray();
        QVERIFY(results.count());

        QJsonObject group = results[0].toObject();
        QVERIFY(group.contains("id"));
        groupId = group["id"].toString();
    }
    QVERIFY(!userId.isEmpty());
    QVERIFY(!groupId.isEmpty());

    {   // check if given user is not a member of the usergroup
        QJsonObject op;
        op.insert("id", groupId);
        QJsonObject query;
        query.insert("id", userId);
        EnginioReply *reply = client.query(op, Enginio::UsergroupMembersOperation);
        QTRY_VERIFY(reply->isFinished());
        QJsonArray results = reply->data()["results"].toArray();
        QCOMPARE(results.count(), 0);
    }
    {
        //![create-newmember]
        QJsonObject query;
        query["id"] = groupId;
        QJsonObject user;
        user["id"] = userId;
        user["objectType"] = QString::fromUtf8("users");
        query["member"] = user;

        EnginioReply *reply = client.create(query, Enginio::UsergroupMembersOperation);
        //![create-newmember]
        QTRY_VERIFY(reply->isFinished());
        CHECK_NO_ERROR(reply);
        QCOMPARE(reply->data()["id"].toString(), userId);
    }
    {
        // confirm operation
        QJsonObject op;
        op.insert("id", groupId);
        QJsonObject query;
        query.insert("id", userId);
        EnginioReply *reply = client.query(op, Enginio::UsergroupMembersOperation);
        QTRY_VERIFY(reply->isFinished());
        QJsonArray results = reply->data()["results"].toArray();
        QCOMPARE(results.count(), 1);
        QCOMPARE(results[0].toObject()["id"].toString(), userId);
    }
}

static void checkUserAgent(QNetworkReply *reply)
{
    QNetworkRequest request = reply->request();
    QVERIFY(request.rawHeaderList().contains("User-Agent"));
    QByteArray agent = request.rawHeader("User-Agent");
    QVERIFY(!agent.isEmpty());
    QVERIFY(agent.contains("Qt:"));
    QVERIFY(agent.contains("Enginio:"));
    QVERIFY(agent.contains("Language:"));
}

void tst_EnginioClient::userAgent()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QNetworkAccessManager *qnam = client.networkManager();
    QObject::connect(qnam, &QNetworkAccessManager::finished, &client, checkUserAgent);

    QVector<EnginioReply *> replies;
    replies.append(client.query(QJsonObject(), Enginio::UserOperation));
    replies.append(client.create(QJsonObject(), Enginio::UserOperation));

    foreach (EnginioReply *reply, replies)
        QTRY_VERIFY(reply->isFinished());
}

struct DeleteReplyCountHelper
{
    QSet<QString> &requests;
    int &counter;
    void operator ()(QNetworkReply *reply)
    {
        QString requestId(reply->request().rawHeader("X-Request-Id"));
        if (requests.contains(requestId))
            ++counter;
    }
};

void tst_EnginioClient::deleteReply()
{
    // This test may be a bit fragile, the main point of it is to test if
    // directly deleting a reply is not causing a crash. We do not do
    // any guaranties about the behavior. The test relays on fact that QNetworkReply
    // is not deleted if not finished, so we can wait for the finish signal and
    // compare request id, if we catch all then we are sure that everything went ok
    // if not we can not say anything.
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QNetworkAccessManager *qnam = client.networkManager();

    QVector<EnginioReply *> replies;
    replies.append(client.query(QJsonObject(), Enginio::UserOperation));
    replies.append(client.create(QJsonObject(), Enginio::UserOperation));

    QSet<QString> requests;
    requests.reserve(replies.count());
    foreach (EnginioReply *r, replies) {
        requests.insert(r->requestId());
    }

    int counter = 0;
    DeleteReplyCountHelper handler = { requests, counter };
    struct DeleteReplyDisconnectHelper
    {
        QMetaObject::Connection _connection;
        ~DeleteReplyDisconnectHelper()
        {
           QObject::disconnect(_connection);
        }
    } connection = {QObject::connect(qnam, &QNetworkAccessManager::finished, handler)};

    // it is not supported but we should not crash
    qDeleteAll(replies);

    QTRY_COMPARE(counter, replies.count());
}

void tst_EnginioClient::create_todos()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    obj["objectType"] = QString::fromUtf8("objects.todos");
    obj["title"] = QString::fromUtf8("test title");
    obj["completed"] = false;
    const EnginioReply* reqId = client.create(obj);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QCOMPARE(data["completed"], obj["completed"]);
    QCOMPARE(data["title"], obj["title"]);
    QCOMPARE(data["objectType"], obj["objectType"]);
}

void tst_EnginioClient::users_crud()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QString name = QUuid::createUuid().toString();
    QString pass = QString::fromUtf8("Metaphysics");
    QString id;
    {
        // CREATE
        int spyCount = spy.count();
        QJsonObject obj;
        obj["username"] = name;
        obj["password"] = pass;

        const EnginioReply* reqId = client.create(obj, Enginio::UserOperation);
        QVERIFY(reqId);
        QCOMPARE(reqId->parent(), &client);

        QTRY_COMPARE(spy.count(), spyCount + 1);
        QCOMPARE(spyError.count(), 0);

        const EnginioReply *response = spy[0][0].value<EnginioReply*>();
        QJsonObject data = response->data();
        QCOMPARE(response, reqId);
        CHECK_NO_ERROR(response);
        QVERIFY(!data.isEmpty());
        QCOMPARE(data["username"], obj["username"]);
        QVERIFY(!data["id"].toString().isEmpty());
        id = data["id"].toString();
    }
    {
        // READ
        int spyCount = spy.count();
        QJsonObject query;
        query["username"] = name;
        QJsonObject obj;
        obj["query"] = query;
        const EnginioReply* reqId = client.query(obj, Enginio::UserOperation);
        QVERIFY(reqId);
        QCOMPARE(reqId->parent(), &client);

        QTRY_COMPARE(spy.count(), spyCount + 1);
        QCOMPARE(spyError.count(), 0);
        CHECK_NO_ERROR(reqId);
        QJsonArray data = reqId->data()["results"].toArray();
        QCOMPARE(data.count(), 1);
        QCOMPARE(data[0].toObject()["id"].toString(), id);
        QCOMPARE(data[0].toObject()["username"].toString(), name);
    }
    {
        // UPDATE
        int spyCount = spy.count();
        QJsonObject obj;
        obj["password"] = pass + "pass";
        obj["id"] = id;
        const EnginioReply* reqId = client.update(obj, Enginio::UserOperation);
        QVERIFY(reqId);
        QCOMPARE(reqId->parent(), &client);

        QTRY_COMPARE(spy.count(), spyCount + 1);
        QCOMPARE(spyError.count(), 0);
        CHECK_NO_ERROR(reqId);
        QJsonObject data = reqId->data();
        QCOMPARE(data["id"].toString(), id);
        QCOMPARE(data["username"].toString(), name);
    }
    {
        int spyCount = spy.count();
        // DELETE it
        QJsonObject obj;
        obj["id"] = id;
        const EnginioReply* reqId = client.remove(obj, Enginio::UserOperation);
        QVERIFY(reqId);
        QCOMPARE(reqId->parent(), &client);

        QTRY_COMPARE(spy.count(), spyCount + 1);
        QCOMPARE(spyError.count(), 0);
    }
}

void tst_EnginioClient::update_todos()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject query;
    query["objectType"] = QString::fromUtf8("objects.todos");
    query["limit"] = 1;
    const EnginioReply* reqId = client.query(query);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QJsonObject data = response->data();
    QVERIFY(!data.isEmpty());
    QJsonObject object = data["results"].toArray().first().toObject();

    object["completed"] = !object["completed"].toBool();
    reqId = client.update(object);

    QVERIFY(reqId);
    QTRY_COMPARE(spy.count(), 2);
    QCOMPARE(spyError.count(), 0);
    response = spy[1][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    data = response->data();
    QCOMPARE(data["title"], object["title"]);
    QCOMPARE(data["objectType"], object["objectType"]);
    QCOMPARE(data["completed"], object["completed"]);
    QCOMPARE(response->backendStatus(), 200);
}

void tst_EnginioClient::update_todos_invalidId()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject object;
    object["objectType"] = QString::fromUtf8("objects.todos");
    object["id"] = QString::fromUtf8("INVALID_ID");
    const EnginioReply* reqId = client.update(object);

    QVERIFY(reqId);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 1);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    QCOMPARE(response->networkError(), QNetworkReply::ContentNotFoundError);
    QVERIFY(!response->errorString().isEmpty());
    QCOMPARE(reqId, spyError[0][0].value<EnginioReply*>());
    // TODO how ugly is that, improve JSON api
    QJsonObject data = response->data();
    QVERIFY(data["errors"].isArray());
    QVERIFY(!data["errors"].toArray()[0].toObject()["message"].toString().isEmpty());
    QVERIFY(!data["errors"].toArray()[0].toObject()["reason"].toString().isEmpty());
    QCOMPARE(response->backendStatus(), 404);
}

void tst_EnginioClient::remove_todos()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));
    QString objectId;
    {
    //![create-todo]
    QJsonObject query;
    query["objectType"] = QString::fromUtf8("objects.todos");
    query["title"] = QString::fromUtf8("A todo");
    query["completed"] = true;
    const EnginioReply* response = client.create(query);
    //![create-todo]
    QVERIFY(response);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    // confirm that a new object was created
    const EnginioReply *spyResponse = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, spyResponse);
    CHECK_NO_ERROR(spyResponse);
    objectId = response->data()["id"].toString();
    QVERIFY(!objectId.isEmpty());
    }

    {
    //![remove-todo]
    QJsonObject query;
    query["objectType"] = QString::fromUtf8("objects.todos");
    query["id"] = objectId;
    const EnginioReply *response = client.remove(query);
    //![remove-todo]

    QVERIFY(response);
    QTRY_COMPARE(spy.count(), 2);
    QCOMPARE(spyError.count(), 0);
    const EnginioReply *spyResponse = spy[1][0].value<EnginioReply*>();
    QCOMPARE(response, spyResponse);
    CHECK_NO_ERROR(spyResponse);
    }

    // it seems that object was deleted but lets try to query for it
    QJsonObject checkQuery;
    QJsonObject constructQuery;
    constructQuery["id"] = objectId;
    checkQuery["query"] = constructQuery;
    checkQuery["objectType"] = QString::fromUtf8("objects.todos");
    const EnginioReply *reqId = client.query(checkQuery);
    QVERIFY(reqId);
    QTRY_COMPARE(spy.count(), 3);
    QCOMPARE(spyError.count(), 0);
    const EnginioReply *response = spy[2][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    QVERIFY(response->data()["results"].toArray().isEmpty());
}

void tst_EnginioClient::backendFakeReply()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spyClientFinished(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyClientError(&client, SIGNAL(error(EnginioReply*)));
    QSignalSpy spyReplyFinished(&client, SIGNAL(finished(EnginioReply*)));

    QJsonObject empty;
    QJsonObject objectTypeOnly;
    objectTypeOnly["objectType"] = QString::fromUtf8("objects.todos");
    QJsonArray objectTypes;
    objectTypes.append(QString::fromUtf8("objects.todos"));
    QJsonObject objectWithObjectTypes;
    objectWithObjectTypes["objectTypes"] = objectTypes;

    QVERIFY(client.query(empty, Enginio::ObjectOperation));
    QVERIFY(client.query(empty, Enginio::AccessControlOperation));
    QVERIFY(client.query(objectTypeOnly, Enginio::AccessControlOperation));
    QVERIFY(client.query(empty, Enginio::UsergroupMembersOperation));

    QVERIFY(client.update(empty, Enginio::ObjectOperation));
    QVERIFY(client.update(empty, Enginio::AccessControlOperation));
    QVERIFY(client.update(objectTypeOnly, Enginio::ObjectOperation));
    QVERIFY(client.update(objectTypeOnly, Enginio::AccessControlOperation));
    QVERIFY(client.update(empty, Enginio::UsergroupMembersOperation));

    QVERIFY(client.remove(empty, Enginio::ObjectOperation));
    QVERIFY(client.remove(empty, Enginio::AccessControlOperation));
    QVERIFY(client.remove(objectTypeOnly, Enginio::ObjectOperation));
    QVERIFY(client.remove(objectTypeOnly, Enginio::AccessControlOperation));
    QVERIFY(client.remove(empty, Enginio::UsergroupMembersOperation));

    QVERIFY(client.fullTextSearch(empty));
    QVERIFY(client.fullTextSearch(objectTypeOnly));
    QVERIFY(client.fullTextSearch(objectWithObjectTypes));

    QVERIFY(client.downloadUrl(empty));
    QVERIFY(client.uploadFile(empty, QUrl()));

    QTRY_COMPARE(spyClientFinished.count(), 19);
    QCOMPARE(spyClientError.count(), spyClientFinished.count());
    QCOMPARE(spyReplyFinished.count(), spyClientFinished.count());

    for (int i = 0; i < spyClientFinished.count(); ++i) {
        EnginioReply *reply = spyClientFinished[i][0].value<EnginioReply*>();
        QVERIFY(reply->isFinished());
        QVERIFY(reply->isError());
        QCOMPARE(reply->parent(), &client);

        QJsonObject data = reply->data();
        QVERIFY(!data.isEmpty());

        QVERIFY(!data["errors"].toArray()[0].toObject()["message"].toString().isEmpty());
        QVERIFY(!data["errors"].toArray()[0].toObject()["reason"].toString().isEmpty());

        QCOMPARE(reply->errorType(), Enginio::BackendError);
        QVERIFY(reply->networkError() != QNetworkReply::NoError);
        QCOMPARE(reply->backendStatus(), 400);

        QCOMPARE(spyReplyFinished[i][0].value<EnginioReply*>(), spyClientFinished[i][0].value<EnginioReply*>());
    }
}

void tst_EnginioClient::acl()
{
    // create an object
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    EnginioOAuth2Authentication identity;
    identity.setUser("logintest");
    identity.setPassword("logintest");
    client.setIdentity(&identity);

    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QString id1, id2, id3; // ids of 3 different users id1 is our login
    QJsonParseError parseError;
    {
        QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
        QJsonObject userQuery = QJsonDocument::fromJson("{\"query\": {\"username\": {\"$in\": [\"logintest\", \"logintest2\",\"logintest3\"]}},"
                                                        "\"sort\": [{\"sortBy\":\"username\", \"direction\": \"asc\"}]}", &parseError).object();
        QCOMPARE(parseError.error, QJsonParseError::NoError);

        const EnginioReply* reqId = client.query(userQuery, Enginio::UserOperation);
        QVERIFY(reqId);
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spyError.count(), 0);
        QJsonArray data = reqId->data()["results"].toArray();
        QCOMPARE(data.count(), 3);
        id1 = data[0].toObject()["id"].toString();
        id2 = data[1].toObject()["id"].toString();
        id3 = data[2].toObject()["id"].toString();
        QVERIFY(!id1.isEmpty());
        QVERIFY(!id2.isEmpty());
        QVERIFY(!id3.isEmpty());
    }

    // wait for authentication, acl requires that
    QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));

    // create an object
    QJsonObject obj;
    obj["objectType"] = QString::fromUtf8("objects.todos");
    obj["title"] = QString::fromUtf8("test title");
    obj["completed"] = false;
    const EnginioReply* reqId = client.create(obj);
    QVERIFY(reqId);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spyError.count(), 0);

    const EnginioReply *response = spy[0][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
    obj = response->data(); // so obj contains valid id

    // view acl of the object
    reqId = client.query(obj, Enginio::AccessControlOperation);
    QVERIFY(reqId);
    QTRY_COMPARE(spy.count(), 2);
    response = spy[1][0].value<EnginioReply*>();
    QCOMPARE(spyError.count(), 0);
    QCOMPARE(response, reqId);
    qDebug() << response->data();
    CHECK_NO_ERROR(response);

    QJsonObject data(response->data());
    QVERIFY(data["admin"].isArray());
    QVERIFY(data["read"].isArray());
    QVERIFY(data["delete"].isArray());
    QVERIFY(data["update"].isArray());

    // update acl of the object
    //![update-access]
    QJsonObject aclUpdate;
    aclUpdate["objectType"] = obj["objectType"];
    aclUpdate["id"] = obj["id"];
    QString json = "{ \"read\": [ { \"id\": \"%3\", \"objectType\": \"users\" } ],"
                     "\"update\": [ { \"id\": \"%2\", \"objectType\": \"users\" } ],"
                     "\"admin\": [ { \"id\": \"%1\", \"objectType\": \"users\" } ] }";
    json = json.arg(id1, id2, id3);
    aclUpdate["access"] = QJsonDocument::fromJson(json.toUtf8()).object();

    reqId = client.update(aclUpdate, Enginio::AccessControlOperation);
    //![update-access]

    QVERIFY(reqId);
    QTRY_COMPARE(spy.count(), 3);
    QCOMPARE(spyError.count(), 0);
    response = spy[2][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);

    // confirm that the response is correct
    data = response->data();
    QVERIFY(data["admin"].isArray());
    QVERIFY(data["read"].isArray());
    QVERIFY(data["delete"].isArray());
    QVERIFY(data["update"].isArray());

    bool valid = false;
    foreach(QJsonValue value, data["read"].toArray()) {
        if (value.toObject()["id"] == id3) {
            valid = true;
            break;
        }
    }
    QVERIFY(valid);

    valid = false;
    foreach(QJsonValue value, data["update"].toArray()) {
        if (value.toObject()["id"] == id2) {
            valid = true;
            break;
        }
    }
    QVERIFY(valid);

    valid = false;
    foreach(QJsonValue value, data["admin"].toArray()) {
        if (value.toObject()["id"] == id1) {
            valid = true;
            break;
        }
    }
    QVERIFY(valid);

    // view acl again to confirm the change on the server side
    reqId = client.query(obj, Enginio::AccessControlOperation);
    QVERIFY(reqId);
    QTRY_COMPARE(spy.count(), 4);
    QCOMPARE(spyError.count(), 0);
    response = spy[3][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);

    data = response->data();
    QVERIFY(data["admin"].isArray());
    QVERIFY(data["read"].isArray());
    QVERIFY(data["delete"].isArray());
    QVERIFY(data["update"].isArray());
    valid = false;
    foreach(QJsonValue value, data["read"].toArray()) {
        if (value.toObject()["id"] == id3) {
            valid = true;
            break;
        }
    }
    QVERIFY(valid);

    valid = false;
    foreach(QJsonValue value, data["update"].toArray()) {
        if (value.toObject()["id"] == id2) {
            valid = true;
            break;
        }
    }
    QVERIFY(valid);

    valid = false;
    foreach(QJsonValue value, data["admin"].toArray()) {
        if (value.toObject()["id"] == id1) {
            valid = true;
            break;
        }
    }
    QVERIFY(valid);
    // it seems to work fine, let's delete the acl we created
    reqId = client.remove(aclUpdate, Enginio::AccessControlOperation);
    QVERIFY(reqId);
    QTRY_COMPARE(spy.count(), 5);
    QCOMPARE(spyError.count(), 0);
    response = spy[4][0].value<EnginioReply*>();
    QCOMPARE(response, reqId);
    CHECK_NO_ERROR(response);
}

void tst_EnginioClient::creator_updater()
{
    // create an object
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    EnginioOAuth2Authentication identity;
    identity.setUser("logintest");
    identity.setPassword("logintest");
    client.setIdentity(&identity);

    QString id1, id2;
    QJsonParseError parseError;
    {
        QJsonObject userQuery = QJsonDocument::fromJson("{\"query\": {\"username\": {\"$in\": [\"logintest\", \"logintest2\"]}},"
                                                        "\"sort\": [{\"sortBy\":\"username\", \"direction\": \"asc\"}]}", &parseError).object();
        QCOMPARE(parseError.error, QJsonParseError::NoError);

        const EnginioReply* reqId = client.query(userQuery, Enginio::UserOperation);
        QVERIFY(reqId);
        QTRY_VERIFY(reqId->isFinished());
        CHECK_NO_ERROR(reqId);

        QJsonArray data = reqId->data()["results"].toArray();
        QCOMPARE(data.count(), 2);
        id1 = data[0].toObject()["id"].toString();
        id2 = data[1].toObject()["id"].toString();
        QVERIFY(!id1.isEmpty());
        QVERIFY(!id2.isEmpty());
    }

    // Make sure we are authenticated and thus have a session
    QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);

    // create an object
    QJsonObject obj;
    obj["objectType"] = QString::fromUtf8("objects.todos");
    obj["title"] = QString::fromUtf8("test title");
    obj["completed"] = false;
    const EnginioReply *reqId = client.create(obj);
    QVERIFY(reqId);

    QTRY_VERIFY(reqId->isFinished());
    CHECK_NO_ERROR(reqId);
    obj = reqId->data(); // so obj contains valid id

    QVERIFY(obj.contains("creator"));
    QJsonObject creator = obj["creator"].toObject();
    QCOMPARE(creator["id"].toString(), id1);
    QCOMPARE(creator["objectType"].toString(), QString::fromLatin1("users"));

    QVERIFY(obj.contains("updater"));
    QJsonObject updater = obj["updater"].toObject();
    QCOMPARE(updater["id"].toString(), id1);
    QCOMPARE(updater["objectType"].toString(), QString::fromLatin1("users"));

    // Change user and update the object
    EnginioOAuth2Authentication identity2;
    identity2.setUser("logintest2");
    identity2.setPassword("logintest2");
    client.setIdentity(&identity2);
    QCOMPARE(client.authenticationState(), Enginio::Authenticating);
    QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);

    obj["completed"] = true;
    const EnginioReply *updateReply = client.update(obj);
    QVERIFY(updateReply);

    QTRY_VERIFY(updateReply->isFinished());
    CHECK_NO_ERROR(updateReply);

    QJsonObject updatedObj = updateReply->data();
    QVERIFY(updatedObj.contains("creator"));
    creator = updatedObj["creator"].toObject();
    QCOMPARE(creator["id"].toString(), id1);
    QCOMPARE(creator["objectType"].toString(), QString::fromLatin1("users"));

    QVERIFY(updatedObj.contains("updater"));
    updater = updatedObj["updater"].toObject();
    QCOMPARE(updater["id"].toString(), id2);
    QCOMPARE(updater["objectType"].toString(), QString::fromLatin1("users"));
}

void tst_EnginioClient::sharingNetworkManager()
{
    // Check sharing of network acces manager in different enginio clients
    EnginioClient *e1 = new EnginioClient;
    EnginioClient *e2 = new EnginioClient;
    QCOMPARE(e1->networkManager(), e2->networkManager());

    // QNAM can not be shared accross threads
    struct EnginioInThread : public QThread {
        QNetworkAccessManager *_qnam;
        EnginioInThread(QNetworkAccessManager *qnam)
            : _qnam(qnam)
        {}
        void run()
        {
            EnginioClient e3;
            QVERIFY(_qnam != e3.networkManager());
        }
    } thread(e1->networkManager());
    thread.start();
    QVERIFY(thread.wait(10000));

    // check if deleting qnam creator is not invalidating qnam itself.
    delete e1;
    QVERIFY(e2->networkManager());
    e2->networkManager()->children(); // should not crash
    delete e2;
}

struct NetworkManagerDestroySignalSpy
{
    bool &called;
    void operator()() { called = true; }
};

void tst_EnginioClient::lifeTimeOfNetworkManager()
{
    struct : public QThread {
        void run()
        {
            bool result = false;
            NetworkManagerDestroySignalSpy spy = { result };
            {
                EnginioClient client;
                QNetworkAccessManager *qnam = client.networkManager();
                QVERIFY(qnam);
                QObject::connect(qnam, &QNetworkAccessManager::destroyed, spy);
            }
            QVERIFY(spy.called);
            // we need to repeat the operation to confirm that a qnam instance will
            // be re-created when needed.
            result = false;
            {
                EnginioClient client;
                QNetworkAccessManager *qnam = client.networkManager();
                QVERIFY(qnam);
                QObject::connect(qnam, &QNetworkAccessManager::destroyed, spy);
            }
            QVERIFY(spy.called);
        }
    } thread;
    thread.start();
    QVERIFY(thread.wait(10000));
}

void tst_EnginioClient::prepareForSearch()
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

    QJsonArray properties;
    properties.append(intValue);
    properties.append(stringValue);
    customObject1["properties"] = properties;
    customObject2["properties"] = properties;

    QVERIFY(_backendManager.createObjectType(_backendName, EnginioTests::TESTAPP_ENV, customObject1));
    QVERIFY(_backendManager.createObjectType(_backendName, EnginioTests::TESTAPP_ENV, customObject2));

    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    qDebug("Populating backend with data");
    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));
    int iterations = 5;

    {
        QJsonObject obj;
        obj["objectType"] = QString::fromUtf8("objects.").append(EnginioTests::CUSTOM_OBJECT1);
        EnginioReply *reply = client.query(obj);
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spyError.count(), 0);
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
        }

        spy.clear();
    }
    {
        QJsonObject obj;
        obj["objectType"] = QString::fromUtf8("objects.").append(EnginioTests::CUSTOM_OBJECT2);
        EnginioReply *reply = client.query(obj);
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spyError.count(), 0);
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
        }
    }
}

QTEST_MAIN(tst_EnginioClient)
#include "tst_enginioclient.moc"
