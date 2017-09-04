/****************************************************************************
**
** Copyright (C) 2017 Paul Lemire <paul.lemire350@gmail.com>
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
#include <Qt3DAnimation/qlerpclipblend.h>
#include <Qt3DAnimation/qanimationcliploader.h>
#include <Qt3DAnimation/private/qlerpclipblend_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DAnimation/qclipblendnodecreatedchange.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QLerpClipBlend : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        qRegisterMetaType<Qt3DAnimation::QAbstractClipBlendNode*>();
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DAnimation::QLerpClipBlend lerpBlend;

        // THEN
        QCOMPARE(lerpBlend.blendFactor(), 0.0f);
        QCOMPARE(lerpBlend.startClip(), static_cast<Qt3DAnimation::QAbstractClipBlendNode *>(nullptr));
        QCOMPARE(lerpBlend.endClip(), static_cast<Qt3DAnimation::QAbstractClipBlendNode *>(nullptr));
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DAnimation::QLerpClipBlend lerpBlend;

        {
            // WHEN
            QSignalSpy spy(&lerpBlend, SIGNAL(blendFactorChanged(float)));
            const float newValue = 0.5f;
            lerpBlend.setBlendFactor(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(lerpBlend.blendFactor(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            lerpBlend.setBlendFactor(newValue);

            // THEN
            QCOMPARE(lerpBlend.blendFactor(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&lerpBlend, SIGNAL(startClipChanged(Qt3DAnimation::QAbstractClipBlendNode*)));
            auto newValue = new Qt3DAnimation::QLerpClipBlend();
            lerpBlend.setStartClip(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(lerpBlend.startClip(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            lerpBlend.setStartClip(newValue);

            // THEN
            QCOMPARE(lerpBlend.startClip(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&lerpBlend, SIGNAL(endClipChanged(Qt3DAnimation::QAbstractClipBlendNode*)));
            auto newValue = new Qt3DAnimation::QLerpClipBlend();
            lerpBlend.setEndClip(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(lerpBlend.endClip(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            lerpBlend.setEndClip(newValue);

            // THEN
            QCOMPARE(lerpBlend.endClip(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DAnimation::QLerpClipBlend lerpBlend;
        Qt3DAnimation::QLerpClipBlend startClip;
        Qt3DAnimation::QLerpClipBlend endClip;

        lerpBlend.setStartClip(&startClip);
        lerpBlend.setEndClip(&endClip);
        lerpBlend.setBlendFactor(0.8f);


        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&lerpBlend);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 3); // 1 + 2 clips

            const auto creationChangeData = qSharedPointerCast<Qt3DAnimation::QClipBlendNodeCreatedChange<Qt3DAnimation::QLerpClipBlendData>>(creationChanges.first());
            const Qt3DAnimation::QLerpClipBlendData cloneData = creationChangeData->data;

            QCOMPARE(lerpBlend.blendFactor(), cloneData.blendFactor);
            QCOMPARE(lerpBlend.id(), creationChangeData->subjectId());
            QCOMPARE(lerpBlend.isEnabled(), true);
            QCOMPARE(lerpBlend.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(lerpBlend.metaObject(), creationChangeData->metaObject());
            QCOMPARE(cloneData.startClipId, startClip.id());
            QCOMPARE(cloneData.endClipId, endClip.id());
        }

        // WHEN
        lerpBlend.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&lerpBlend);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 3); // 1 + 2 clips

            const auto creationChangeData = qSharedPointerCast<Qt3DAnimation::QClipBlendNodeCreatedChange<Qt3DAnimation::QLerpClipBlendData>>(creationChanges.first());
            const Qt3DAnimation::QLerpClipBlendData cloneData = creationChangeData->data;

            QCOMPARE(lerpBlend.blendFactor(), cloneData.blendFactor);
            QCOMPARE(lerpBlend.id(), creationChangeData->subjectId());
            QCOMPARE(lerpBlend.isEnabled(), false);
            QCOMPARE(lerpBlend.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(lerpBlend.metaObject(), creationChangeData->metaObject());
            QCOMPARE(cloneData.startClipId, startClip.id());
            QCOMPARE(cloneData.endClipId, endClip.id());
        }
    }

    void checkBlendFactorUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QLerpClipBlend lerpBlend;
        arbiter.setArbiterOnNode(&lerpBlend);

        {
            // WHEN
            lerpBlend.setBlendFactor(0.4f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "blendFactor");
            QCOMPARE(change->value().value<float>(), lerpBlend.blendFactor());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            lerpBlend.setBlendFactor(0.4f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkStartClipUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QLerpClipBlend lerpBlend;
        arbiter.setArbiterOnNode(&lerpBlend);
        auto startClip = new Qt3DAnimation::QLerpClipBlend();

        {
            // WHEN
            lerpBlend.setStartClip(startClip);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "startClip");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), lerpBlend.startClip()->id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            lerpBlend.setStartClip(startClip);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }
    }

    void checkEndClipUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QLerpClipBlend lerpBlend;
        arbiter.setArbiterOnNode(&lerpBlend);
        auto endClip = new Qt3DAnimation::QLerpClipBlend();

        {
            // WHEN
            lerpBlend.setEndClip(endClip);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "endClip");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), lerpBlend.endClip()->id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            lerpBlend.setEndClip(endClip);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }
    }

    void checkStartClipBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DAnimation::QLerpClipBlend> lerpBlend(new Qt3DAnimation::QLerpClipBlend);
        {
            // WHEN
            Qt3DAnimation::QLerpClipBlend clip;
            lerpBlend->setStartClip(&clip);

            // THEN
            QCOMPARE(clip.parent(), lerpBlend.data());
            QCOMPARE(lerpBlend->startClip(), &clip);
        }
        // THEN (Should not crash and clip be unset)
        QVERIFY(lerpBlend->startClip() == nullptr);

        {
            // WHEN
            Qt3DAnimation::QLerpClipBlend someOtherLerpBlend;
            QScopedPointer<Qt3DAnimation::QLerpClipBlend> clip(new Qt3DAnimation::QLerpClipBlend(&someOtherLerpBlend));
            lerpBlend->setStartClip(clip.data());

            // THEN
            QCOMPARE(clip->parent(), &someOtherLerpBlend);
            QCOMPARE(lerpBlend->startClip(), clip.data());

            // WHEN
            lerpBlend.reset();
            clip.reset();

            // THEN Should not crash when the effect is destroyed (tests for failed removal of destruction helper)
        }
    }

    void checkEndClipBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DAnimation::QLerpClipBlend> lerpBlend(new Qt3DAnimation::QLerpClipBlend);
        {
            // WHEN
            Qt3DAnimation::QLerpClipBlend clip;
            lerpBlend->setEndClip(&clip);

            // THEN
            QCOMPARE(clip.parent(), lerpBlend.data());
            QCOMPARE(lerpBlend->endClip(), &clip);
        }
        // THEN (Should not crash and clip be unset)
        QVERIFY(lerpBlend->endClip() == nullptr);

        {
            // WHEN
            Qt3DAnimation::QLerpClipBlend someOtherLerpBlend;
            QScopedPointer<Qt3DAnimation::QLerpClipBlend> clip(new Qt3DAnimation::QLerpClipBlend(&someOtherLerpBlend));
            lerpBlend->setEndClip(clip.data());

            // THEN
            QCOMPARE(clip->parent(), &someOtherLerpBlend);
            QCOMPARE(lerpBlend->endClip(), clip.data());

            // WHEN
            lerpBlend.reset();
            clip.reset();

            // THEN Should not crash when the effect is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QLerpClipBlend)

#include "tst_qlerpclipblend.moc"
