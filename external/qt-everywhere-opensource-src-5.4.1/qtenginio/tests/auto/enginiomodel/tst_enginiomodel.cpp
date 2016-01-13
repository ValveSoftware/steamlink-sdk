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

#include <QtWidgets/qlistview.h>
#include <QtWidgets/qapplication.h>

#include <Enginio/enginioclient.h>
#include <Enginio/enginioreply.h>
#include <Enginio/enginiomodel.h>
#include <Enginio/enginioidentity.h>
#include <Enginio/enginiooauth2authentication.h>

#include "../common/common.h"

class tst_EnginioModel: public QObject
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
    void ctor();
    void deleteReply();
    void enginio_property();
    void query_property();
    void operation_property();
    void roleNames();
    void listView();
    void invalidRemove();
    void invalidSetProperty();
    void multpleConnections();
    void deletionReordered();
    void deleteTwiceTheSame();
    void updateAndDeleteReordered();
    void updateReordered();
    void append();
    void removeExternallyRemovedObject();
    void setPropertyOnExternallyRemovedObject();
    void createAndModify();
    void externalNotification();
    void createUpdateRemoveWithNotification();
    void appendBeforeInitialModelReset();
    void delayedRequestBeforeInitialModelReset();
    void appendAndChangeQueryBeforeItIsFinished();
    void deleteModelDurringRequests();
    void updatingRoles();
    void setData();
    void setJsonData();
    void data();
    void setInvalidJsonData();
    void reload();
    void identityChange();
private:
    template<class T>
    void externallyRemovedImpl();
};

void tst_EnginioModel::initTestCase()
{
    if (EnginioTests::TESTAPP_URL.isEmpty())
        QFAIL("Needed environment variable ENGINIO_API_URL is not set!");

    _backendName = QStringLiteral("EnginioModel") + QString::number(QDateTime::currentMSecsSinceEpoch());
    QVERIFY(_backendManager.createBackend(_backendName));

    QJsonObject apiKeys = _backendManager.backendApiKeys(_backendName, EnginioTests::TESTAPP_ENV);
    _backendId = apiKeys["backendId"].toString().toUtf8();

    QVERIFY(!_backendId.isEmpty());

    // The test operates on user data.
    EnginioTests::prepareTestUsersAndUserGroups(_backendId);
    EnginioTests::prepareTestObjectType(_backendName);

    // Object types for the reload test
    QJsonObject reload1;
    reload1["name"] = QStringLiteral("reload1");
    QJsonObject title;
    title["name"] = QStringLiteral("title");
    title["type"] = QStringLiteral("string");
    title["indexed"] = false;
    QJsonArray properties;
    properties.append(title);
    reload1["properties"] = properties;
    QVERIFY(_backendManager.createObjectType(_backendName, EnginioTests::TESTAPP_ENV, reload1));

    QJsonObject reload2;
    reload2["name"] = QStringLiteral("reload2");
    reload2["properties"] = properties;
    QVERIFY(_backendManager.createObjectType(_backendName, EnginioTests::TESTAPP_ENV, reload2));
}

void tst_EnginioModel::cleanupTestCase()
{
    QVERIFY(_backendManager.removeBackend(_backendName));
}

void tst_EnginioModel::ctor()
{
    {
        EnginioModel model1, model2(this);
        Q_UNUSED(model1);
        Q_UNUSED(model2);
    }

    QJsonObject query = QJsonDocument::fromJson("{\"foo\":\"invalid\"}").object();
    QVERIFY(!query.isEmpty());
    {   // check if destructor of a fully initilized EnginioClient detach fully initilized model
        EnginioModel model;
        model.setOperation(Enginio::ObjectOperation);
        model.setQuery(query);

        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        model.setClient(&client);
    }
    {   // check if destructor of a fully initilized EnginioClient detach fully initilized model
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
        EnginioModel model;
        model.setOperation(Enginio::ObjectOperation);
        model.setQuery(query);
        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);
        model.setClient(&client);
    }
}

void tst_EnginioModel::enginio_property()
{
    {
        EnginioClient client;
        QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
        EnginioModel model;
        // No initial value
        QCOMPARE(model.client(), static_cast<EnginioClient*>(0));

        QSignalSpy spy(&model, SIGNAL(clientChanged(EnginioClient*)));
        model.setClient(&client);
        QCOMPARE(model.client(), &client);
        QTRY_COMPARE(spy.count(), 1);
    }
    {
        EnginioModel model;
        QSignalSpy spy(&model, SIGNAL(clientChanged(EnginioClient*)));
        {
            EnginioClient client;
            QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
            model.setClient(&client);
            QCOMPARE(model.client(), &client);
            QTRY_COMPARE(spy.count(), 1);
            QCOMPARE(spy[0][0].value<EnginioClient*>(), &client);
        }
        QCOMPARE(model.client(), static_cast<EnginioClient*>(0));
        QTRY_COMPARE(spy.count(), 2);
        QCOMPARE(spy[1][0].value<EnginioClient*>(), static_cast<EnginioClient*>(0));
    }
    {
        EnginioModel model;
        QTest::ignoreMessage(QtWarningMsg, "EnginioModel::append(): Enginio client is not set");
        QVERIFY(!model.append(QJsonObject()));
        QTest::ignoreMessage(QtWarningMsg, "EnginioModel::remove(): Enginio client is not set");
        QVERIFY(!model.remove(0));
        QTest::ignoreMessage(QtWarningMsg, "EnginioModel::setData(): Enginio client is not set");
        QVERIFY(!model.setData(0, QVariant(), "blah"));
    }
    {
        // check if initial set is not calling reset twice
        EnginioClient client;
        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);

        EnginioModel model;
        QSignalSpy spyAboutToReset(&model, SIGNAL(modelAboutToBeReset()));
        QSignalSpy spyReset(&model, SIGNAL(modelReset()));
        QJsonObject query;
        query.insert("limit", 1);
        model.setQuery(query);
        model.setOperation(Enginio::UserOperation);
        model.setClient(&client);

        QTRY_COMPARE(spyAboutToReset.count(), 1);
        QTRY_COMPARE(spyReset.count(), 1);
    }
    {
        // check if initial set is not calling reset twice
        EnginioClient client;
        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);

        EnginioModel model;
        QSignalSpy spyAboutToReset(&model, SIGNAL(modelAboutToBeReset()));
        QSignalSpy spyReset(&model, SIGNAL(modelReset()));
        model.setClient(&client);
        QJsonObject query;
        query.insert("limit", 1);
        query.insert("objectTypes", "objects." + EnginioTests::CUSTOM_OBJECT1);
        model.setQuery(query);

        QTRY_COMPARE(spyAboutToReset.count(), 1);
        QTRY_COMPARE(spyReset.count(), 1);
    }
    {
        // check if assigning the same enginio is not emitting the changed signal
        EnginioClient client;
        EnginioModel model;

        QSignalSpy clientSpy(&model, SIGNAL(clientChanged(EnginioClient*)));
        QCOMPARE(model.client(), (EnginioClient *)0);

        model.setClient(&client);
        QCOMPARE(clientSpy.count(), 1);
        QCOMPARE(clientSpy[0][0].value<EnginioClient*>(), &client);
        QCOMPARE(model.client(), &client);

        model.setClient(&client);
        QCOMPARE(clientSpy.count(), 1);
    }
}

