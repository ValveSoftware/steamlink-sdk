/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include <QtTest/QtTest>
#include <Qt3DCore/qaspectjob.h>

using namespace Qt3DCore;

class FakeAspectJob : public QAspectJob
{
public:
    void run() Q_DECL_OVERRIDE {}
};

class tst_QAspectJob : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void shouldAddDependencies()
    {
        // GIVEN
        QAspectJobPtr job1(new FakeAspectJob);
        QAspectJobPtr job2(new FakeAspectJob);
        QAspectJobPtr job3(new FakeAspectJob);

        // THEN
        QVERIFY(job1->dependencies().isEmpty());
        QVERIFY(job2->dependencies().isEmpty());
        QVERIFY(job3->dependencies().isEmpty());

        // WHEN
        job1->addDependency(job2);
        job1->addDependency(job3);

        // THEN
        QCOMPARE(job1->dependencies().size(), 2);
        QCOMPARE(job1->dependencies().at(0).lock(), job2);
        QCOMPARE(job1->dependencies().at(1).lock(), job3);
        QVERIFY(job2->dependencies().isEmpty());
        QVERIFY(job3->dependencies().isEmpty());
    }

    void shouldRemoveDependencies()
    {
        // GIVEN
        QAspectJobPtr job1(new FakeAspectJob);
        QAspectJobPtr job2(new FakeAspectJob);
        QAspectJobPtr job3(new FakeAspectJob);

        job1->addDependency(job2);
        job1->addDependency(job3);

        // WHEN
        job1->removeDependency(job2);

        // THEN
        QCOMPARE(job1->dependencies().size(), 1);
        QCOMPARE(job1->dependencies().at(0).lock(), job3);
    }

    void shouldClearNullDependencies()
    {
        // GIVEN
        QAspectJobPtr job1(new FakeAspectJob);
        QAspectJobPtr job2(new FakeAspectJob);
        QAspectJobPtr job3(new FakeAspectJob);

        job1->addDependency(job2);
        job1->addDependency(job3);

        // WHEN
        job2.clear();
        job1->removeDependency(QWeakPointer<QAspectJob>());

        // THEN
        QCOMPARE(job1->dependencies().size(), 1);
        QCOMPARE(job1->dependencies().at(0).lock(), job3);
    }
};

QTEST_MAIN(tst_QAspectJob)

#include "tst_qaspectjob.moc"
