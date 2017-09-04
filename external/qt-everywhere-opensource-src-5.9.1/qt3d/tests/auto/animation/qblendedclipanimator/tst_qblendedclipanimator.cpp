/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DAnimation/qblendedclipanimator.h>
#include <Qt3DAnimation/private/qblendedclipanimator_p.h>
#include <Qt3DAnimation/qlerpclipblend.h>
#include <Qt3DAnimation/qchannelmapper.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <QObject>
#include <QSignalSpy>
#include <testpostmanarbiter.h>

class tst_QBlendedClipAnimator : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DAnimation::QChannelMapper*>("QChannelMapper *");
        qRegisterMetaType<Qt3DAnimation::QAbstractClipBlendNode*>("QAbstractClipBlendNode *");
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DAnimation::QBlendedClipAnimator blendedClipAnimator;

        // THEN
        QVERIFY(blendedClipAnimator.blendTree() == nullptr);
        QVERIFY(blendedClipAnimator.channelMapper() == nullptr);
        QCOMPARE(blendedClipAnimator.isRunning(), false);
        QCOMPARE(blendedClipAnimator.loopCount(), 1);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DAnimation::QBlendedClipAnimator blendedClipAnimator;

        {
            // WHEN
            QSignalSpy spy(&blendedClipAnimator, SIGNAL(blendTreeChanged(QAbstractClipBlendNode *)));
            Qt3DAnimation::QLerpClipBlend newValue;
            blendedClipAnimator.setBlendTree(&newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(blendedClipAnimator.blendTree(), &newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            blendedClipAnimator.setBlendTree(&newValue);

            // THEN
            QCOMPARE(blendedClipAnimator.blendTree(), &newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&blendedClipAnimator, SIGNAL(channelMapperChanged(QChannelMapper *)));
            Qt3DAnimation::QChannelMapper newValue;
            blendedClipAnimator.setChannelMapper(&newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(blendedClipAnimator.channelMapper(), &newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            blendedClipAnimator.setChannelMapper(&newValue);

            // THEN
            QCOMPARE(blendedClipAnimator.channelMapper(), &newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&blendedClipAnimator, SIGNAL(runningChanged(bool)));
            const bool newValue = true;
            blendedClipAnimator.setRunning(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(blendedClipAnimator.isRunning(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            blendedClipAnimator.setRunning(newValue);

            // THEN
            QCOMPARE(blendedClipAnimator.isRunning(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&blendedClipAnimator, SIGNAL(loopCountChanged(int)));
            const int newValue = 5;
            blendedClipAnimator.setLoopCount(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(blendedClipAnimator.loopCount(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            blendedClipAnimator.setLoopCount(newValue);

            // THEN
            QCOMPARE(blendedClipAnimator.loopCount(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DAnimation::QBlendedClipAnimator blendedClipAnimator;
        Qt3DAnimation::QChannelMapper channelMapper;
        Qt3DAnimation::QLerpClipBlend blendRoot;

        blendedClipAnimator.setBlendTree(&blendRoot);
        blendedClipAnimator.setChannelMapper(&channelMapper);
        blendedClipAnimator.setRunning(true);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&blendedClipAnimator);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 3); // 1 + mapper + blend tree root

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DAnimation::QBlendedClipAnimatorData>>(creationChanges.first());
            const Qt3DAnimation::QBlendedClipAnimatorData cloneData = creationChangeData->data;

            QCOMPARE(blendedClipAnimator.blendTree()->id(), cloneData.blendTreeRootId);
            QCOMPARE(blendedClipAnimator.channelMapper()->id(), cloneData.mapperId);
            QCOMPARE(blendedClipAnimator.isRunning(), cloneData.running);
            QCOMPARE(blendedClipAnimator.id(), creationChangeData->subjectId());
            QCOMPARE(blendedClipAnimator.isEnabled(), true);
            QCOMPARE(blendedClipAnimator.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(blendedClipAnimator.metaObject(), creationChangeData->metaObject());
            QCOMPARE(blendedClipAnimator.loopCount(), cloneData.loops);
        }

        // WHEN
        blendedClipAnimator.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&blendedClipAnimator);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 3); // 1 + mapper + blend tree root

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DAnimation::QBlendedClipAnimatorData>>(creationChanges.first());
            const Qt3DAnimation::QBlendedClipAnimatorData cloneData = creationChangeData->data;

            QCOMPARE(blendedClipAnimator.blendTree()->id(), cloneData.blendTreeRootId);
            QCOMPARE(blendedClipAnimator.channelMapper()->id(), cloneData.mapperId);
            QCOMPARE(blendedClipAnimator.isRunning(), cloneData.running);
            QCOMPARE(blendedClipAnimator.id(), creationChangeData->subjectId());
            QCOMPARE(blendedClipAnimator.isEnabled(), false);
            QCOMPARE(blendedClipAnimator.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(blendedClipAnimator.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkBlendTreeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QBlendedClipAnimator blendedClipAnimator;
        arbiter.setArbiterOnNode(&blendedClipAnimator);
        Qt3DAnimation::QLerpClipBlend blendRoot;

        {
            // WHEN
            blendedClipAnimator.setBlendTree(&blendRoot);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "blendTree");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), blendRoot.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            blendedClipAnimator.setBlendTree(&blendRoot);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkBlendTreeBookkeeping()
    {
        // GIVEN
        Qt3DAnimation::QBlendedClipAnimator blendedClipAnimator;

        {
            // WHEN
            Qt3DAnimation::QLerpClipBlend blendRoot;
            blendedClipAnimator.setBlendTree(&blendRoot);

            QCOMPARE(blendedClipAnimator.blendTree(), &blendRoot);
        }

        // THEN -> should not crash
        QVERIFY(blendedClipAnimator.blendTree() == nullptr);
    }

    void checkChannelMapperUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QBlendedClipAnimator blendedClipAnimator;
        arbiter.setArbiterOnNode(&blendedClipAnimator);

        Qt3DAnimation::QChannelMapper channelMapper;
        {
            // WHEN
            blendedClipAnimator.setChannelMapper(&channelMapper);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "channelMapper");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), channelMapper.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            blendedClipAnimator.setChannelMapper(&channelMapper);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkChannelMapperBookkeeping()
    {
        // GIVEN
        Qt3DAnimation::QBlendedClipAnimator blendedClipAnimator;

        {
            // WHEN
            Qt3DAnimation::QChannelMapper channelMapper;
            blendedClipAnimator.setChannelMapper(&channelMapper);

            QCOMPARE(blendedClipAnimator.channelMapper(), &channelMapper);
        }

        // THEN -> should not crash
        QVERIFY(blendedClipAnimator.channelMapper() == nullptr);
    }

    void checkRunningUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QBlendedClipAnimator blendedClipAnimator;
        arbiter.setArbiterOnNode(&blendedClipAnimator);

        {
            // WHEN
            blendedClipAnimator.setRunning(true);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "running");
            QCOMPARE(change->value().value<bool>(), blendedClipAnimator.isRunning());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            blendedClipAnimator.setRunning(true);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkLoopsUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QBlendedClipAnimator blendedClipAnimator;
        arbiter.setArbiterOnNode(&blendedClipAnimator);

        {
            // WHEN
            blendedClipAnimator.setLoopCount(1584);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "loops");
            QCOMPARE(change->value().value<int>(), blendedClipAnimator.loopCount());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            blendedClipAnimator.setLoopCount(1584);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }
};

QTEST_MAIN(tst_QBlendedClipAnimator)

#include "tst_qblendedclipanimator.moc"