void tst_EnginioModel::query_property()
{
    QJsonObject query = QJsonDocument::fromJson("{\"foo\":\"invalid\"}").object();
    QVERIFY(!query.isEmpty());
    EnginioModel model;
    QSignalSpy spy(&model, SIGNAL(queryChanged(QJsonObject)));

    // initial value is empty
    QVERIFY(model.query().isEmpty());

    model.setQuery(query);
    QCOMPARE(model.query(), query);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].value<QJsonObject>(), query);

    // try to set the same query again, it should not emit the signal
    model.setQuery(query);
    QCOMPARE(model.query(), query);
    QCOMPARE(spy.count(), 1);

    model.setQuery(QJsonObject());
    QTRY_COMPARE(spy.count(), 2);
    QVERIFY(model.query().isEmpty());
    QVERIFY(spy[1][0].value<QJsonObject>().isEmpty());

    {
        model.setOperation(Enginio::UserOperation);
        QJsonObject query = QJsonDocument::fromJson("{\"limit\":5}").object();

        EnginioClient client;
        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);

        model.setClient(&client);
        model.setQuery(query);
        QTRY_VERIFY(model.rowCount() > 0);

        model.setQuery(QJsonObject());
        QCOMPARE(model.rowCount(), 0);
    }
}

void tst_EnginioModel::operation_property()
{
    EnginioModel model;
    QSignalSpy spy(&model, SIGNAL(operationChanged(Enginio::Operation)));

    // check initial value
    QCOMPARE(model.operation(), Enginio::ObjectOperation);

    model.setOperation(Enginio::UserOperation);
    QCOMPARE(model.operation(), Enginio::UserOperation);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].value<Enginio::Operation>(), Enginio::UserOperation);

    // try to set the same operation again, it should not emit the signal
    model.setOperation(Enginio::UserOperation);
    QCOMPARE(model.operation(), Enginio::UserOperation);
    QCOMPARE(spy.count(), 1);

    // try to change it agian.
    model.setOperation(Enginio::UsergroupOperation);
    QTRY_COMPARE(spy.count(), 2);
    QCOMPARE(model.operation(), Enginio::UsergroupOperation);
    QCOMPARE(spy[1][0].value<Enginio::Operation>(), Enginio::UsergroupOperation);
}

void tst_EnginioModel::roleNames()
{
    struct EnginioModelChild: public EnginioModel
    {
        using EnginioModel::roleNames;
    } model;
    QVERIFY(model.roleNames().isEmpty()); // Initilial value

    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);
    model.setClient(&client);

    QVERIFY(model.client() == &client);

    model.setOperation(Enginio::UserOperation);
    QJsonObject query = QJsonDocument::fromJson("{\"limit\":5}").object();
    model.setQuery(query);

    QTRY_COMPARE(model.rowCount(), 5);
    QHash<int, QByteArray> roles = model.roleNames();
    QSet<QByteArray> expectedRoles;
    expectedRoles << "updatedAt" << "objectType" << "id" << "username" << "createdAt" << "_synced";
    QSet<QByteArray> roleNames = roles.values().toSet();
    foreach(const QByteArray role, expectedRoles)
        QVERIFY(roleNames.contains(role));
}

void tst_EnginioModel::listView()
{
    QJsonObject query = QJsonDocument::fromJson("{\"limit\":2}").object();
    QVERIFY(!query.isEmpty());
    EnginioModel model;
    model.setQuery(query);
    model.setOperation(Enginio::UserOperation);

    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);
    model.setClient(&client);

    QListView view;
    view.setModel(&model);
    view.show();

    QTRY_COMPARE(model.rowCount(), 2);
    QVERIFY(view.model() == &model);
    QVERIFY(view.indexAt(QPoint(1,1)).data().isValid());
}

struct ReplyCounter
{
    int &counter;
    ReplyCounter(int *storage)
        : counter(*storage)
    {}

    void operator ()(EnginioReply *)
    {
        ++counter;
    }
};

struct InvalidRemoveErrorChecker: public ReplyCounter
{
    InvalidRemoveErrorChecker(int *storage)
        : ReplyCounter(storage)
    {}

    void operator ()(EnginioReply *reply)
    {
        QVERIFY(reply->isFinished());
        QVERIFY(reply->isError());
        QCOMPARE(reply->errorType(), Enginio::BackendError);
        QVERIFY(reply->networkError() != QNetworkReply::NoError);\
        QCOMPARE(reply->backendStatus(), 400);

        QJsonObject data = reply->data();
        QVERIFY(!data.isEmpty());

        QVERIFY(!data["errors"].toArray()[0].toObject()["message"].toString().isEmpty());
        QVERIFY(!data["errors"].toArray()[0].toObject()["reason"].toString().isEmpty());

        ReplyCounter::operator ()(reply);
    }
};
void tst_EnginioModel::invalidRemove()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    EnginioModel model;
    model.setClient(&client);

    int counter = 0;
    InvalidRemoveErrorChecker replyCounter(&counter);
    EnginioReply *reply;

    reply = model.remove(0);
    QVERIFY(reply);
    QVERIFY(reply->parent());
    QObject::connect(reply, &EnginioReply::finished, replyCounter);

    reply = model.remove(-10);
    QVERIFY(reply);
    QVERIFY(reply->parent());
    QObject::connect(reply, &EnginioReply::finished, replyCounter);

    reply = model.remove(-1);
    QVERIFY(reply);
    QVERIFY(reply->parent());
    QObject::connect(reply, &EnginioReply::finished, replyCounter);

    reply = model.remove(model.rowCount());
    QVERIFY(reply);
    QVERIFY(reply->parent());
    QObject::connect(reply, &EnginioReply::finished, replyCounter);

    QTRY_COMPARE(counter, 4);
}

void tst_EnginioModel::invalidSetProperty()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    EnginioModel model;
    model.setClient(&client);
    QJsonObject query = QJsonDocument::fromJson("{\"limit\":2}").object();
    model.setOperation(Enginio::UserOperation);
    model.setQuery(query);

    QTRY_VERIFY(model.rowCount());

    int counter = 0;
    ReplyCounter replyCounter(&counter);
    EnginioReply *reply;

    reply = model.setData(-10, QVariant(123), "Blah");
    QVERIFY(reply);
    QVERIFY(reply->parent());
    QObject::connect(reply, &EnginioReply::finished, replyCounter);

    reply = model.setData(-1, QVariant(123), "Blah");
    QVERIFY(reply);
    QVERIFY(reply->parent());
    QObject::connect(reply, &EnginioReply::finished, replyCounter);

    reply = model.setData(model.rowCount(), QVariant(123), "Blah");
    QVERIFY(reply);
    QVERIFY(reply->parent());
    QObject::connect(reply, &EnginioReply::finished, replyCounter);

    reply = model.setData(0, QVariant(123), "Blah"); // invalid Role
    QVERIFY(reply);
    QVERIFY(reply->parent());
    QObject::connect(reply, &EnginioReply::finished, replyCounter);

    QTRY_COMPARE(counter, 4);
}

struct EnginioClientConnectionSpy: public EnginioClient
{
    virtual void connectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE
    {
        counter[signal.name()] += 1;
    }
    virtual void disconnectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE
    {
        counter[signal.name()] -= 1;
    }

    QHash<QByteArray, int> counter;
};

