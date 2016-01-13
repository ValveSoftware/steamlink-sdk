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

#include <Enginio/enginioclient.h>
#include <Enginio/enginioreply.h>
#include <Enginio/private/enginiostring_p.h>
#include <Enginio/private/enginioclient_p.h>
#include <QtCore/qjsonarray.h>
#include <QtTest/QSignalSpy>
#include <QtTest/QtTest>

#include "common.h"

namespace EnginioTests {

static const QByteArray getRequest = QByteArrayLiteral("GET");
static const QByteArray postRequest = QByteArrayLiteral("POST");
static const QByteArray deleteRequest = QByteArrayLiteral("DELETE");

struct PrintAllErrors
{
    QSignalSpy *_spy;
    PrintAllErrors(QSignalSpy *spy)
        : _spy(spy)
    {
        Q_ASSERT(spy);
    }

    ~PrintAllErrors()
    {
        if (!_spy->isEmpty()) {
            qDebug() << "SignalSpy caught errors:";
            for (int i = 0; i < _spy->count(); ++i) {
                EnginioReply *reply = _spy->at(i).first().value<EnginioReply*>();
                reply->dumpDebugInfo();
            }
        }
    }
};

EnginioBackendManager::EnginioBackendManager(QObject *parent)
    : QObject(parent)
{
    _client.setServiceUrl(EnginioTests::TESTAPP_URL);
    QString credentialsFileName = qgetenv("ENGINIO_CREDENTIALS_FILE_PATH");
    if (!credentialsFileName.isEmpty()) {
        QFile credentialsFile(credentialsFileName);
        if (!credentialsFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Could not open a file" << credentialsFileName << ". Backend setup failed!";
            return;
        }
        _email = credentialsFile.readLine().trimmed();
        _password = credentialsFile.readLine().trimmed();
        if (_email.isEmpty() || _password.isEmpty()) {
            qDebug("ENGINIO_CREDENTIALS_FILE_PATH environment variable was specified but we failed to load needed data. Backend setup failed!");
            return;
        }
    } else {
        _email = qgetenv("ENGINIO_EMAIL_ADDRESS");
        _password = qgetenv("ENGINIO_LOGIN_PASSWORD");
        if (_email.isEmpty() || _password.isEmpty()) {
            qDebug("Needed environment variables ENGINIO_EMAIL_ADDRESS, ENGINIO_LOGIN_PASSWORD are not set. Backend setup failed!");
            return;
        }
    }

    _headers["Accept"] = QStringLiteral("application/json");

    QObject::connect(&_client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    QObject::connect(&_client, SIGNAL(finished(EnginioReply *)), this, SLOT(finished(EnginioReply *)));

    if (!authenticate())
        qDebug("ERROR: Session authentication failed!");
}

EnginioBackendManager::~EnginioBackendManager()
{}

void EnginioBackendManager::finished(EnginioReply *reply)
{
    Q_ASSERT(reply);
    _responseData = reply->data();
}

void EnginioBackendManager::error(EnginioReply *reply)
{
    Q_ASSERT(reply);
    qDebug() << "\n\n### ERROR";
    qDebug() << reply->errorString();
    reply->dumpDebugInfo();
    qDebug() << "\n###\n";
}

bool EnginioBackendManager::synchronousRequest(const QUrl &url, const QByteArray &httpOperation, const QJsonObject &data)
{
    QSignalSpy finishedSpy(&_client, SIGNAL(finished(EnginioReply *)));
    QSignalSpy errorSpy(&_client, SIGNAL(error(EnginioReply *)));
    PrintAllErrors printErrors(&errorSpy);
    _responseData = QJsonObject();
    _client.customRequest(url, httpOperation, data);
    return finishedSpy.wait(30000) && !errorSpy.count();
}

bool EnginioBackendManager::authenticate()
{
    QByteArray data;
    {
        QUrlQuery urlQuery;
        urlQuery.addQueryItem(EnginioString::grant_type, EnginioString::password);
        urlQuery.addQueryItem(EnginioString::username, _email);
        urlQuery.addQueryItem(EnginioString::password, _password);
        data = urlQuery.query().toUtf8();
    }
    QUrl url(_client.serviceUrl());
    url.setPath(QStringLiteral("/v1/account/auth/oauth2/token"));

    QNetworkRequest request(EnginioClientConnectionPrivate::get(&_client)->prepareRequest(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, EnginioString::Application_x_www_form_urlencoded);
    request.setRawHeader(EnginioString::Accept, EnginioString::Application_json);
    request.setUrl(url);

    QNetworkReply *reply = _client.networkManager()->post(request, data);
    QSignalSpy spy(reply, SIGNAL(finished()));
    spy.wait(20000);

    _responseData = QJsonDocument::fromJson(reply->readAll()).object();
    QString sessionToken = _responseData[EnginioString::access_token].toString();
    _headers[EnginioString::Authorization] = EnginioString::Bearer_ + sessionToken;

    reply->deleteLater();
    return !sessionToken.isEmpty();
}

QString EnginioBackendManager::getAppId(const QString &backendName)
{
    QString appId = _backendEnvironments.value(backendName).first().toObject()["appId"].toString();

    if (!appId.isEmpty())
        return appId;

    QJsonArray results = getAllBackends();
    foreach (const QJsonValue& value, results) {
        QJsonObject data = value.toObject();
        if (data["name"].toString() == backendName) {
            appId = data["id"].toString();
            break;
        }
    }

    return appId;
}

QJsonArray EnginioBackendManager::getAllBackends()
{
    QJsonObject obj;
    obj["headers"] = _headers;
    QUrl url(_client.serviceUrl());
    url.setPath("/v1/account/apps");

    synchronousRequest(url, getRequest, obj);
    return _responseData["results"].toArray();
}

QJsonArray EnginioBackendManager::getEnvironments(const QString &backendName)
{
    QJsonArray environments = _backendEnvironments.value(backendName);

    if (!environments.isEmpty())
        return environments;

    QString appPath = QStringLiteral("/v1/account/apps/");
    QString appId = getAppId(backendName);
    appPath.append(appId);
    QJsonObject obj;
    obj["headers"] = _headers;
    QUrl url(_client.serviceUrl());
    url.setPath(appPath);

    if (synchronousRequest(url, getRequest, obj)) {
        environments = _responseData["environments"].toArray();
        _backendEnvironments[backendName] = environments;
    }

    return environments;
}

bool EnginioBackendManager::removeAppWithId(const QString &appId)
{
    if (appId.isEmpty())
        return false;

    QJsonObject obj;
    obj["headers"] = _headers;
    QString appsPath = QStringLiteral("/v1/account/apps/");
    appsPath.append(appId);
    QUrl url(_client.serviceUrl());
    url.setPath(appsPath);
    return synchronousRequest(url, deleteRequest, obj);
}

bool EnginioBackendManager::createBackend(const QString &backendName)
{
    qDebug("## Creating backend: %s", backendName.toUtf8().data());

    QJsonObject obj;
    obj["headers"] = _headers;
    QJsonObject backend;
    backend["name"] = backendName;
    obj["payload"] = backend;
    QUrl url(_client.serviceUrl());
    url.setPath("/v1/account/apps");

    if (!synchronousRequest(url, postRequest, obj))
        return false;

    _backendEnvironments[backendName] = _responseData["environments"].toArray();
    return true;
}

bool EnginioBackendManager::removeBackend(const QString &backendName)
{
    qDebug("## Deleting backend: %s", backendName.toUtf8().data());
    if (!removeAppWithId(getAppId(backendName)))
        return false;

    _backendEnvironments.remove(backendName);
    return true;
}

bool EnginioBackendManager::createObjectType(const QString &backendName, const QString &environment, const QJsonObject &schema)
{
    qDebug("## Creating new object type: %s on backend: %s", schema["name"].toString().toUtf8().data(), backendName.toUtf8().data());
    QJsonArray environments = getEnvironments(backendName);
    if (environments.isEmpty())
        return false;

    QString backendId;
    QString backendMasterKey;

    foreach (const QJsonValue &value, environments) {
        QJsonObject env = value.toObject();
        if (env["name"].toString() == environment) {
            backendId = env["id"].toString();
            QJsonArray masterKeys = env["masterKeys"].toArray();
            QJsonObject masterKey = masterKeys.first().toObject();
            backendMasterKey = masterKey["key"].toString();
            break;
        }
    }

    _headers["Enginio-Backend-Id"] = backendId;
    _headers["Enginio-Backend-MasterKey"] = backendMasterKey;

    QJsonObject obj;
    obj["headers"] = _headers;
    obj["payload"] = schema;

    QUrl url(_client.serviceUrl());
    url.setPath("/v1/object_types");
    synchronousRequest(url, postRequest, obj);
    return !_responseData["properties"].toArray().isEmpty();
}

QJsonObject EnginioBackendManager::backendApiKeys(const QString &backendName, const QString &environment)
{
    QJsonArray environments = getEnvironments(backendName);

    if (environments.isEmpty())
        return QJsonObject();

    QString backendId;

    foreach (const QJsonValue &value, environments) {
        QJsonObject env = value.toObject();
        if (env["name"].toString() == environment) {
            backendId = env["id"].toString();
            QJsonArray keys = env["apiKeys"].toArray();
            QJsonObject apiKey = keys.first().toObject();
            break;
        }
    }

    QJsonObject apiKeys;
    apiKeys["backendId"] = backendId;

    return apiKeys;
}

void prepareTestUsersAndUserGroups(const QByteArray& backendId)
{
    // Create some users to be used in later tests
    EnginioClient client;
    client.setBackendId(backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));
    PrintAllErrors printErrors(&spyError);

    QJsonObject obj;
    int spyCount = spy.count();

    const int UserCount = 8;
    for (int i = 0; i < UserCount; ++i) {
        QJsonObject query;
        query["username"] = QStringLiteral("logintest") + (i ? QString::number(i) : "");
        obj["query"] = query;
        client.query(obj, Enginio::UserOperation);
        ++spyCount;
    }

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), spyCount, 15000);
    QCOMPARE(spyError.count(), 0);

