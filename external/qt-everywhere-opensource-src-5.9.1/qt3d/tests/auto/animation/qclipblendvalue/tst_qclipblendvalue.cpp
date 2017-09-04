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
#include <Qt3DAnimation/qclipblendvalue.h>
#include <Qt3DAnimation/qanimationcliploader.h>
#include <Qt3DAnimation/private/qclipblendvalue_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DAnimation/qclipblendnodecreatedchange.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QClipBlendValue : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        qRegisterMetaType<Qt3DAnimation::QAbstractAnimationClip*>();
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DAnimation::QClipBlendValue clipBlendNode;

        // THEN;
        QCOMPARE(clipBlendNode.clip(), static_cast<Qt3DAnimation::QAbstractAnimationClip *>(nullptr));
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DAnimation::QClipBlendValue clipBlendNode;

        {
            // WHEN
            QSignalSpy spy(&clipBlendNode, SIGNAL(clipChanged(Qt3DAnimation::QAbstractAnimationClip*)));
            auto newValue = new Qt3DAnimation::QAnimationClipLoader();
            clipBlendNode.setClip(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(clipBlendNode.clip(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            clipBlendNode.setClip(newValue);

            // THEN
            QCOMPARE(clipBlendNode.clip(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DAnimation::QClipBlendValue clipBlendNode;
        Qt3DAnimation::QAnimationClipLoader clip;

        clipBlendNode.setClip(&clip);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&clipBlendNode);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 2); // 1 + 1 clip

            const auto creationChangeData = qSharedPointerCast<Qt3DAnimation::QClipBlendNodeCreatedChange<Qt3DAnimation::QClipBlendValueData>>(creationChanges.first());
            const Qt3DAnimation::QClipBlendValueData cloneData = creationChangeData->data;

            QCOMPARE(clipBlendNode.id(), creationChangeData->subjectId());
            QCOMPARE(clipBlendNode.isEnabled(), true);
            QCOMPARE(clipBlendNode.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(clipBlendNode.metaObject(), creationChangeData->metaObject());
            QCOMPARE(cloneData.clipId, clip.id());
        }

        // WHEN
        clipBlendNode.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&clipBlendNode);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 2); // 1 + 1 clip

            const auto creationChangeData = qSharedPointerCast<Qt3DAnimation::QClipBlendNodeCreatedChange<Qt3DAnimation::QClipBlendValueData>>(creationChanges.first());
            const Qt3DAnimation::QClipBlendValueData cloneData = creationChangeData->data;

            QCOMPARE(clipBlendNode.id(), creationChangeData->subjectId());
            QCOMPARE(clipBlendNode.isEnabled(), false);
            QCOMPARE(clipBlendNode.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(clipBlendNode.metaObject(), creationChangeData->metaObject());
            QCOMPARE(cloneData.clipId, clip.id());
        }
    }

    void checkClipUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QClipBlendValue clipBlendNode;
        arbiter.setArbiterOnNode(&clipBlendNode);
        auto clip = new Qt3DAnimation::QAnimationClipLoader();

        {
            // WHEN
            clipBlendNode.setClip(clip);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "clip");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), clipBlendNode.clip()->id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            clipBlendNode.setClip(clip);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }
    }

    void checkStartClipBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DAnimation::QClipBlendValue> clipBlendNode(new Qt3DAnimation::QClipBlendValue);
        {
            // WHEN
            Qt3DAnimation::QAnimationClipLoader clip;
            clipBlendNode->setClip(&clip);

            // THEN
            QCOMPARE(clip.parent(), clipBlendNode.data());
            QCOMPARE(clipBlendNode->clip(), &clip);
        }
        // THEN (Should not crash and clip be unset)
        QVERIFY(clipBlendNode->clip() == nullptr);

        {
            // WHEN
            Qt3DAnimation::QClipBlendValue someOtherNode;
            QScopedPointer<Qt3DAnimation::QAnimationClipLoader> clip(new Qt3DAnimation::QAnimationClipLoader(&someOtherNode));
            clipBlendNode->setClip(clip.data());

            // THEN
            QCOMPARE(clip->parent(), &someOtherNode);
            QCOMPARE(clipBlendNode->clip(), clip.data());

            // WHEN
            clipBlendNode.reset();
            clip.reset();

            // THEN Should not crash when the effect is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QClipBlendValue)

#include "tst_qclipblendvalue.moc"