struct EnginioModelConnectionSpy: public EnginioModel
{
    virtual void connectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE
    {
        counter[signal.name()] += 1;
    }
    virtual void disconnectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE
    {
        counter[signal.name()] -= 1;
    }

    QHash<QByteArray, int> counter;
};

void tst_EnginioModel::multpleConnections()
{
    {
        EnginioClientConnectionSpy client1;
        client1.setBackendId(_backendId);
        client1.setServiceUrl(EnginioTests::TESTAPP_URL);
        EnginioClientConnectionSpy client2;
        client2.setBackendId(_backendId);
        client2.setServiceUrl(EnginioTests::TESTAPP_URL);

        EnginioModelConnectionSpy model;
        for (int i = 0; i < 20; ++i) {
            model.setClient(&client1);
            model.setClient(&client2);
            model.setClient(0);
        }

        // The values here are not strict. Use qDebug() << model.counter; to see what
        // makes sense. Just to be sure 2097150 or 20 doesn't.
        QCOMPARE(model.counter["operationChanged"], 0);
        QCOMPARE(model.counter["queryChanged"], 0);
        QCOMPARE(model.counter["clientChanged"], 0);
        QCOMPARE(client1.counter["finished"], 0);
        QCOMPARE(client2.counter["finished"], 0);

        // All of them are acctually disconnected but disconnectNotify is not called, it is
        // a known bug in Qt.
        QCOMPARE(client1.counter["backendIdChanged"], 20);
        QCOMPARE(client1.counter["destroyed"], 20);
        QCOMPARE(client2.counter["backendIdChanged"], 20);
        QCOMPARE(client2.counter["destroyed"], 20);
    }
    {
        EnginioClientConnectionSpy client;
        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);

        EnginioModelConnectionSpy model;
        model.setClient(&client);
        QJsonObject query1, query2;
        query1.insert("objectType", QString("objects.todos"));
        query2.insert("objectType", QString("objects.blah"));
        for (int i = 0; i < 20; ++i) {
            model.setQuery(query1);
            model.setQuery(query2);
            model.setQuery(QJsonObject());
        }

        // The values here are not strict. Use qDebug() << model.counter; to see what
        // makes sense. Just to be sure 2097150 or 20 doesn't.
        QCOMPARE(model.counter["operationChanged"], 0);
        QCOMPARE(model.counter["queryChanged"], 0);
        QCOMPARE(model.counter["clientChanged"], 0);
    }
    {
        EnginioClientConnectionSpy client;
        client.setBackendId(_backendId);
        client.setServiceUrl(EnginioTests::TESTAPP_URL);

        EnginioModelConnectionSpy model;
        model.setClient(&client);
        QJsonObject query;
        query.insert("objectType", QString("objects.todos"));
        model.setQuery(query);

        for (int i = 0; i < 20; ++i) {
            model.setOperation(Enginio::ObjectOperation);
            model.setOperation(Enginio::AccessControlOperation);
        }

        // The values here are not strict. Use qDebug() << model.counter; to see what
        // makes sense. Just to be sure 2097150 or 20 doesn't.
        QCOMPARE(model.counter["operationChanged"], 0);
        QCOMPARE(model.counter["queryChanged"], 0);
        QCOMPARE(model.counter["clientChanged"], 0);
    }
}

struct StopDelayingFunctor
{
    EnginioReply *reply;
    StopDelayingFunctor(EnginioReply *r)
        : reply(r)
    {}
    void operator()()
    {
        reply->setDelayFinishedSignal(false);
    }
};

void tst_EnginioModel::deletionReordered()
{
    QJsonObject query = QJsonDocument::fromJson("{\"limit\":2}").object();
    QVERIFY(!query.isEmpty());
    EnginioModel model;
    model.disableNotifications();
    model.setQuery(query);
    model.setOperation(Enginio::UserOperation);

    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);
    model.setClient(&client);

    QTRY_COMPARE(model.rowCount(), int(query["limit"].toDouble()));
    QVERIFY(model.rowCount() >= 2);

    EnginioReply *r2 = model.remove(model.rowCount() - 1);
    EnginioReply *r1 = model.remove(0);

    QVERIFY(!r1->isFinished());
    QVERIFY(!r2->isFinished());
    QVERIFY(!r1->isError());
    QVERIFY(!r2->isError());

    r2->setDelayFinishedSignal(true);
    StopDelayingFunctor activateR2(r2);
    QObject::connect(r1, &EnginioReply::finished, activateR2);

    int counter = 0;
    ReplyCounter replyCounter(&counter);
    QObject::connect(r1, &EnginioReply::finished, replyCounter);
    QObject::connect(r2, &EnginioReply::finished, replyCounter);

    QTRY_COMPARE(counter, 2);
}

void tst_EnginioModel::deleteTwiceTheSame()
{
    QJsonObject query = QJsonDocument::fromJson("{\"limit\":1}").object();
    QVERIFY(!query.isEmpty());
    EnginioModel model;
    model.disableNotifications();
    model.setQuery(query);
    model.setOperation(Enginio::UserOperation);

    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);
    model.setClient(&client);

    QTRY_COMPARE(model.rowCount(), int(query["limit"].toDouble()));

    EnginioReply *r2 = model.remove(model.rowCount() - 1);
    EnginioReply *r1 = model.remove(model.rowCount() - 1);

    QVERIFY(!r1->isFinished());
    QVERIFY(!r2->isFinished());
    QVERIFY(!r1->isError());
    QVERIFY(!r2->isError());

    int counter = 0;
    ReplyCounter replyCounter(&counter);
    QObject::connect(r1, &EnginioReply::finished, replyCounter);
    QObject::connect(r2, &EnginioReply::finished, replyCounter);

    QTRY_COMPARE(counter, 2);

    // Sometimes server send us two positive message about deletion and
    // sometimes one positive and one 404.
    QVERIFY((r1->backendStatus() == 404 && !r2->isError())
            || (r2->backendStatus() == 404 && !r1->isError())
            || (!r1->isError() && !r2->isError()));
    // Check if the model is in sync with the backend, only one item should
    // be removed.
    QTRY_COMPARE(model.rowCount(), int(query["limit"].toDouble()) - 1);
}


void tst_EnginioModel::updateAndDeleteReordered()
{
    QJsonObject query = QJsonDocument::fromJson("{\"limit\":1}").object();
    QVERIFY(!query.isEmpty());
    EnginioModel model;
    model.disableNotifications();
    model.setQuery(query);
    model.setOperation(Enginio::UserOperation);

    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);
    model.setClient(&client);

    QTRY_COMPARE(model.rowCount(), int(query["limit"].toDouble()));

    EnginioReply *r2 = model.setData(model.rowCount() - 1, "email@email.com", "email");
    EnginioReply *r1 = model.remove(model.rowCount() - 1);

    QVERIFY(!r1->isFinished());
    QVERIFY(!r2->isFinished());
    QVERIFY(!r1->isError());
    QVERIFY(!r2->isError());

    int counter = 0;
    ReplyCounter replyCounter(&counter);
    QObject::connect(r1, &EnginioReply::finished, replyCounter);
    QObject::connect(r2, &EnginioReply::finished, replyCounter);

    QTRY_COMPARE(counter, 2);

    // Sometimes server send us two positive message and
    // sometimes one positive and one 404.
    QVERIFY((r1->backendStatus() == 404 && !r2->isError())
            || (r2->backendStatus() == 404 && !r1->isError())
            || (!r1->isError() && !r2->isError()));
    // Check if the model is in sync with the backend, one item should
    // be removed.
    QTRY_COMPARE(model.rowCount(), int(query["limit"].toDouble()) - 1);
}

