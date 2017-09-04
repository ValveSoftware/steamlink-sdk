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
#include <Qt3DAnimation/private/animationclip_p.h>
#include <Qt3DAnimation/qanimationcliploader.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include <Qt3DCore/private/qpropertyupdatedchangebase_p.h>
#include <qbackendnodetester.h>
#include <testpostmanarbiter.h>

using namespace Qt3DAnimation::Animation;

class tst_AnimationClip : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:
    void checkPeerPropertyMirroring()
    {
        // GIVEN
        AnimationClip backendClip;
        Handler handler;
        backendClip.setHandler(&handler);
        Qt3DAnimation::QAnimationClipLoader clip;

        clip.setSource(QUrl::fromLocalFile("walk.qlip"));

        // WHEN
        simulateInitialization(&clip, &backendClip);

        // THEN
        QCOMPARE(backendClip.peerId(), clip.id());
        QCOMPARE(backendClip.isEnabled(), clip.isEnabled());
        QCOMPARE(backendClip.source(), clip.source());
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        AnimationClip backendClip;
        Handler handler;
        backendClip.setHandler(&handler);

        // THEN
        QVERIFY(backendClip.peerId().isNull());
        QCOMPARE(backendClip.isEnabled(), false);
        QCOMPARE(backendClip.source(), QUrl());
        QCOMPARE(backendClip.duration(), 0.0f);
        QCOMPARE(backendClip.status(), Qt3DAnimation::QAnimationClipLoader::NotReady);

        // GIVEN
        Qt3DAnimation::QAnimationClipLoader clip;
        clip.setSource(QUrl::fromLocalFile("walk.qlip"));

        // WHEN
        simulateInitialization(&clip, &backendClip);
        backendClip.setSource(QUrl::fromLocalFile("run.qlip"));
        backendClip.cleanup();

        // THEN
        QCOMPARE(backendClip.source(), QUrl());
        QCOMPARE(backendClip.isEnabled(), false);
        QCOMPARE(backendClip.duration(), 0.0f);
        QCOMPARE(backendClip.status(), Qt3DAnimation::QAnimationClipLoader::NotReady);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        AnimationClip backendClip;
        Handler handler;
        backendClip.setHandler(&handler);
        backendClip.setDataType(Qt3DAnimation::Animation::AnimationClip::File);
        Qt3DCore::QPropertyUpdatedChangePtr updateChange;

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("enabled");
        updateChange->setValue(true);
        backendClip.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendClip.isEnabled(), true);

        // WHEN
        const QUrl newSource = QUrl::fromLocalFile("fallover.qlip");
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("source");
        updateChange->setValue(newSource);
        backendClip.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendClip.source(), newSource);
    }

    void checkDurationPropertyBackendNotification()
    {
        // GIVEN
        TestArbiter arbiter;
        AnimationClip backendClip;
        backendClip.setEnabled(true);
        Qt3DCore::QBackendNodePrivate::get(&backendClip)->setArbiter(&arbiter);

        // WHEN
        backendClip.setDuration(64.0f);

        // THEN
        QCOMPARE(backendClip.duration(), 64.0f);
        QCOMPARE(arbiter.events.count(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "duration");
        QCOMPARE(change->value().toFloat(), backendClip.duration());
        QCOMPARE(Qt3DCore::QPropertyUpdatedChangeBasePrivate::get(change.data())->m_isIntermediate,
                 false);

        arbiter.events.clear();

        // WHEN
        backendClip.setDuration(64.0f);

        // THEN
        QCOMPARE(backendClip.duration(), 64.0f);
        QCOMPARE(arbiter.events.count(), 0);

        arbiter.events.clear();
    }

    void checkStatusPropertyBackendNotification()
    {
        // GIVEN
        TestArbiter arbiter;
        AnimationClip backendClip;
        backendClip.setEnabled(true);
        Qt3DCore::QBackendNodePrivate::get(&backendClip)->setArbiter(&arbiter);

        // WHEN
        backendClip.setStatus(Qt3DAnimation::QAnimationClipLoader::Error);

        // THEN
        QCOMPARE(backendClip.status(), Qt3DAnimation::QAnimationClipLoader::Error);
        QCOMPARE(arbiter.events.count(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "status");
        QCOMPARE(change->value().value<Qt3DAnimation::QAnimationClipLoader::Status>(), backendClip.status());
        QCOMPARE(Qt3DCore::QPropertyUpdatedChangeBasePrivate::get(change.data())->m_isIntermediate,
                 false);

        arbiter.events.clear();

        // WHEN
        backendClip.setStatus(Qt3DAnimation::QAnimationClipLoader::Error);

        // THEN
        QCOMPARE(backendClip.status(), Qt3DAnimation::QAnimationClipLoader::Error);
        QCOMPARE(arbiter.events.count(), 0);

        arbiter.events.clear();
    }
};

QTEST_APPLESS_MAIN(tst_AnimationClip)

#include "tst_animationclip.moc"
