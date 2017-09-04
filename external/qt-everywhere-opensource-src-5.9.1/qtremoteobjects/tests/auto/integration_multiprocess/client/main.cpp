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

#include "rep_MyInterface_replica.h"

#include <QCoreApplication>
#include <QtRemoteObjects/qremoteobjectnode.h>
#include <QtTest/QtTest>

class tst_Client_Process : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRun()
    {
        QRemoteObjectNode repNode;
        repNode.connectToNode(QUrl(QStringLiteral("tcp://127.0.0.1:65213")));

        QSharedPointer<MyInterfaceReplica> rep(repNode.acquire<MyInterfaceReplica>());
        QVERIFY(rep->waitForSource());

        auto reply = rep->start();
        QVERIFY(reply.waitForFinished());

        // BEGIN: Testing
        QSignalSpy advanceSpy(rep.data(), SIGNAL(advance()));

        QSignalSpy spy(rep.data(), SIGNAL(enum1Changed(MyInterfaceReplica::Enum1)));
        QVERIFY(advanceSpy.wait());

        QCOMPARE(spy.count(), 2);
        // END: Testing

        reply = rep->stop();
        QVERIFY(reply.waitForFinished());

        qDebug() << "Done. Shutting down.";

        // wait for delivery of events
        QTest::qWait(200);
    }
};

QTEST_MAIN(tst_Client_Process)

#include "main.moc"