void tst_EnginioModel::updateReordered()
{
    QJsonObject query = QJsonDocument::fromJson("{\"limit\":1}").object();
    QVERIFY(!query.isEmpty());
    EnginioModel model;
    model.disableNotifications();
    model.setQuery(query);
    model.setOperation(Enginio::UserOperation);

    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);
    model.setClient(&client);

    QTRY_COMPARE(model.rowCount(), int(query["limit"].toDouble()));

    int counter = 0;

    EnginioReply *r2 = model.setData(0, "email2@email.com", "email");
    QVERIFY(!r2->isError());
    r2->setDelayFinishedSignal(true);

    ReplyCounter replyCounter(&counter);
    QObject::connect(r2, &EnginioReply::finished, replyCounter);

    QTRY_VERIFY(!r2->data().isEmpty()); // at this point r2 is done but finished signal is not emited
    QTRY_COMPARE(counter, 0);

    EnginioReply *r1 = model.setData(0, "email1@email.com", "email");
    QVERIFY(!r1->isError());

    QObject::connect(r1, &EnginioReply::finished, replyCounter);
    StopDelayingFunctor activateR2(r2);
    QObject::connect(r1, &EnginioReply::finished, activateR2);

    QTRY_COMPARE(counter, 2);

    QDateTime r1UpdatedAt = QDateTime::fromString(r1->data()["updatedAt"].toString(), Qt::ISODate);
    QDateTime r2UpdatedAt = QDateTime::fromString(r2->data()["updatedAt"].toString(), Qt::ISODate);

    QVERIFY(r2UpdatedAt < r1UpdatedAt);
    QCOMPARE(model.data(model.index(0)).value<QJsonValue>().toObject(), r1->data());
}

void tst_EnginioModel::append()
{
    QString propertyName = "title";
    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType); // should be an empty set

    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    EnginioModel model;
    model.disableNotifications();
    model.setQuery(query);

    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));

    {   // init the model
        QSignalSpy spy(&model, SIGNAL(modelReset()));
        model.setClient(&client);

        QTRY_VERIFY(spy.count() > 0);
    }
    {
        // add two items to an empy model
        QCOMPARE(model.rowCount(), 0);
        QJsonObject o1, o2;
        o1.insert(propertyName, QString::fromLatin1("o1"));
        o1.insert("objectType", objectType);
        o2.insert(propertyName, QString::fromLatin1("o2"));
        o2.insert("objectType", objectType);

        EnginioReply *r1 = model.append(o1);
        EnginioReply *r2 = model.append(o2);
        QVERIFY(r1);
        QVERIFY(r2);

        QCOMPARE(model.rowCount(), 2);

        // check data content
        QCOMPARE(model.data(model.index(0)).value<QJsonValue>().toObject(), o1);
        QCOMPARE(model.data(model.index(1)).value<QJsonValue>().toObject(), o2);

        // check synced flag, the data is not synced yet
        QCOMPARE(model.data(model.index(0), Enginio::SyncedRole).value<bool>(), false);
        QCOMPARE(model.data(model.index(1), Enginio::SyncedRole).value<bool>(), false);

        // check synced flag, the data should be in sync at some point
        QTRY_COMPARE(model.data(model.index(0), Enginio::SyncedRole).value<bool>(), true);
        QTRY_COMPARE(model.data(model.index(1), Enginio::SyncedRole).value<bool>(), true);
    }
    {
        // add a new item and remove earlier the first item
        QVERIFY(model.rowCount() > 0);
        QJsonObject o3;
        o3.insert(propertyName, QString::fromLatin1("o3"));
        o3.insert("objectType", objectType);

        // check if everything is in sync
        for (int i = 0; i < model.rowCount(); ++i)
            QCOMPARE(model.data(model.index(i), Enginio::SyncedRole).value<bool>(), true);

        const int initialRowCount = model.rowCount();
        // the real test
        EnginioReply *r3 = model.append(o3);
        QVERIFY(r3);
        QVERIFY(!r3->isFinished());
        r3->setDelayFinishedSignal(true);
        QCOMPARE(model.data(model.index(initialRowCount), Enginio::SyncedRole).value<bool>(), false);

        EnginioReply *r4 = model.remove(0);

        QCOMPARE(model.data(model.index(0), Enginio::SyncedRole).value<bool>(), false);
        QCOMPARE(model.data(model.index(initialRowCount), Enginio::SyncedRole).value<bool>(), false);

        int counter = 0;
        ReplyCounter replyCounter(&counter);
        QObject::connect(r3, &EnginioReply::finished, replyCounter);
        QObject::connect(r4, &EnginioReply::finished, replyCounter);

        QTRY_COMPARE(counter, 1);
        QVERIFY(r4->isFinished());
        CHECK_NO_ERROR(r4);
        // at this point the first value was deleted but append is still not confirmed
        QCOMPARE(r3->delayFinishedSignal(), true);
        QCOMPARE(model.rowCount(), initialRowCount); // one added and one removed

        for (int i = 0; i < model.rowCount() - 1; ++i) {
            QCOMPARE(model.data(model.index(i), Enginio::SyncedRole).value<bool>(), true);
        }
        {
            QModelIndex idx = model.index(initialRowCount - 1);
            QCOMPARE(model.data(idx, Enginio::SyncedRole).value<bool>(), false);
            QCOMPARE(model.data(idx).value<QJsonValue>().toObject(), o3);
        }

        r3->setDelayFinishedSignal(false);
        QVERIFY(!client.finishDelayedReplies());
        QTRY_COMPARE(counter, 2);
        // everything should be done
        QCOMPARE(model.rowCount(), initialRowCount);
        QVERIFY(r4->isFinished());
        CHECK_NO_ERROR(r4);
        QVERIFY(r3->isFinished());
        CHECK_NO_ERROR(r3);

        // check if everything is in sync
        for (int i = 0; i < model.rowCount(); ++i)
            QCOMPARE(model.data(model.index(i), Enginio::SyncedRole).value<bool>(), true);

        QModelIndex idx = model.index(initialRowCount - 1);
        QCOMPARE(model.data(idx).value<QJsonValue>().toObject()[propertyName], o3[propertyName]);
    }
}

template<class T>
void tst_EnginioModel::externallyRemovedImpl()
{
    T operation;
    // Remove an item from model which was removed earlier from server
    QString propertyName = operation.propertyName();
    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);
    query.insert("limit", 1);

    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    {
        QJsonObject o;
        o.insert(propertyName, QString::fromLatin1("IWillBeRemoved"));
        o.insert("objectType", objectType);
        EnginioReply *r = client.create(o);
        QTRY_VERIFY(r->isFinished());
        CHECK_NO_ERROR(r);
    }

    EnginioModel model;
    model.disableNotifications();
    model.setQuery(query);

    {   // init the model
        QSignalSpy spy(&model, SIGNAL(modelReset()));
        model.setClient(&client);

        QTRY_VERIFY(spy.count() > 0);
        QVERIFY(model.rowCount());
    }
    QModelIndex i = model.index(0);
    QString id = model.data(i, Enginio::IdRole).value<QJsonValue>().toString();
    QVERIFY(!id.isEmpty());

    QJsonObject o;
    o.insert("id", id);
    o.insert("objectType", objectType);
    EnginioReply *r1 = client.remove(o);
    QTRY_VERIFY(r1->isFinished());

    // the value was removed on the server side,
    QCOMPARE(model.data(i, Enginio::IdRole).value<QJsonValue>().toString(), id);
    // but is still in the model cache.

    EnginioReply *r2 = operation(model, 0);

    QTRY_VERIFY(r2->isFinished());
    QVERIFY(r2->isError());
    QCOMPARE(r2->backendStatus(), 404); // abviously item was not found on the server side

    // Let's check if the model was updated
    i = model.index(0);
    QCOMPARE(model.rowCount(), 0);
}

