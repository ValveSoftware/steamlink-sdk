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
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DAnimation/qchannelmapper.h>
#include <Qt3DAnimation/private/qchannelmapper_p.h>
#include <Qt3DAnimation/qchannelmapping.h>

#include <Qt3DCore/QPropertyUpdatedChange>
#include <Qt3DCore/QPropertyNodeAddedChange>
#include <Qt3DCore/QPropertyNodeRemovedChange>

#include "testpostmanarbiter.h"

class tst_QChannelmapper : public Qt3DAnimation::QChannelMapper
{
    Q_OBJECT
public:
    tst_QChannelmapper()
    {
        qRegisterMetaType<Qt3DCore::QNode *>();
    }

private Q_SLOTS:
    void checkCreationChange_data()
    {
        QTest::addColumn<Qt3DAnimation::QChannelMapper *>("mapper");

        Qt3DAnimation::QChannelMapper *defaultConstructed = new Qt3DAnimation::QChannelMapper;
        QTest::newRow("defaultConstructed") << defaultConstructed;

        Qt3DAnimation::QChannelMapper *mapperWithMappings = new Qt3DAnimation::QChannelMapper;
        mapperWithMappings->addMapping(new Qt3DAnimation::QChannelMapping);
        mapperWithMappings->addMapping(new Qt3DAnimation::QChannelMapping);
        mapperWithMappings->addMapping(new Qt3DAnimation::QChannelMapping);
        QTest::newRow("mapperWithMappings") << mapperWithMappings;
    }

    void checkCreationChange()
    {
        // GIVEN
        QFETCH(Qt3DAnimation::QChannelMapper *, mapper);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(mapper);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        const int mappingCount = mapper->mappings().count();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + mappingCount);

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DAnimation::QChannelMapperData> creationChange =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DAnimation::QChannelMapperData>>(creationChanges.first());
        const Qt3DAnimation::QChannelMapperData &creationData = creationChange->data;

        // THEN
        QCOMPARE(mapper->id(), creationChange->subjectId());
        QCOMPARE(mapper->isEnabled(), creationChange->isNodeEnabled());
        QCOMPARE(mapper->metaObject(), creationChange->metaObject());
        QCOMPARE(mappingCount, creationData.mappingIds.count());

        for (int i = 0; i < mappingCount; ++i)
            QCOMPARE(mapper->mappings().at(i)->id(), creationData.mappingIds.at(i));
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DAnimation::QChannelMapper> mapper(new Qt3DAnimation::QChannelMapper);
        arbiter.setArbiterOnNode(mapper.data());

        // WHEN
        mapper->setEnabled(false);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr propertyChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(propertyChange->propertyName(), "enabled");
        QCOMPARE(propertyChange->value().toBool(), mapper->isEnabled());
        QCOMPARE(propertyChange->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }

    void checkMappingBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DAnimation::QChannelMapper> mapper(new Qt3DAnimation::QChannelMapper);
        {
            // WHEN
            Qt3DAnimation::QChannelMapping mapping;
            mapper->addMapping(&mapping);

            // THEN
            QCOMPARE(mapping.parent(), mapper.data());
            QCOMPARE(mapper->mappings().size(), 1);
        }
        // THEN (Should not crash and output be unset)
        QVERIFY(mapper->mappings().empty());

        {
            // WHEN
            Qt3DAnimation::QChannelMapper someOtherMapper;
            QScopedPointer<Qt3DAnimation::QChannelMapping> mapping(new Qt3DAnimation::QChannelMapping(&someOtherMapper));
            mapper->addMapping(mapping.data());

            // THEN
            QCOMPARE(mapping->parent(), &someOtherMapper);
            QCOMPARE(mapper->mappings().size(), 1);

            // WHEN
            mapper.reset();
            mapping.reset();

            // THEN Should not crash when the output is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QChannelmapper)

#include "tst_qchannelmapper.moc"
