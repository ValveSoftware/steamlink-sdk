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
#include <Qt3DAnimation/private/blendedclipanimator_p.h>
#include <Qt3DAnimation/qanimationcliploader.h>
#include <Qt3DAnimation/qblendedclipanimator.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include <qbackendnodetester.h>
#include <testpostmanarbiter.h>

#include <QtTest/QTest>
#include <Qt3DAnimation/qblendedclipanimator.h>
#include <Qt3DAnimation/qlerpclipblend.h>
#include <Qt3DAnimation/qchannelmapper.h>
#include <Qt3DAnimation/private/qblendedclipanimator_p.h>
#include <Qt3DAnimation/private/blendedclipanimator_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "qbackendnodetester.h"

class tst_BlendedClipAnimator : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DAnimation::Animation::BlendedClipAnimator backendBlendedClipAnimator;

        // THEN
        QCOMPARE(backendBlendedClipAnimator.isEnabled(), false);
        QVERIFY(backendBlendedClipAnimator.peerId().isNull());
        QCOMPARE(backendBlendedClipAnimator.blendTreeRootId(), Qt3DCore::QNodeId());
        QCOMPARE(backendBlendedClipAnimator.mapperId(), Qt3DCore::QNodeId());
        QCOMPARE(backendBlendedClipAnimator.isRunning(), false);
        QCOMPARE(backendBlendedClipAnimator.startTime(), 0);
        QCOMPARE(backendBlendedClipAnimator.currentLoop(), 0);
        QCOMPARE(backendBlendedClipAnimator.loops(), 1);

    }

    void checkCleanupState()
    {
        // GIVEN
        Qt3DAnimation::Animation::BlendedClipAnimator backendBlendedClipAnimator;
        Qt3DAnimation::Animation::Handler handler;
        backendBlendedClipAnimator.setHandler(&handler);

        // WHEN
        backendBlendedClipAnimator.setEnabled(true);
        backendBlendedClipAnimator.setBlendTreeRootId(Qt3DCore::QNodeId::createId());
        backendBlendedClipAnimator.setMapperId(Qt3DCore::QNodeId::createId());
        backendBlendedClipAnimator.setRunning(true);
        backendBlendedClipAnimator.setStartTime(28);
        backendBlendedClipAnimator.cleanup();

        // THEN
        QCOMPARE(backendBlendedClipAnimator.isEnabled(), false);
        QCOMPARE(backendBlendedClipAnimator.blendTreeRootId(), Qt3DCore::QNodeId());
        QCOMPARE(backendBlendedClipAnimator.mapperId(), Qt3DCore::QNodeId());
        QCOMPARE(backendBlendedClipAnimator.isRunning(), false);
        QCOMPARE(backendBlendedClipAnimator.startTime(), 0);
        QCOMPARE(backendBlendedClipAnimator.currentLoop(), 0);
        QCOMPARE(backendBlendedClipAnimator.loops(), 1);
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DAnimation::QBlendedClipAnimator blendedClipAnimator;
        Qt3DAnimation::QChannelMapper mapper;
        Qt3DAnimation::QLerpClipBlend blendTree;
        blendedClipAnimator.setRunning(true);
        blendedClipAnimator.setBlendTree(&blendTree);
        blendedClipAnimator.setChannelMapper(&mapper);
        blendedClipAnimator.setLoopCount(10);

        {
            // WHEN
            Qt3DAnimation::Animation::BlendedClipAnimator backendBlendedClipAnimator;
            Qt3DAnimation::Animation::Handler handler;
            backendBlendedClipAnimator.setHandler(&handler);

            simulateInitialization(&blendedClipAnimator, &backendBlendedClipAnimator);

            // THEN
            QCOMPARE(backendBlendedClipAnimator.isEnabled(), true);
            QCOMPARE(backendBlendedClipAnimator.peerId(), blendedClipAnimator.id());
            QCOMPARE(backendBlendedClipAnimator.blendTreeRootId(), blendTree.id());
            QCOMPARE(backendBlendedClipAnimator.mapperId(), mapper.id());
            QCOMPARE(backendBlendedClipAnimator.isRunning(), true);
            QCOMPARE(backendBlendedClipAnimator.loops(), 10);
        }
        {
            // WHEN
            Qt3DAnimation::Animation::BlendedClipAnimator backendBlendedClipAnimator;
            Qt3DAnimation::Animation::Handler handler;
            backendBlendedClipAnimator.setHandler(&handler);

            blendedClipAnimator.setEnabled(false);
            simulateInitialization(&blendedClipAnimator, &backendBlendedClipAnimator);

            // THEN
            QCOMPARE(backendBlendedClipAnimator.peerId(), blendedClipAnimator.id());
            QCOMPARE(backendBlendedClipAnimator.isEnabled(), false);
        }
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        Qt3DAnimation::Animation::BlendedClipAnimator backendBlendedClipAnimator;
        Qt3DAnimation::Animation::Handler handler;
        backendBlendedClipAnimator.setHandler(&handler);

        {
             // WHEN
             const bool newValue = false;
             const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
             change->setPropertyName("enabled");
             change->setValue(newValue);
             backendBlendedClipAnimator.sceneChangeEvent(change);

             // THEN
            QCOMPARE(backendBlendedClipAnimator.isEnabled(), newValue);
        }
        {
             // WHEN
             const Qt3DCore::QNodeId newValue = Qt3DCore::QNodeId::createId();
             const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
             change->setPropertyName("blendTree");
             change->setValue(QVariant::fromValue(newValue));
             backendBlendedClipAnimator.sceneChangeEvent(change);

             // THEN
            QCOMPARE(backendBlendedClipAnimator.blendTreeRootId(), newValue);
        }
        {
             // WHEN
             const Qt3DCore::QNodeId newValue = Qt3DCore::QNodeId::createId();
             const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
             change->setPropertyName("channelMapper");
             change->setValue(QVariant::fromValue(newValue));
             backendBlendedClipAnimator.sceneChangeEvent(change);

             // THEN
            QCOMPARE(backendBlendedClipAnimator.mapperId(), newValue);
        }
        {
             // WHEN
             const bool newValue = true;
             const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
             change->setPropertyName("running");
             change->setValue(QVariant::fromValue(newValue));
             backendBlendedClipAnimator.sceneChangeEvent(change);

             // THEN
            QCOMPARE(backendBlendedClipAnimator.isRunning(), newValue);
        }
        {
            // WHEN
            const int newValue = 883;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("loops");
            change->setValue(QVariant::fromValue(newValue));
            backendBlendedClipAnimator.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendBlendedClipAnimator.loops(), newValue);
        }
    }

};

QTEST_APPLESS_MAIN(tst_BlendedClipAnimator)

#include "tst_blendedclipanimator.moc"