struct ExternallyRemovedSetProperty
{
    EnginioReply *operator()(EnginioModel &model, int row) const
    {
        return model.setData(row, "areYouDeleted?", propertyName());
    }
    QString propertyName() const
    {
        return "title";
    }
};

void tst_EnginioModel::setPropertyOnExternallyRemovedObject()
{
    externallyRemovedImpl<ExternallyRemovedSetProperty>();
}

struct ExternallyRemovedRemove
{
    EnginioReply *operator()(EnginioModel &model, int row) const
    {
        return model.remove(row);
    }
    QString propertyName() const
    {
        return "title";
    }
};

void tst_EnginioModel::removeExternallyRemovedObject()
{
    externallyRemovedImpl<ExternallyRemovedRemove>();
}

void tst_EnginioModel::createAndModify()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);;

    QString propertyName = "title";
    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);
    query.insert("limit", 1);

    EnginioModel model;
    model.disableNotifications();
    model.setQuery(query);
    {   // init the model
        QSignalSpy spy(&model, SIGNAL(modelReset()));
        model.setClient(&client);
        QTRY_VERIFY(spy.count() > 0);
    }

    QJsonObject o;
    o.insert(propertyName, QString::fromLatin1("o"));
    o.insert("objectType", objectType);

    {
        // create and immediatelly remove
        const int initialRowCount = model.rowCount();
        EnginioReply *r1 = model.append(o);
        QVERIFY(!r1->isFinished());
        QVERIFY(!r1->isError());

        EnginioReply *r2 = model.remove(model.rowCount() - 1);
        QVERIFY(!r2->isFinished());
        QVERIFY(!r2->isError());
        r2->setDelayFinishedSignal(true);

        QTRY_VERIFY(r1->isFinished());
        QModelIndex i = model.index(model.rowCount() - 1);
        QCOMPARE(model.data(i, Enginio::SyncedRole).value<bool>(), false);
        r2->setDelayFinishedSignal(false);

        QTRY_VERIFY(r2->isFinished());
        CHECK_NO_ERROR(r2);
        QTRY_COMPARE(model.rowCount(), initialRowCount);
    }
    {
        // create and immediatelly update
        const int initialRowCount = model.rowCount();
        EnginioReply *r1 = model.append(o);
        QVERIFY(!r1->isFinished());
        QVERIFY(!r1->isError());

        EnginioReply *r2 = model.setData(model.rowCount() - 1, QString::fromLatin1("newO"), propertyName);
        QVERIFY(!r2->isFinished());
        QVERIFY(!r2->isError());
        r2->setDelayFinishedSignal(true);

        QTRY_VERIFY(r1->isFinished());
        QModelIndex i = model.index(model.rowCount() - 1);
        QCOMPARE(model.data(i, Enginio::SyncedRole).value<bool>(), false);
        r2->setDelayFinishedSignal(false);

        QTRY_VERIFY(r2->isFinished());
        QCOMPARE(model.rowCount(), initialRowCount + 1);
        QVERIFY(!r2->isError());
        i = model.index(model.rowCount() - 1);
        QCOMPARE(model.data(i).value<QJsonValue>().toObject()["title"].toString(), QString::fromLatin1("newO"));
        QCOMPARE(model.data(i, Enginio::SyncedRole).value<bool>(), true);
    }
}

int externalNotificationFindHelper(EnginioModel &model, const QString propertyName, const QJsonObject &o)
{
    for (int idx = 0; idx < model.rowCount(); ++idx) {
        QModelIndex i = model.index(idx);
        if (model.data(i).toJsonValue().toObject()[propertyName].toString()
                == o[propertyName].toString()) {
            return idx;
        }
    }
    return -1;
}

void tst_EnginioModel::externalNotification()
{
    if (EnginioTests::TESTAPP_URL != "https://staging.engin.io")
        QSKIP("The test depands on notifications, which are enabled only in staging environment");

    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString propertyName = "title";
    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);

    EnginioModel model;
    model.setQuery(query);
    {   // init the model
        QSignalSpy spy(&model, SIGNAL(modelReset()));
        model.setClient(&client);
        QTRY_VERIFY(spy.count() > 0);
    }

    QJsonObject o;
    o.insert(propertyName, QString::fromLatin1("externalNotification"));
    o.insert("objectType", objectType);
    {
        // insert a new object
        //TODO should we wait for the websocket?
        const int initialRowCount = model.rowCount();

        EnginioReply *r = client.create(o);
        QTRY_VERIFY(r->isFinished());
        CHECK_NO_ERROR(r);
        o = r->data();
        QTRY_COMPARE(model.rowCount(), initialRowCount + 1);
        QVERIFY(externalNotificationFindHelper(model, propertyName, o) != -1);
    }
    {
        // modify an object
        o.insert(propertyName, QString::fromLatin1("externalNotification_modified"));
        EnginioReply *r = client.update(o);
        QTRY_VERIFY(r->isFinished());
        CHECK_NO_ERROR(r);

        int idx;
        for (idx = 0; idx < model.rowCount(); ++idx) {
            QModelIndex i = model.index(idx);
            if (model.data(i).toJsonValue().toObject()[propertyName].toString()
                    == o[propertyName].toString()) {
                break;
            }
        }
        QTRY_VERIFY(externalNotificationFindHelper(model, propertyName, o) != -1);
    }
    {
        // remove an object
        const int initialRowCount = model.rowCount();

        EnginioReply *r = client.remove(o);
        QTRY_VERIFY(r->isFinished());
        QTRY_COMPARE(model.rowCount(), initialRowCount - 1);

        int idx;
        for (idx = 0; idx < model.rowCount(); ++idx) {
            QModelIndex i = model.index(idx);
            if (model.data(i).toJsonValue().toObject()[propertyName].toString()
                    == o[propertyName].toString()) {
                break;
            }
        }
        QTRY_VERIFY(externalNotificationFindHelper(model, propertyName, o) == -1);
    }
}

