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

#include <QtCore/qstring.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>

#include <Enginio/enginioclient.h>
#include <Enginio/enginioreply.h>
#include <Enginio/enginioidentity.h>

#include "../../common/common.h"

template<class Derived, class Identity>
class IdentityCommonTest
{
    Derived *This() { return static_cast<Derived*>(this); }
protected:
    QString _backendName;
    EnginioTests::EnginioBackendManager _backendManager;
    QByteArray _backendId;

    void initTestCase(const QString &name);
    void cleanupTestCase();
    void error(EnginioReply *reply);

    void identity();
    void identity_changing();
    void identity_invalid();
    void identity_afterLogout(const QByteArray &headerName);
    void queryRestrictedObject();
    void userAndPass();
};

template<class Derived, class Identity>
void IdentityCommonTest<Derived, Identity>::initTestCase(const QString &name)
{
    if (EnginioTests::TESTAPP_URL.isEmpty())
        QFAIL("Needed environment variable ENGINIO_API_URL is not set!");

    _backendName = name + QString::number(QDateTime::currentMSecsSinceEpoch());
    QVERIFY(_backendManager.createBackend(_backendName));

    QJsonObject apiKeys = _backendManager.backendApiKeys(_backendName, EnginioTests::TESTAPP_ENV);
    _backendId = apiKeys["backendId"].toString().toUtf8();

    QVERIFY(!_backendId.isEmpty());

    EnginioTests::prepareTestUsersAndUserGroups(_backendId);
    EnginioTests::prepareTestObjectType(_backendName);

    {
        QJsonObject obj;
        obj["objectType"] = QString::fromUtf8("objects.") + EnginioTests::CUSTOM_OBJECT1;
        obj["testCase"] = QString::fromUtf8("IdentityCommonTest");
        obj["title"] = QString::fromUtf8("title");

        EnginioClient client;
        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);

        EnginioReply *reply = client.create(obj);
        QTRY_VERIFY(reply->isFinished());
        CHECK_NO_ERROR(reply);
    }
}

template<class Derived, class Identity>
void IdentityCommonTest<Derived, Identity>::cleanupTestCase()
{
    QVERIFY(_backendManager.removeBackend(_backendName));
}

template<class Derived, class Identity>
void IdentityCommonTest<Derived, Identity>::error(EnginioReply *reply)
{
    qDebug() << "\n\n### ERROR";
    qDebug() << reply->errorString();
    reply->dumpDebugInfo();
    qDebug() << "\n###\n";
}

