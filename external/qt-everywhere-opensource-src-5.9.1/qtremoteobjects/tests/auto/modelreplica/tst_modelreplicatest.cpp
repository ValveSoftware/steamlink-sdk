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

#include <QString>
#include <QtTest>
#include "rep_model_merged.h"

class ModelreplicaTest : public QObject
{
    Q_OBJECT

public:
    ModelreplicaTest();

private Q_SLOTS:
    void testCase1();
};

ModelreplicaTest::ModelreplicaTest()
{
}

void ModelreplicaTest::testCase1()
{
    QRemoteObjectRegistryHost host(QUrl("tcp://localhost:5555"));
    QStringListModel *model = new QStringListModel();
    model->setStringList(QStringList() << "Track1" << "Track2" << "Track3");
    MediaSimpleSource source(model);
    host.enableRemoting<MediaSourceAPI>(&source);

    QRemoteObjectNode client(QUrl("tcp://localhost:5555"));
    const QScopedPointer<MediaReplica> replica(client.acquire<MediaReplica>());
    QSignalSpy tracksSpy(replica->tracks(), &QAbstractItemModelReplica::initialized);
    QVERIFY2(replica->waitForSource(100), "Failure");
    QVERIFY(tracksSpy.wait());
    // Rep file only uses display role
    QCOMPARE(QVector<int>{Qt::DisplayRole}, replica->tracks()->availableRoles());

    QCOMPARE(model->rowCount(), replica->tracks()->rowCount());
    for (int i = 0; i < replica->tracks()->rowCount(); i++)
    {
        // We haven't received any data yet
        QCOMPARE(QVariant(), replica->tracks()->data(replica->tracks()->index(i, 0)));
    }

    // Wait for data to be fetch and confirm
    QTest::qWait(100);
    QCOMPARE(model->rowCount(), replica->tracks()->rowCount());
    for (int i = 0; i < replica->tracks()->rowCount(); i++)
    {
        QCOMPARE(model->data(model->index(i), Qt::DisplayRole), replica->tracks()->data(replica->tracks()->index(i, 0)));
    }
}

QTEST_MAIN(ModelreplicaTest)

#include "tst_modelreplicatest.moc"
