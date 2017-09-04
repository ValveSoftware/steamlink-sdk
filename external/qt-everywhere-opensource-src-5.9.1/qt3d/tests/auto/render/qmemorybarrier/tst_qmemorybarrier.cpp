/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
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


#include <QtTest/QTest>
#include <Qt3DRender/qmemorybarrier.h>
#include <Qt3DRender/private/qmemorybarrier_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QMemoryBarrier : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DRender::QMemoryBarrier::Operations>("QMemoryBarrier::Operations");
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QMemoryBarrier memoryBarrier;

        // THEN
        QCOMPARE(memoryBarrier.waitOperations(), Qt3DRender::QMemoryBarrier::None);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QMemoryBarrier memoryBarrier;

        {
            // WHEN
            QSignalSpy spy(&memoryBarrier, SIGNAL(waitOperationsChanged(QMemoryBarrier::Operations)));
            const Qt3DRender::QMemoryBarrier::Operations newValue(Qt3DRender::QMemoryBarrier::ShaderStorage|Qt3DRender::QMemoryBarrier::VertexAttributeArray);
            memoryBarrier.setWaitOperations(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(memoryBarrier.waitOperations(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            memoryBarrier.setWaitOperations(newValue);

            // THEN
            QCOMPARE(memoryBarrier.waitOperations(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QMemoryBarrier memoryBarrier;

        memoryBarrier.setWaitOperations(Qt3DRender::QMemoryBarrier::Command);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&memoryBarrier);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QMemoryBarrierData>>(creationChanges.first());
            const Qt3DRender::QMemoryBarrierData cloneData = creationChangeData->data;

            QCOMPARE(memoryBarrier.waitOperations(), cloneData.waitOperations);
            QCOMPARE(memoryBarrier.id(), creationChangeData->subjectId());
            QCOMPARE(memoryBarrier.isEnabled(), true);
            QCOMPARE(memoryBarrier.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(memoryBarrier.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        memoryBarrier.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&memoryBarrier);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QMemoryBarrierData>>(creationChanges.first());
            const Qt3DRender::QMemoryBarrierData cloneData = creationChangeData->data;

            QCOMPARE(memoryBarrier.waitOperations(), cloneData.waitOperations);
            QCOMPARE(memoryBarrier.id(), creationChangeData->subjectId());
            QCOMPARE(memoryBarrier.isEnabled(), false);
            QCOMPARE(memoryBarrier.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(memoryBarrier.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkTypesUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QMemoryBarrier memoryBarrier;
        arbiter.setArbiterOnNode(&memoryBarrier);

        {
            // WHEN
            memoryBarrier.setWaitOperations(Qt3DRender::QMemoryBarrier::ShaderStorage);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "waitOperations");
            QCOMPARE(change->value().value<Qt3DRender::QMemoryBarrier::Operations>(), memoryBarrier.waitOperations());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            memoryBarrier.setWaitOperations(Qt3DRender::QMemoryBarrier::ShaderStorage);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QMemoryBarrier)

#include "tst_qmemorybarrier.moc"
