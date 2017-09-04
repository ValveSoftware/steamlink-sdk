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
#include <Qt3DAnimation/private/channelmapper_p.h>
#include <Qt3DAnimation/qchannelmapper.h>
#include <Qt3DAnimation/qchannelmapping.h>
#include <Qt3DAnimation/private/qchannelmapper_p.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include "testpostmanarbiter.h"

class tst_ChannelMapper : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:
    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DAnimation::Animation::Handler handler;
        Qt3DAnimation::Animation::ChannelMapper backendMapper;
        backendMapper.setHandler(&handler);
        Qt3DAnimation::QChannelMapper mapper;

        mapper.addMapping(new Qt3DAnimation::QChannelMapping);
        mapper.addMapping(new Qt3DAnimation::QChannelMapping);

        // WHEN
        simulateInitialization(&mapper, &backendMapper);

        // THEN
        QCOMPARE(backendMapper.peerId(), mapper.id());
        QCOMPARE(backendMapper.isEnabled(), mapper.isEnabled());

        const int mappingCount = backendMapper.mappingIds().size();
        if (mappingCount > 0) {
            for (int i = 0; i < mappingCount; ++i)
                QCOMPARE(backendMapper.mappingIds().at(i), mapper.mappings().at(i)->id());
        }
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DAnimation::Animation::Handler handler;
        Qt3DAnimation::Animation::ChannelMapper backendMapper;
        backendMapper.setHandler(&handler);

        // THEN
        QVERIFY(backendMapper.peerId().isNull());
        QCOMPARE(backendMapper.isEnabled(), false);
        QCOMPARE(backendMapper.mappingIds(), QVector<Qt3DCore::QNodeId>());

        // GIVEN
        Qt3DAnimation::QChannelMapper mapper;
        mapper.addMapping(new Qt3DAnimation::QChannelMapping());

        // WHEN
        simulateInitialization(&mapper, &backendMapper);
        backendMapper.cleanup();

        // THEN
        QCOMPARE(backendMapper.isEnabled(), false);
        QCOMPARE(backendMapper.mappingIds(), QVector<Qt3DCore::QNodeId>());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DAnimation::Animation::Handler handler;
        Qt3DAnimation::Animation::ChannelMapper backendMapper;
        backendMapper.setHandler(&handler);
        Qt3DCore::QPropertyUpdatedChangePtr updateChange;

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("enabled");
        updateChange->setValue(true);
        backendMapper.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendMapper.isEnabled(), true);

        // WHEN
        Qt3DAnimation::QChannelMapping mapping;
        const Qt3DCore::QNodeId mappingId = mapping.id();
        const auto nodeAddedChange = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &mapping);
        nodeAddedChange->setPropertyName("mappings");
        backendMapper.sceneChangeEvent(nodeAddedChange);

        // THEN
        QCOMPARE(backendMapper.mappingIds().size(), 1);
        QCOMPARE(backendMapper.mappingIds().first(), mappingId);

        // WHEN
        const auto nodeRemovedChange = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &mapping);
        nodeRemovedChange->setPropertyName("mappings");
        backendMapper.sceneChangeEvent(nodeRemovedChange);

        // THEN
        QCOMPARE(backendMapper.mappingIds().size(), 0);
    }
};

QTEST_MAIN(tst_ChannelMapper)

#include "tst_channelmapper.moc"