template<class Derived, class Identity>
void IdentityCommonTest<Derived, Identity>::identity()
{
    {
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), This(), SLOT(error(EnginioReply *)));
        Identity identity;
        QSignalSpy spy(&client, SIGNAL(sessionAuthenticated(EnginioReply*)));
        QSignalSpy spyAuthError(&client, SIGNAL(sessionAuthenticationError(EnginioReply*)));
        QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

        identity.setUser("logintest");
        identity.setPassword("logintest");


        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        client.setIdentity(&identity);

        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spyError.count(), 0);
        QCOMPARE(spyAuthError.count(), 0);
        QCOMPARE(client.authenticationState(), Enginio::Authenticated);

        QJsonObject token = Derived::enginioData(spy[0][0].value<EnginioReply*>()->data());
        QVERIFY(token.contains("user"));
        QVERIFY(token.contains("usergroups"));
    }
    {
        // Different initialization order
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), This(), SLOT(error(EnginioReply *)));
        Identity identity;
        QSignalSpy spy(&client, SIGNAL(sessionAuthenticated(EnginioReply*)));
        QSignalSpy spyAuthError(&client, SIGNAL(sessionAuthenticationError(EnginioReply*)));
        QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

        identity.setUser("logintest");
        identity.setPassword("logintest");

        client.setIdentity(&identity);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        client.setBackendId(_backendId);

        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spyError.count(), 0);
        QCOMPARE(spyAuthError.count(), 0);
        QCOMPARE(client.authenticationState(), Enginio::Authenticated);
    }
    {
        // login / logout
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), This(), SLOT(error(EnginioReply *)));
        Identity identity;
        QSignalSpy spyAuthError(&client, SIGNAL(sessionAuthenticationError(EnginioReply*)));
        QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

        identity.setUser("logintest");
        identity.setPassword("logintest");
        client.setIdentity(&identity);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        client.setBackendId(_backendId);

        QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);
        QCOMPARE(spyError.count(), 0);

        client.setIdentity(0);
        QTRY_COMPARE(client.authenticationState(), Enginio::NotAuthenticated);
        QCOMPARE(spyError.count(), 0);

        client.setIdentity(&identity);
        QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);
        QCOMPARE(spyError.count(), 0);
        QCOMPARE(spyAuthError.count(), 0);
    }
    {
        // change backend id
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), This(), SLOT(error(EnginioReply *)));
        Identity identity;
        QSignalSpy spyAuthError(&client, SIGNAL(sessionAuthenticationError(EnginioReply*)));
        QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

        identity.setUser("logintest");
        identity.setPassword("logintest");
        client.setIdentity(&identity);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        client.setBackendId(_backendId);

        QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);
        QCOMPARE(spyError.count(), 0);

        client.setBackendId(QByteArray());
        client.setBackendId(_backendId);
        QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);
        QCOMPARE(spyError.count(), 0);
        QCOMPARE(spyAuthError.count(), 0);
    }
    {
        // fast identity change before initialization
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), This(), SLOT(error(EnginioReply *)));
        client.setServiceUrl(EnginioTests::TESTAPP_URL);

        QSignalSpy spyAuthError(&client, SIGNAL(sessionAuthenticationError(EnginioReply*)));
        QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

        Identity identity1;
        Identity identity2;
        Identity identity3;

        identity1.setUser("logintest");
        identity1.setPassword("logintest");
        identity2.setUser("logintest2");
        identity2.setPassword("logintest2");
        identity3.setUser("logintest3");
        identity3.setPassword("logintest3");

        QCOMPARE(spyError.count(), 0);
        QCOMPARE(client.authenticationState(), Enginio::NotAuthenticated);

        for (uint i = 0; i < 4; ++i) {
            client.setIdentity(&identity1);
            client.setIdentity(&identity2);
            client.setIdentity(&identity3);
        }

        QCOMPARE(spyError.count(), 0);
        QCOMPARE(client.authenticationState(), Enginio::NotAuthenticated);

        client.setBackendId(_backendId); // trigger authentication process

        QCOMPARE(client.authenticationState(), Enginio::Authenticating);
        QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);

        QCOMPARE(spyError.count(), 0);
        QCOMPARE(spyAuthError.count(), 0);
    }
    {
        // check if EnginoClient is properly detached from identity in destructor.
        EnginioClient *client = new EnginioClient;
        client->setServiceUrl(EnginioTests::TESTAPP_URL);
        client->setBackendId(_backendId);

        Identity identity;
        client->setIdentity(&identity);

        delete client;

        identity.setPassword("blah");
        identity.setUser("blah");
        // we should not crash
    }
    {
        // check if EnginoClient is properly detached from identity destructor.
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), This(), SLOT(error(EnginioReply *)));
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        client.setBackendId(_backendId);
        {
            Identity identity;
            client.setIdentity(&identity);
        }
        QVERIFY(!client.identity());
    }
}

