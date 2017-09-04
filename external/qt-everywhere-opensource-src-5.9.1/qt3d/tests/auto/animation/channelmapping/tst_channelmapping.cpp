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
#include <qbackendnodetester.h>
#include <Qt3DAnimation/private/handler_p.h>
#include <Qt3DAnimation/private/channelmapping_p.h>
#include <Qt3DAnimation/qchannelmapping.h>
#include <Qt3DAnimation/private/qchannelmapping_p.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include "testpostmanarbiter.h"

class tst_ChannelMapping : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:
    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DAnimation::Animation::Handler handler;
        Qt3DAnimation::Animation::ChannelMapping backendMapping;
        backendMapping.setHandler(&handler);
        Qt3DAnimation::QChannelMapping mapping;
        auto target = new Qt3DCore::QEntity;

        mapping.setChannelName(QLatin1String("Location"));
        mapping.setTarget(target);
        mapping.setProperty(QLatin1String("foo"));

        // WHEN
        simulateInitialization(&mapping, &backendMapping);

        // THEN
        QCOMPARE(backendMapping.peerId(), mapping.id());
        QCOMPARE(backendMapping.isEnabled(), mapping.isEnabled());
        QCOMPARE(backendMapping.channelName(), mapping.channelName());
        QCOMPARE(backendMapping.targetId(), mapping.target()->id());
        QCOMPARE(backendMapping.property(), mapping.property());
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DAnimation::Animation::Handler handler;
        Qt3DAnimation::Animation::ChannelMapping backendMapping;
        backendMapping.setHandler(&handler);

        // THEN
        QVERIFY(backendMapping.peerId().isNull());
        QCOMPARE(backendMapping.isEnabled(), false);
        QCOMPARE(backendMapping.channelName(), QString());
        QCOMPARE(backendMapping.targetId(), Qt3DCore::QNodeId());
        QCOMPARE(backendMapping.property(), QString());

        // GIVEN
        Qt3DAnimation::QChannelMapping mapping;
        auto target = new Qt3DCore::QEntity;
        mapping.setChannelName(QLatin1String("Location"));
        mapping.setTarget(target);
        mapping.setProperty(QLatin1String("foo"));

        // WHEN
        simulateInitialization(&mapping, &backendMapping);
        backendMapping.cleanup();

        // THEN
        QCOMPARE(backendMapping.isEnabled(), false);
        QCOMPARE(backendMapping.channelName(), QString());
        QCOMPARE(backendMapping.targetId(), Qt3DCore::QNodeId());
        QCOMPARE(backendMapping.property(), QString());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DAnimation::Animation::Handler handler;
        Qt3DAnimation::Animation::ChannelMapping backendMapping;
        backendMapping.setHandler(&handler);
        Qt3DCore::QPropertyUpdatedChangePtr updateChange;

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("enabled");
        updateChange->setValue(true);
        backendMapping.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendMapping.isEnabled(), true);

        // WHEN
        const QString channelName(QLatin1String("Rotation"));
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("channelName");
        updateChange->setValue(channelName);
        backendMapping.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendMapping.channelName(), channelName);

        // WHEN
        const auto id = Qt3DCore::QNodeId::createId();
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("target");
        updateChange->setValue(QVariant::fromValue(id));
        backendMapping.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendMapping.targetId(), id);

        // WHEN
        const QString property(QLatin1String("bar"));
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("property");
        updateChange->setValue(property);
        backendMapping.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendMapping.property(), property);
    }
};

QTEST_MAIN(tst_ChannelMapping)

#include "tst_channelmapping.moc"
