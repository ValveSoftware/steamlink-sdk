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

#ifndef ENGINIOTESTSCOMMON_H
#define ENGINIOTESTSCOMMON_H

#include <Enginio/enginioclient.h>
#include <QtCore/qmap.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qurl.h>

#if !defined(QTRY_VERIFY)
#error QtTest library has to be included before common.h
#endif

#undef QTRY_VERIFY
#undef QTRY_COMPARE

#define QTRY_VERIFY(Test) QTRY_VERIFY_WITH_TIMEOUT(Test, 15000)
#define QTRY_COMPARE(Test1, Test2) QTRY_COMPARE_WITH_TIMEOUT(Test1, Test2, 15000)

#define CHECK_NO_ERROR(response) \
    if (!response->isFinished()) {\
        QTRY_VERIFY(response->isFinished()); \
        QVERIFY2(false, "response->isFinished() returned false, but waiting helped, probably it is a test case problem");\
    } \
    QVERIFY(response->isFinished()); \
    QVERIFY(!response->isError()); \
    QCOMPARE(response->errorType(), Enginio::NoError);\
    QCOMPARE(response->networkError(), QNetworkReply::NoError);\
    QVERIFY(response->backendStatus() >= 200 && response->backendStatus() < 300);


QT_FORWARD_DECLARE_CLASS(EnginioReply);

namespace EnginioTests
{

const QString TESTAPP_URL(qgetenv("ENGINIO_API_URL"));
const QString TESTAPP_ENV = QStringLiteral("development");
const QString CUSTOM_OBJECT1(QStringLiteral("CustomObject1"));
const QString CUSTOM_OBJECT2(QStringLiteral("CustomObject2"));

class EnginioBackendManager: public QObject
{
    Q_OBJECT

    EnginioClient _client;
    QJsonObject _headers;
    QJsonObject _responseData;
    QMap<QString, QJsonArray> _backendEnvironments;
    QString _email;
    QString _password;

    bool synchronousRequest(const QUrl &url, const QByteArray &httpOperation, const QJsonObject &data = QJsonObject());
    bool removeAppWithId(const QString &appId);
    bool authenticate();
    QString getAppId(const QString &backendName) Q_REQUIRED_RESULT;
    QJsonArray getAllBackends() Q_REQUIRED_RESULT;
    QJsonArray getEnvironments(const QString &backendName);

public slots:
    void error(EnginioReply *reply);
    void finished(EnginioReply* reply);

public:
    explicit EnginioBackendManager(QObject *parent = 0);
    virtual ~EnginioBackendManager();
    bool createBackend(const QString &backendName);
    bool removeBackend(const QString &backendName);
    bool createObjectType(const QString &backendName, const QString &environment, const QJsonObject &schema);
    QJsonObject backendApiKeys(const QString &backendName, const QString &environment);
};

void prepareTestUsersAndUserGroups(const QByteArray &backendId);
bool prepareTestObjectType(const QString &backendName);

}

#endif // ENGINIOTESTSCOMMON_H