template<class Derived, class Identity>
void IdentityCommonTest<Derived, Identity>::identity_changing()
{
    {   // check if login is triggered on passowrd change
        EnginioClient client;
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        client.setBackendId(_backendId);

        Identity identity;
        identity.setUser("logintest");
        client.setIdentity(&identity);
        QTRY_COMPARE(client.authenticationState(), Enginio::AuthenticationFailure);

        identity.setPassword("logintest");
        QCOMPARE(client.authenticationState(), Enginio::Authenticating);

        QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);
    }
    {   // check if login is triggered on user change
        EnginioClient client;
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        client.setBackendId(_backendId);

        Identity identity;
        identity.setUser("logintest");
        identity.setPassword("logintest");
        client.setIdentity(&identity);

        QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);

        identity.setUser("invalid");
        QCOMPARE(client.authenticationState(), Enginio::Authenticating);
        QTRY_COMPARE(client.authenticationState(), Enginio::AuthenticationFailure);

        identity.setPassword("invalid");
        QCOMPARE(client.authenticationState(), Enginio::Authenticating);
        QTRY_COMPARE(client.authenticationState(), Enginio::AuthenticationFailure);
    }
    {   // check races when identity flickers fast.
        EnginioClient client;
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        client.setBackendId(_backendId);

        QSignalSpy spy(&client, SIGNAL(sessionAuthenticated(EnginioReply*)));
        Identity identity;
        client.setIdentity(&identity);
        QTRY_COMPARE(client.authenticationState(), Enginio::AuthenticationFailure);

        for (int i = 0; i < 8; ++i) {
            identity.setUser("logintest1");
            identity.setPassword("logintest1");
            identity.setUser("logintest2");
            identity.setPassword("logintest2");
        }
        identity.setUser("logintest");
        identity.setPassword("logintest");
        QCOMPARE(client.authenticationState(), Enginio::Authenticating);

        QTRY_COMPARE_WITH_TIMEOUT(client.authenticationState(), Enginio::Authenticated, 20000);
        QJsonObject data = Derived::enginioData(spy.last().last().value<EnginioReply*>()->data());
        QCOMPARE(data["user"].toObject()["username"].toString(), QString::fromLatin1("logintest"));
    }
}

template<class Derived, class Identity>
void IdentityCommonTest<Derived, Identity>::identity_invalid()
{
    {
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), This(), SLOT(error(EnginioReply *)));
        Identity identity;
        QSignalSpy spy(&client, SIGNAL(sessionAuthenticated(EnginioReply*)));
        QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));
        QSignalSpy spyAuthError(&client, SIGNAL(sessionAuthenticationError(EnginioReply*)));

        identity.setUser("invalidLogin");
        identity.setPassword("invalidPassword");

        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        client.setIdentity(&identity);

        QTRY_COMPARE(spyAuthError.count(), 1);
        QTRY_COMPARE(spy.count(), 0);
        QCOMPARE(spyError.count(), 0);
        QCOMPARE(client.authenticationState(), Enginio::AuthenticationFailure);
    }
    {   // check if an old session is _not_ invalidated on an invalid re-login
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), This(), SLOT(error(EnginioReply *)));
        Identity identity;
        QSignalSpy spy(&client, SIGNAL(sessionAuthenticated(EnginioReply*)));
        QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));
        QSignalSpy spyAuthError(&client, SIGNAL(sessionAuthenticationError(EnginioReply*)));

        identity.setUser("logintest");
        identity.setPassword("logintest");

        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        client.setIdentity(&identity);

        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spyError.count(), 0);
        QTRY_COMPARE(spyAuthError.count(), 0);
        QCOMPARE(client.authenticationState(), Enginio::Authenticated);

        const QJsonObject identityReplyData = spy[0][0].value<EnginioReply*>()->data();

        // we are logged-in
        identity.setUser("invalidLogin");
        QTRY_COMPARE(spyAuthError.count(), 1);
        identity.setPassword("invalidPass");
        QTRY_COMPARE(spyAuthError.count(), 2);

        // get back to logged-in state
        identity.setUser("logintest2");
        QTRY_COMPARE(spyAuthError.count(), 3);
        QCOMPARE(spy.count(), 1);
        identity.setPassword("logintest2");
        QTRY_COMPARE(spy.count(), 2);
        QTRY_COMPARE(spyAuthError.count(), 3);

        QVERIFY(spy[1][0].value<EnginioReply*>()->data() != identityReplyData);
        QCOMPARE(client.authenticationState(), Enginio::Authenticated);
    }
    {
        // wrong backend id and invalid password
        EnginioClient client;
        client.setBackendId("deadbeef");
        Identity identity;
        identity.setUser("invalidLogin");
        identity.setPassword("invalidPassword");
        client.setIdentity(&identity);
        QCOMPARE(client.authenticationState(), Enginio::Authenticating);
        QTRY_COMPARE(client.authenticationState(), Enginio::AuthenticationFailure);
    }
    {
        // missing empty user name and pass
        EnginioClient client;
        client.setBackendId(_backendId);
        Identity identity;
        client.setIdentity(&identity);
        QCOMPARE(client.authenticationState(), Enginio::Authenticating);
        QTRY_COMPARE(client.authenticationState(), Enginio::AuthenticationFailure);
    }
}