    for (int i = 0; i < UserCount; ++i) {
        EnginioReply *reply = spy[i][0].value<EnginioReply*>();
        QVERIFY(reply);
        QVERIFY(!reply->data().isEmpty());
        QVERIFY(!reply->data()["query"].isUndefined());
        obj = reply->data()["query"].toObject();
        QVERIFY(!obj.isEmpty());
        obj = obj["query"].toObject();
        QVERIFY(!obj.isEmpty());
        QString identity = obj["username"].toString();
        QVERIFY(!identity.isEmpty());
        QVERIFY(!reply->data()["results"].isUndefined());
        QJsonArray data = reply->data()["results"].toArray();
        if (!data.count()) {
            QJsonObject query;
            query["username"] = identity;
            query["password"] = identity;
            query["email"] = identity + "@email.com";
            client.create(query, Enginio::UserOperation);
            ++spyCount;
        }
    }

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), spyCount, 15000);
    QCOMPARE(spyError.count(), 0);


    // Create user groups for later tests if the backend does not have them yet.
    spy.clear();
    spyCount = 0;

    const int UserGroupCount = 8;
    for (int i = 0; i < UserGroupCount; ++i) {
        QJsonObject query;
        query["name"] = "usergroup" + (i ? QString::number(i) : "");
        obj["query"] = query;
        client.query(obj, Enginio::UsergroupOperation);
        ++spyCount;
    }

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), spyCount, 15000);
    QCOMPARE(spyError.count(), 0);

    for (int i = 0; i < UserGroupCount; ++i) {
        EnginioReply *reply = spy[i][0].value<EnginioReply*>();
        QVERIFY(reply);
        QVERIFY(!reply->data().isEmpty());
        QVERIFY(!reply->data()["query"].isUndefined());
        obj = reply->data()["query"].toObject();
        QVERIFY(!obj.isEmpty());
        obj = obj["query"].toObject();
        QVERIFY(!obj.isEmpty());
        QString groupName = obj["name"].toString();
        QVERIFY(!groupName.isEmpty());
        QVERIFY(!reply->data()["results"].isUndefined());
        QJsonArray data = reply->data()["results"].toArray();
        if (!data.count()) {
            QJsonObject query;
            query["name"] = groupName;
            client.create(query, Enginio::UsergroupOperation);
            ++spyCount;
        }
    }

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), spyCount, 15000);
    QCOMPARE(spyError.count(), 0);
}