void tst_EnginioModel::createUpdateRemoveWithNotification()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString propertyName = "title";
    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);

    EnginioModel model;
    model.setQuery(query);
    {   // init the model
        QSignalSpy spy(&model, SIGNAL(modelReset()));
        model.setClient(&client);
        QTRY_VERIFY(spy.count() > 0);
    }

    const int repliesCount = 12;
    const int initialCount = model.rowCount();
    QJsonObject o;
    o.insert("objectType", objectType);

    QVector<EnginioReply *> replies;
    replies.reserve(repliesCount);
    for (int i = 0; i < replies.capacity(); ++i) {
        o.insert(propertyName, QString::fromLatin1("withNotification") + QString::number(i));
        replies << model.append(o);
    }

    // wait for responses first, then for rowCount
    // so if the test is broken rowCount would be bigger then replies.count()
    foreach (EnginioReply *reply, replies) {
        QTRY_VERIFY_WITH_TIMEOUT(reply->isFinished(), 10000);
        CHECK_NO_ERROR(reply);
    }
    QTRY_COMPARE(model.rowCount(), initialCount + repliesCount);


    // lets try to update our objects
    replies.resize(0);
    for (int i = 0; i < model.rowCount(); ++i) {
        QModelIndex idx = model.index(i);
        QJsonObject o = model.data(idx).toJsonValue().toObject();
        if (o[propertyName].toString().startsWith("withNotification")) {
            replies << model.setData(i, QString::fromLatin1("withNotification_mod"), propertyName);
        }
    }
    QVERIFY(replies.count() == repliesCount);
    foreach (EnginioReply *reply, replies) {
        QTRY_VERIFY_WITH_TIMEOUT(reply->isFinished(), 10000);
        CHECK_NO_ERROR(reply);
    }
    QTRY_COMPARE(model.rowCount(), initialCount + repliesCount);
    int counter = 0;
    for (int i = 0; i < model.rowCount(); ++i) {
        QModelIndex idx = model.index(i);
        QJsonObject o = model.data(idx).toJsonValue().toObject();
        if (o[propertyName].toString().startsWith("withNotification_mod")) {
            counter++;
        }
    }
    QCOMPARE(counter, repliesCount);

    // lets remove our objects
    replies.resize(0);
    for (int i = 0; i < model.rowCount(); ++i) {
        QModelIndex idx = model.index(i);
        QJsonObject o = model.data(idx).toJsonValue().toObject();
        if (o[propertyName].toString().startsWith("withNotification")) {
            replies << model.remove(i);
        }
    }
    QCOMPARE(replies.count(), repliesCount);
    foreach (EnginioReply *reply, replies) {
        QTRY_VERIFY_WITH_TIMEOUT(reply->isFinished(), 10000);
        CHECK_NO_ERROR(reply);
    }
    QTRY_COMPARE(model.rowCount(), initialCount);
}

void tst_EnginioModel::appendBeforeInitialModelReset()
{
    // The test is trying to append data to a model before it is initially populated.
    // This may be flaky, because it depends on a initial query being slower then append
    // that is why it is executad in a loop.

    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);
    for (int i = 0; i < 24 ; ++i) {
        QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
        QJsonObject query;
        query.insert("objectType", objectType);

        EnginioModel model;
        QSignalSpy resetSpy(&model, SIGNAL(modelReset()));
        model.setQuery(query);
        model.setClient(&client);

        query.insert("title", QString::fromUtf8("appendAndRemoveModel"));
        EnginioReply *reply = model.append(query);
        QTRY_VERIFY(reply->isFinished());
        CHECK_NO_ERROR(reply);
        if (resetSpy.isEmpty())
            break;
    }
}


void tst_EnginioModel::delayedRequestBeforeInitialModelReset()
{
    // This test is an extension of tst_EnginioModel::appendBeforeInitialModelReset()
    // The test is trying to append data and modify it before model is initially populated.
    // This may be flaky, because it depends on a initial query being slower then append
    // that is why it is executad in a loop.

    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);
    query.insert("title", QString::fromUtf8("appendAndRemoveModel"));

    QJsonObject object;
    object["title"] = "New Title";

    for (int i = 0; i < 24 ; ++i) {
        EnginioModel model;
        QSignalSpy resetSpy(&model, SIGNAL(modelReset()));
        model.setQuery(query);
        model.setClient(&client);

        EnginioReply *append1 = model.append(query);
        EnginioReply *append2 = model.append(query);
        EnginioReply *update1 = model.setData(0, QString::fromUtf8("appendAndRemoveModel1"), "title");
        EnginioReply *update2 = model.setData(0, object);
        EnginioReply *remove = model.remove(1);
        QTRY_VERIFY(append1->isFinished() && append2->isFinished() && remove->isFinished() && update1->isFinished() && update2->isFinished());
        QVERIFY(!append1->isError() || append1->errorString().contains("EnginioModel: The query was changed before the request could be sent"));
        QVERIFY(!append2->isError() || append2->errorString().contains("EnginioModel: The query was changed before the request could be sent"));
        QVERIFY(!update1->isError() || update1->errorString().contains("EnginioModel: The query was changed before the request could be sent"));
        QVERIFY(!update2->isError() || update1->errorString().contains("EnginioModel: The query was changed before the request could be sent"));
        QVERIFY(!remove->isError() || remove->errorString().contains("EnginioModel: The query was changed before the request could be sent"));
        if (resetSpy.isEmpty())
            break;
    }
}

void tst_EnginioModel::appendAndChangeQueryBeforeItIsFinished()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);

    EnginioModel model;
    QSignalSpy resetSpy(&model, SIGNAL(modelReset()));
    model.setQuery(query);
    model.setClient(&client);
    QTRY_COMPARE(resetSpy.count(), 1);

    query.insert("title", QString::fromUtf8("appendAndChangeQueryBeforeItIsFinished"));
    EnginioReply *reply = model.append(query);
    reply->setDelayFinishedSignal(true);

    {   // change query
        QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT2;
        QJsonObject query;
        query.insert("objectType", objectType);
        model.setQuery(query);
        QVERIFY(resetSpy.count() != 2);
        QTRY_COMPARE(resetSpy.count(), 2);
        reply->setDelayFinishedSignal(false);
    }
    QTRY_VERIFY(reply->isFinished());
    CHECK_NO_ERROR(reply);

    QString appendedId = reply->data()["id"].toString();
    for (int i = 0; i < model.rowCount(); ++i) {
        QString id = model.data(model.index(i)).toJsonValue().toObject()["id"].toString();
        QVERIFY(id != appendedId);
    }
}

void tst_EnginioModel::deleteModelDurringRequests()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QVarLengthArray<EnginioReply*, 12> replies;
    {
        QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
        QJsonObject query;
        query.insert("objectType", objectType);
        EnginioModel model;
        model.setQuery(query);
        model.setClient(&client);

        query.insert("title", QString::fromUtf8("deleteModelDurringRequests"));
        QJsonObject object = query;
        object.insert("count", 111);

        replies.append(model.append(query));
        replies.append(model.append(query));
        replies.append(model.setData(0, QString::fromUtf8("deleteModelDurringRequests1"), "title"));
        replies.append(model.setData(1, object));
        replies.append(model.remove(0));
    }

    foreach (EnginioReply *reply, replies)
        QTRY_VERIFY(reply->isFinished());

    CHECK_NO_ERROR(replies[0]);
    CHECK_NO_ERROR(replies[1]);

    for (int i = 2; i < replies.count(); ++i) {
        QVERIFY(replies[i]->isError());
        QCOMPARE(replies[i]->errorType(), Enginio::BackendError);
        QCOMPARE(replies[i]->backendStatus(), 400);
        QVERIFY(!replies[i]->errorString().isEmpty());
        QVERIFY(!replies[i]->data().isEmpty());
    }
}

