/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#include "rep_server_source.h"

#include <QCoreApplication>
#include <QtTest/QtTest>

class MyTestServer : public TestClassSimpleSource
{
    Q_OBJECT
public:
    bool shouldQuit = false;
    // TestClassSimpleSource interface
public slots:
    bool slot1() override {return true;}
    QString slot2() override {return QLatin1String("Hello there");}
    void ping(const QString &message) override
    {
        emit pong(message);
    }

    bool quit() override
    {
        qDebug() << "quit() called";

        return shouldQuit = true;
    }
};

class tst_Server_Process : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRun()
    {
        QRemoteObjectHost srcNode(QUrl(QStringLiteral("tcp://127.0.0.1:65214")));
        MyTestServer myTestServer{};

        srcNode.enableRemoting(&myTestServer);

        qDebug() << "Waiting for incoming connections";

        QTRY_VERIFY_WITH_TIMEOUT(myTestServer.shouldQuit, 20000); // wait up to 20s

        qDebug() << "Stopping server";
        QTest::qWait(200); // wait for server to send reply to client invoking quit() function
    }
};

QTEST_MAIN(tst_Server_Process)

#include "main.moc"