template<class Derived, class Identity>
void IdentityCommonTest<Derived, Identity>::identity_afterLogout(const QByteArray &headerName)
{
    qRegisterMetaType<QNetworkReply*>();
    EnginioClient client;

    Identity identity;
    identity.setUser("logintest");
    identity.setPassword("logintest");

    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);
    client.setIdentity(&identity);

    QVERIFY(client.networkManager());
    QTRY_COMPARE(client.authenticationState(), Enginio::Authenticated);

    // This may be fragile, we need real network reply to catch the header.
    QSignalSpy spy(client.networkManager(), SIGNAL(finished(QNetworkReply*)));

    // make a connection with a new session token
    QJsonObject obj;
    obj["objectType"] = QString::fromUtf8("objects.todos");
    const EnginioReply* reqId = client.query(obj);
    QVERIFY(reqId);
    QTRY_VERIFY(!reqId->data().isEmpty());
    CHECK_NO_ERROR(reqId);
    QCOMPARE(spy.count(), 1);

    QVERIFY(spy[0][0].value<QNetworkReply*>()->request().hasRawHeader(headerName));

    client.setIdentity(0);
    QTRY_COMPARE(client.authenticationState(), Enginio::NotAuthenticated);

    // unsecured data should be still accessible
    reqId = client.query(obj);
    QVERIFY(reqId);
    QTRY_VERIFY(!reqId->data().isEmpty());
    CHECK_NO_ERROR(reqId);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!spy[1][0].value<QNetworkReply*>()->request().hasRawHeader(headerName));
}

struct QueryRestrictedObject_objectId
{
    QString &objectId;
    void operator ()(EnginioReply *reply)
    {
        CHECK_NO_ERROR(reply);

        QJsonArray results = reply->data()["results"].toArray();
        QVERIFY(results.count());
        objectId = results[0].toObject()["id"].toString();
        QVERIFY(!objectId.isEmpty());
    }
};

template<typename T>
struct QueryRestrictedObject_userId
{
    QString &userId;
    void operator ()(EnginioReply *reply)
    {
        userId = T::enginioData(reply->data())["user"].toObject()["id"].toString();
        QVERIFY(!userId.isEmpty());
    }
};

template<class Derived, class Identity>
void IdentityCommonTest<Derived, Identity>::queryRestrictedObject()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString userName = "logintest";
    QString userPass = "logintest";
    QString userId;

    QueryRestrictedObject_userId<Derived> userIdsetter = { userId };
    QObject::connect(&client, &EnginioClient::sessionAuthenticated, userIdsetter);

    Identity identity;
    identity.setUser(userName);
    identity.setPassword(userPass);
    client.setIdentity(&identity);
    QCOMPARE(client.authenticationState(), Enginio::Authenticating);

    QString objectType = QString::fromUtf8("objects.").append(EnginioTests::CUSTOM_OBJECT1);

    // query an object
    QString objectId; // restricted object id
    {
        QJsonObject obj;
        obj["objectType"] = objectType;
        EnginioReply *reply = client.query(obj);
        QueryRestrictedObject_objectId setter = { objectId };
        QObject::connect(reply, &EnginioReply::finished, setter);
    }

    QTRY_VERIFY(userId.length() && objectId.length());
    QCOMPARE(client.authenticationState(), Enginio::Authenticated);

    // TODO enable it