void tst_EnginioModel::updatingRoles()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);

    QTest::ignoreMessage(QtWarningMsg, "Can not use custom role index lower then Enginio::CustomPropertyRole, but '261' was used for 'invalid'");
    struct CustomModel: public EnginioModel
    {
        enum {
            BarRole = Enginio::CustomPropertyRole,
            FooRole = Enginio::CustomPropertyRole + 1,
            TitleRole = Enginio::CustomPropertyRole + 20, // existing custom role
            InvalidRole = Enginio::ObjectTypeRole, // custom and with wrong index
            IdRole = Enginio::IdRole // duplicate of existing role
        };

        bool useBaseClassImplementation;
        QHash<int, QByteArray> roles;

        CustomModel()
            : useBaseClassImplementation(false)
        {
            roles = EnginioModel::roleNames();
            roles.insert(CustomModel::FooRole, "foo");
            roles.insert(CustomModel::BarRole, "bar");
            roles.insert(CustomModel::IdRole, "id");
            roles.insert(CustomModel::TitleRole, "title");
            roles.insert(CustomModel::InvalidRole, "invalid");
        }

        virtual QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE
        {
            return useBaseClassImplementation ? EnginioModel::roleNames() : roles;
        }
    } model;

    QByteArray foo = "foo";
    QByteArray bar = "bar";
    QByteArray title = "title";
    QByteArray invalid = "invalid";

    QCOMPARE(model.roleNames()[CustomModel::FooRole], foo);
    QCOMPARE(model.roleNames()[CustomModel::BarRole], bar);
    QCOMPARE(model.roleNames()[CustomModel::TitleRole], title);
    QCOMPARE(model.roleNames()[CustomModel::InvalidRole], invalid);

    model.setQuery(query);
    model.setClient(&client);

    QCOMPARE(model.roleNames()[CustomModel::FooRole], foo);
    QCOMPARE(model.roleNames()[CustomModel::BarRole], bar);
    QCOMPARE(model.roleNames()[CustomModel::TitleRole], title);
    QCOMPARE(model.roleNames()[CustomModel::InvalidRole], invalid);

    QTRY_VERIFY(model.rowCount());

    QCOMPARE(model.roleNames()[CustomModel::FooRole], foo);
    QCOMPARE(model.roleNames()[CustomModel::BarRole], bar);
    QCOMPARE(model.roleNames()[CustomModel::TitleRole], title);
    QCOMPARE(model.roleNames()[CustomModel::InvalidRole], invalid);

    model.useBaseClassImplementation = true;

    QCOMPARE(model.roleNames()[CustomModel::FooRole], foo);
    QCOMPARE(model.roleNames()[CustomModel::BarRole], bar);
    QCOMPARE(model.roleNames()[CustomModel::TitleRole], title);
    QCOMPARE(model.roleNames()[CustomModel::InvalidRole], QByteArray("objectType"));
}

struct CustomModel: public EnginioModel {
    enum Roles {
        TitleRole = Enginio::CustomPropertyRole,
        CountRole
    };

    virtual QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE
    {
        QHash<int, QByteArray> roles = EnginioModel::roleNames();
        roles.insert(TitleRole, "title");
        roles.insert(CountRole, "count");
        return roles;
    }
};

void tst_EnginioModel::setData()
{
    QString propertyName = "title";
    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);

    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    CustomModel model;

    model.disableNotifications();
    model.setQuery(query);

    {   // init the model
        QSignalSpy spy(&model, SIGNAL(modelReset()));
        model.setClient(&client);

        QTRY_VERIFY(spy.count() > 0);
    }

    if (model.rowCount() < 1) {
        QJsonObject o;
        o.insert(propertyName, QString::fromLatin1("o"));
        o.insert("objectType", objectType);
        model.append(o);
    }

    // try to get data through an invalid index
    QCOMPARE(model.data(model.index(-1)), QVariant());
    QCOMPARE(model.data(model.index(-1, 1)), QVariant());
    QCOMPARE(model.data(model.index(model.rowCount() + 3)), QVariant());
    QCOMPARE(model.data(model.index(model.rowCount())), QVariant());

    QTRY_VERIFY(model.rowCount() > 0);

    // try to set data through an invalid index
    QVERIFY(!model.setData(model.index(model.rowCount()), QVariant()));
    QVERIFY(!model.setData(model.index(model.rowCount() + 3), QVariant()));
    QVERIFY(!model.setData(model.index(-1), QVariant()));
    QVERIFY(!model.setData(model.index(-1, 1), QVariant()));

    // make a correct setData call
    QVERIFY(model.setData(model.index(0), QString::fromLatin1("1111"), CustomModel::TitleRole));
}

void tst_EnginioModel::setJsonData()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString propertyName1 = "title";
    QString propertyName2 = "count";
    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);

    CustomModel model;

    model.disableNotifications();
    model.setQuery(query);

    {   // init the model
        QSignalSpy spy(&model, SIGNAL(modelReset()));
        model.setClient(&client);

        QTRY_VERIFY(spy.count() > 0);
    }

    if (model.rowCount() < 1) {
        QJsonObject o;
        o.insert(propertyName1, QString::fromLatin1("o"));
        o.insert(propertyName2, 123);
        o.insert("objectType", objectType);
        model.append(o);
    }

    QTRY_VERIFY(model.rowCount());

    QJsonObject object = model.data(model.index(0)).toJsonValue().toObject();
    QVERIFY(!object.isEmpty());

    object[propertyName1] = object[propertyName1].toString() + QString::fromLatin1("o");
    object[propertyName2] = object[propertyName2].toInt() + 123;

    EnginioReply *reply = model.setData(0, object);
    QTRY_VERIFY(reply->isFinished());
    CHECK_NO_ERROR(reply);

    QJsonObject data = reply->data();
    QCOMPARE(data[propertyName1], object[propertyName1]);
    QCOMPARE(data[propertyName2], object[propertyName2]);
}

void tst_EnginioModel::data()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString propertyName1 = "title";
    QString propertyName2 = "count";
    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);

    CustomModel model;

    model.disableNotifications();
    model.setQuery(query);

    {   // init the model
        QSignalSpy spy(&model, SIGNAL(modelReset()));
        model.setClient(&client);

        QTRY_VERIFY(spy.count() > 0);
    }

    if (model.rowCount() < 1) {
        QJsonObject o;
        o.insert(propertyName1, QString::fromLatin1("o"));
        o.insert(propertyName2, 123);
        o.insert("objectType", objectType);
        model.append(o);
    }

    QTRY_VERIFY(model.rowCount());
    QModelIndex index = model.index(0);
    QJsonObject item = model.data(index, Enginio::JsonObjectRole).toJsonValue().toObject();
    QVERIFY(!item.isEmpty());

    QCOMPARE(model.data(index, Enginio::IdRole).toJsonValue(), QJsonValue(item["id"]));
    QCOMPARE(model.data(index, Enginio::CreatedAtRole).toJsonValue(), QJsonValue(item["createdAt"]));
    QCOMPARE(model.data(index, Enginio::UpdatedAtRole).toJsonValue(), QJsonValue(item["updatedAt"]));
}

