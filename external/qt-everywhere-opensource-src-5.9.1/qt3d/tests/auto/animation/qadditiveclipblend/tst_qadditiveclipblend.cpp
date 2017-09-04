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
#include <Qt3DAnimation/qadditiveclipblend.h>
#include <Qt3DAnimation/qanimationcliploader.h>
#include <Qt3DAnimation/private/qadditiveclipblend_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DAnimation/qclipblendnodecreatedchange.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QAdditiveClipBlend : public QObject
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
        Qt3DAnimation::QAdditiveClipBlend addBlend;

        // THEN
        QCOMPARE(addBlend.additiveFactor(), 0.0f);
        QCOMPARE(addBlend.baseClip(), static_cast<Qt3DAnimation::QAbstractClipBlendNode *>(nullptr));
        QCOMPARE(addBlend.additiveClip(), static_cast<Qt3DAnimation::QAbstractClipBlendNode *>(nullptr));
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DAnimation::QAdditiveClipBlend addBlend;

        {
            // WHEN
            QSignalSpy spy(&addBlend, SIGNAL(additiveFactorChanged(float)));
            const float newValue = 0.5f;
            addBlend.setAdditiveFactor(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(addBlend.additiveFactor(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            addBlend.setAdditiveFactor(newValue);

            // THEN
            QCOMPARE(addBlend.additiveFactor(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&addBlend, SIGNAL(baseClipChanged(Qt3DAnimation::QAbstractClipBlendNode*)));
            auto newValue = new Qt3DAnimation::QAdditiveClipBlend();
            addBlend.setBaseClip(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(addBlend.baseClip(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            addBlend.setBaseClip(newValue);

            // THEN
            QCOMPARE(addBlend.baseClip(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&addBlend, SIGNAL(additiveClipChanged(Qt3DAnimation::QAbstractClipBlendNode*)));
            auto newValue = new Qt3DAnimation::QAdditiveClipBlend();
            addBlend.setAdditiveClip(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(addBlend.additiveClip(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            addBlend.setAdditiveClip(newValue);

            // THEN
            QCOMPARE(addBlend.additiveClip(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DAnimation::QAdditiveClipBlend addBlend;
        Qt3DAnimation::QAdditiveClipBlend baseClip;
        Qt3DAnimation::QAdditiveClipBlend additiveClip;

        addBlend.setBaseClip(&baseClip);
        addBlend.setAdditiveClip(&additiveClip);
        addBlend.setAdditiveFactor(0.8f);


        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&addBlend);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 3); // 1 + 2 clips

            const auto creationChangeData = qSharedPointerCast<Qt3DAnimation::QClipBlendNodeCreatedChange<Qt3DAnimation::QAdditiveClipBlendData>>(creationChanges.first());
            const Qt3DAnimation::QAdditiveClipBlendData cloneData = creationChangeData->data;

            QCOMPARE(addBlend.additiveFactor(), cloneData.additiveFactor);
            QCOMPARE(addBlend.id(), creationChangeData->subjectId());
            QCOMPARE(addBlend.isEnabled(), true);
            QCOMPARE(addBlend.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(addBlend.metaObject(), creationChangeData->metaObject());
            QCOMPARE(cloneData.baseClipId, baseClip.id());
            QCOMPARE(cloneData.additiveClipId, additiveClip.id());
        }

        // WHEN
        addBlend.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&addBlend);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 3); // 1 + 2 clips

            const auto creationChangeData = qSharedPointerCast<Qt3DAnimation::QClipBlendNodeCreatedChange<Qt3DAnimation::QAdditiveClipBlendData>>(creationChanges.first());
            const Qt3DAnimation::QAdditiveClipBlendData cloneData = creationChangeData->data;

            QCOMPARE(addBlend.additiveFactor(), cloneData.additiveFactor);
            QCOMPARE(addBlend.id(), creationChangeData->subjectId());
            QCOMPARE(addBlend.isEnabled(), false);
            QCOMPARE(addBlend.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(addBlend.metaObject(), creationChangeData->metaObject());
            QCOMPARE(cloneData.baseClipId, baseClip.id());
            QCOMPARE(cloneData.additiveClipId, additiveClip.id());
        }
    }

    void checkAdditiveFactorUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QAdditiveClipBlend addBlend;
        arbiter.setArbiterOnNode(&addBlend);

        {
            // WHEN
            addBlend.setAdditiveFactor(0.4f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "additiveFactor");
            QCOMPARE(change->value().value<float>(), addBlend.additiveFactor());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            addBlend.setAdditiveFactor(0.4f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkBaseClipUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QAdditiveClipBlend addBlend;
        arbiter.setArbiterOnNode(&addBlend);
        auto baseClip = new Qt3DAnimation::QAdditiveClipBlend();

        {
            // WHEN
            addBlend.setBaseClip(baseClip);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "baseClip");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), addBlend.baseClip()->id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            addBlend.setBaseClip(baseClip);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }
    }

    void checkAdditiveClipUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QAdditiveClipBlend addBlend;
        arbiter.setArbiterOnNode(&addBlend);
        auto additiveClip = new Qt3DAnimation::QAdditiveClipBlend();

        {
            // WHEN
            addBlend.setAdditiveClip(additiveClip);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "additiveClip");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), addBlend.additiveClip()->id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            addBlend.setAdditiveClip(additiveClip);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }
    }

    void checkBaseClipBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DAnimation::QAdditiveClipBlend> additiveBlend(new Qt3DAnimation::QAdditiveClipBlend);
        {
            // WHEN
            Qt3DAnimation::QAdditiveClipBlend clip;
            additiveBlend->setBaseClip(&clip);

            // THEN
            QCOMPARE(clip.parent(), additiveBlend.data());
            QCOMPARE(additiveBlend->baseClip(), &clip);
        }
        // THEN (Should not crash and clip be unset)
        QVERIFY(additiveBlend->baseClip() == nullptr);

        {
            // WHEN
            Qt3DAnimation::QAdditiveClipBlend someOtherAdditiveBlend;
            QScopedPointer<Qt3DAnimation::QAdditiveClipBlend> clip(new Qt3DAnimation::QAdditiveClipBlend(&someOtherAdditiveBlend));
            additiveBlend->setBaseClip(clip.data());

            // THEN
            QCOMPARE(clip->parent(), &someOtherAdditiveBlend);
            QCOMPARE(additiveBlend->baseClip(), clip.data());

            // WHEN
            additiveBlend.reset();
            clip.reset();

            // THEN Should not crash when the effect is destroyed (tests for failed removal of destruction helper)
        }
    }

    void checkAdditiveClipBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DAnimation::QAdditiveClipBlend> additiveBlend(new Qt3DAnimation::QAdditiveClipBlend);
        {
            // WHEN
            Qt3DAnimation::QAdditiveClipBlend clip;
            additiveBlend->setAdditiveClip(&clip);

            // THEN
            QCOMPARE(clip.parent(), additiveBlend.data());
            QCOMPARE(additiveBlend->additiveClip(), &clip);
        }
        // THEN (Should not crash and clip be unset)
        QVERIFY(additiveBlend->additiveClip() == nullptr);

        {
            // WHEN
            Qt3DAnimation::QAdditiveClipBlend someOtherAdditiveBlend;
            QScopedPointer<Qt3DAnimation::QAdditiveClipBlend> clip(new Qt3DAnimation::QAdditiveClipBlend(&someOtherAdditiveBlend));
            additiveBlend->setAdditiveClip(clip.data());

            // THEN
            QCOMPARE(clip->parent(), &someOtherAdditiveBlend);
            QCOMPARE(additiveBlend->additiveClip(), clip.data());

            // WHEN
            additiveBlend.reset();
            clip.reset();

            // THEN Should not crash when the effect is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QAdditiveClipBlend)

#include "tst_qadditiveclipblend.moc"