//    // apply acl to the object allowing only user to read
//    {
//        QJsonObject aclUpdate;
//        aclUpdate["objectType"] = objectType;
//        aclUpdate["id"] = objectId;
//        QString json = "{ \"read\": [ { \"id\": \"%1\", \"objectType\": \"users\" } ],"
//                         "\"update\": [ { \"id\": \"%1\", \"objectType\": \"users\" } ],"
//                         "\"admin\": [ { \"id\": \"%1\", \"objectType\": \"users\" } ] }";
//        json = json.arg(userId);
//        aclUpdate["access"] = QJsonDocument::fromJson(json.toUtf8()).object();
//        qDebug() << aclUpdate;
//        EnginioReply *reply = client.update(aclUpdate, Enginio::AccessControlOperation);
//        QTRY_VERIFY(reply->isFinished());
//        qDebug() << reply;
//        CHECK_NO_ERROR(reply);
//    }

//    // query the restricted object
//    {
//        QJsonObject obj;
//        obj["objectType"] = objectType;
//        QJsonObject query;
//        query["id"] = objectId;
//        obj["query"] = query;

//        EnginioReply *reply = client.query(obj);
//        QTRY_VERIFY(reply->isFinished());
//        CHECK_NO_ERROR(reply);

//        QJsonArray results = reply->data()["results"].toArray();
//        QCOMPARE(results.count(), 1);
//    }

//    // logout
//    client.setIdentity(0);
//    QTRY_COMPARE(client.authenticationState(), Enginio::NotAuthenticated);

//    // query the restricted object
//    {
//        QJsonObject obj;
//        obj["objectType"] = objectType;
//        QJsonObject query;
//        query["id"] = objectId;
//        obj["query"] = query;

//        EnginioReply *reply = client.query(obj);
//        QTRY_VERIFY(reply->isFinished());
//        CHECK_NO_ERROR(reply);

//        QJsonArray results = reply->data()["results"].toArray();
//        QCOMPARE(results.count(), 0); // we do not have rights to access the object
//    }

}

template<class Derived, class Identity>
void IdentityCommonTest<Derived, Identity>::userAndPass()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString userName = "logintest";
    QString userPass = "logintest";

    Identity identity;
    QSignalSpy userSpy(&identity, SIGNAL(userChanged(QString)));
    QSignalSpy passSpy(&identity, SIGNAL(passwordChanged(QString)));

    QCOMPARE(identity.user(), QString());
    QCOMPARE(identity.password(), QString());

    identity.setUser(userName);
    QCOMPARE(userSpy.count(), 1);
    QCOMPARE(passSpy.count(), 0);
    identity.setPassword(userPass);
    QCOMPARE(passSpy.count(), 1);

    QCOMPARE(identity.user(), userName);
    QCOMPARE(identity.password(), userPass);

    // no real change
    identity.setUser(userName);
    QCOMPARE(userSpy.count(), 1);
    identity.setPassword(userPass);
    QCOMPARE(passSpy.count(), 1);

    QCOMPARE(identity.user(), userName);
    QCOMPARE(identity.password(), userPass);

    identity.setUser(QString());
    QCOMPARE(userSpy.count(), 2);
    QCOMPARE(passSpy.count(), 1);
    identity.setPassword(QString());
    QCOMPARE(passSpy.count(), 2);

    QCOMPARE(identity.user(), QString());
    QCOMPARE(identity.password(), QString());
}