bool prepareTestObjectType(const QString &backendName)
{
    QJsonObject schema;
    schema["name"] = CUSTOM_OBJECT1;

    QJsonObject settings;
    settings["websocket"] = true;
    schema["settings"] = settings;

    QJsonObject testCase;
    testCase["name"] = QStringLiteral("testCase");
    testCase["type"] = QStringLiteral("string");
    testCase["indexed"] = false;
    QJsonObject title;
    title["name"] = QStringLiteral("title");
    title["type"] = QStringLiteral("string");
    title["indexed"] = false;
    QJsonObject count;
    count["name"] = QStringLiteral("count");
    count["type"] = QStringLiteral("number");
    count["indexed"] = false;
    QJsonObject file;
    file["name"] = QStringLiteral("fileAttachment");
    file["type"] = QStringLiteral("ref");
    file["objectType"] = QStringLiteral("files");
    file["indexed"] = true;
    QJsonObject processor;
    processor["type"] = QStringLiteral("image");
    QJsonObject thumbnail;
    thumbnail["crop"] = QStringLiteral("20x20");
    QJsonObject variants;
    variants["thumbnail"] = thumbnail;
    processor["variants"] = variants;
    QJsonArray processors;
    processors.append(processor);
    file["processors"] = processors;

    QJsonArray properties;
    properties.append(testCase);
    properties.append(title);
    properties.append(count);
    properties.append(file);
    schema["properties"] = properties;

    qDebug() << schema;

    EnginioTests::EnginioBackendManager backendManager;
    return backendManager.createObjectType(backendName, EnginioTests::TESTAPP_ENV, schema);
}
} // namespace EnginioTests