void tst_EnginioModel::setInvalidJsonData()
{
    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QString propertyName1 = "title";
    QString propertyName2 = "count";
    QString objectType = "objects." + EnginioTests::CUSTOM_OBJECT1;
    QJsonObject query;
    query.insert("objectType", objectType);

    CustomModel model;

    model.disableNotifications();
    model.setQuery(query);

    QJsonObject object;
    {
        QSignalSpy spy(&model, SIGNAL(modelReset()));
        model.setClient(&client);

        object.insert(propertyName1, QString::fromLatin1("o"));
        object.insert(propertyName2, 123);
        object.insert("objectType", objectType);

        QTRY_VERIFY(spy.count() > 0);
    }

    if (model.rowCount() < 1) {
        QJsonObject o;
        o.insert(propertyName1, QString::fromLatin1("o"));
        o.insert(propertyName2, 123);
        o.insert("objectType", objectType);
        model.append(o);
    }

    QVector<EnginioReply *> replies;
    replies.append(model.setData(-1, object));
    replies.append(model.setData(model.rowCount(), object));

    QTRY_VERIFY(model.rowCount());
    replies.append(model.setData(0, QJsonObject()));

    foreach (const EnginioReply *reply, replies){
        QTRY_VERIFY(reply->isFinished());
        QVERIFY(reply->isError());
    }
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

void tst_EnginioModel::deleteReply()
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

    QJsonObject query;
    query.insert("limit", 1);

    EnginioModel model;
    model.disableNotifications();
    model.setOperation(Enginio::UserOperation);
    model.setQuery(query);
    model.setClient(&client);

    QJsonObject newUser;
    newUser.insert("username", QString::fromUtf8("fool"));
    newUser.insert("password", QString::fromUtf8("foolPass"));

    QNetworkAccessManager *qnam = client.networkManager();
    QVector<EnginioReply *> replies;

    QTRY_VERIFY(model.rowCount() > 0);

    replies.append(model.append(newUser));
    replies.append(model.append(newUser));
    replies.append(model.remove(model.rowCount() - 1));

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

QJsonObject createTestObject(const QString &name, const QString &type)
{
    QJsonObject obj;
    obj.insert("title", name);
    obj.insert("objectType", type);
    return obj;
}

void tst_EnginioModel::reload()
{
    QString objectType = "objects.reload1";

    EnginioClient client;
    client.setBackendId(_backendId);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QJsonObject query;
    query.insert("objectType", objectType);

    EnginioModel model;
    model.disableNotifications();
    model.setQuery(query);
    model.setClient(&client);

    QCOMPARE(model.rowCount(), 0);

    // create an object, since notification are disabled the model cannot know about it
    EnginioReply *r1(client.create(createTestObject(QString::fromLatin1("o1"), objectType)));
    QTRY_VERIFY(r1->isFinished());
    CHECK_NO_ERROR(r1);
    QCOMPARE(model.rowCount(), 0);

    // reload and verify that the object appears
    EnginioReply *reload(model.reload());
    QTRY_VERIFY(reload->isFinished());
    CHECK_NO_ERROR(reload);
    QCOMPARE(model.rowCount(), 1);

    // this test mostly checks that we don't crash
    // when calling reload while other operations are on-going it is undefined
    // if the result contains the data of them or not

    // create object and reload before that is finished
    EnginioReply *r2(client.create(createTestObject(QString::fromLatin1("o2"), objectType)));
    EnginioReply *reload2 = model.reload();
    QVERIFY(reload2);
    QTRY_VERIFY(r2->isFinished());
    CHECK_NO_ERROR(r2);
    QTRY_VERIFY(reload2->isFinished());
    CHECK_NO_ERROR(reload2);

    // create object bug delay it's response until reload is done
    EnginioReply *r3(client.create(createTestObject(QString::fromLatin1("o3"), objectType)));
    QVERIFY(r3);
    r3->setDelayFinishedSignal(true);
    EnginioReply *reload3(model.reload());
    QVERIFY(reload3);
    QTRY_VERIFY(reload3->isFinished());
    r3->setDelayFinishedSignal(false);
    QTRY_VERIFY(r3->isFinished());

    // make sure we are in a defined state again
    reload = model.reload();
    QTRY_VERIFY(reload->isFinished());
    QCOMPARE(model.rowCount(), 3);

    EnginioReply *r4(model.append(createTestObject(QString::fromLatin1("o4"), objectType)));
    EnginioReply *reload4(model.reload());
    EnginioReply *r5(model.append(createTestObject(QString::fromLatin1("o5"), objectType)));
    EnginioReply *reload5(model.reload());
    QTRY_VERIFY(r4->isFinished() && reload4->isFinished() && r5->isFinished() && reload5->isFinished());

    reload = model.reload();
    QTRY_VERIFY(reload->isFinished());
    QCOMPARE(model.rowCount(), 5);

    // randomly reordered append and reset calls
    EnginioReply *r6(model.append(createTestObject(QString::fromLatin1("o6"), objectType)));
    r6->setDelayFinishedSignal(true);
    EnginioReply *reload6(model.reload());
    reload6->setDelayFinishedSignal(true);
    EnginioReply *r7(model.append(createTestObject(QString::fromLatin1("o7"), objectType)));
    r7->setDelayFinishedSignal(true);
    EnginioReply *reload7(model.reload());
    EnginioReply *c7(model.setData(6, QString::fromLatin1("object7"), QString::fromLatin1("title")));
    EnginioReply *c5(model.setData(4, QString::fromLatin1("object5"), QString::fromLatin1("title")));
    QTRY_VERIFY(reload7->isFinished());
    r6->setDelayFinishedSignal(false);
    r7->setDelayFinishedSignal(false);
    reload6->setDelayFinishedSignal(false);
    QTRY_VERIFY(c5->isFinished() && c7->isFinished() && r6->isFinished() && r7->isFinished() && reload6->isFinished());

    reload = model.reload();
    QTRY_VERIFY(reload->isFinished());
    QCOMPARE(model.rowCount(), 7);

    // completely try to mess it up by changing the query
    // object type reload2 is empty
    EnginioReply *reload8 = model.reload();
    reload8->setDelayFinishedSignal(true);
    QJsonObject query2;
    query2.insert("objectTypes", "objects.reload2");
    model.setQuery(query2);

    QSignalSpy spy(&model, SIGNAL(modelReset()));
    QTRY_VERIFY(spy.count());
    reload8->setDelayFinishedSignal(false);
    QTRY_VERIFY(reload8->isFinished());
    QCOMPARE(model.rowCount(), 0);
}

void tst_EnginioModel::identityChange()
{
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId("5376019e698b3c6ad500095a");

    QJsonObject query;
    query["objectType"] = "objects.EnginioModelIdentityChange";

    EnginioModel model;
    model.setQuery(query);
    model.setClient(&client);

    EnginioOAuth2Authentication identity1;
    identity1.setUser("test1");
    identity1.setPassword("test1");

    EnginioOAuth2Authentication identity2;
    identity2.setUser("test2");
    identity2.setPassword("test2");

    QTRY_COMPARE(model.rowCount(), 1);  // There is only one publicly visible element

    client.setIdentity(&identity1);
    QTRY_COMPARE(model.rowCount(), 2);  // Test1 sees one additional element

    client.setIdentity(&identity2);
    QTRY_COMPARE(model.rowCount(), 3);  // Test2 sees two additional elements

    client.setIdentity(0);
    QTRY_COMPARE(model.rowCount(), 1);
}

QTEST_MAIN(tst_EnginioModel)
#include "tst_enginiomodel.moc"
