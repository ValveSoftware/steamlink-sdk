/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qgraphicsapifilter.h>
#include <Qt3DRender/private/qgraphicsapifilter_p.h>
#include <Qt3DRender/private/qtechnique_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QTechnique : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QTechnique technique;

        // THEN
        QCOMPARE(technique.graphicsApiFilter()->profile(), Qt3DRender::QGraphicsApiFilter::NoProfile);
        QCOMPARE(technique.graphicsApiFilter()->majorVersion(), 0);
        QCOMPARE(technique.graphicsApiFilter()->minorVersion(), 0);
        QVERIFY(technique.renderPasses().empty());
        QVERIFY(technique.parameters().empty());
        QVERIFY(technique.filterKeys().empty());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QTechnique technique;

        {
            // WHEN
            Qt3DRender::QRenderPass pass;

            technique.addRenderPass(&pass);

            // THEN
            QCOMPARE(technique.renderPasses().size(), 1);

            // WHEN
            technique.addRenderPass(&pass);

            // THEN
            QCOMPARE(technique.renderPasses().size(), 1);

            // WHEN
            technique.removeRenderPass(&pass);

            // THEN
            QCOMPARE(technique.renderPasses().size(), 0);
        }
        {
            // WHEN
            Qt3DRender::QParameter parameter;

            technique.addParameter(&parameter);

            // THEN
            QCOMPARE(technique.parameters().size(), 1);

            // WHEN
            technique.addParameter(&parameter);

            // THEN
            QCOMPARE(technique.parameters().size(), 1);

            // WHEN
            technique.removeParameter(&parameter);

            // THEN
            QCOMPARE(technique.parameters().size(), 0);
        }
        {
            // WHEN
            Qt3DRender::QFilterKey filterKey;

            technique.addFilterKey(&filterKey);

            // THEN
            QCOMPARE(technique.filterKeys().size(), 1);

            // WHEN
            technique.addFilterKey(&filterKey);

            // THEN
            QCOMPARE(technique.filterKeys().size(), 1);

            // WHEN
            technique.removeFilterKey(&filterKey);

            // THEN
            QCOMPARE(technique.filterKeys().size(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(technique.graphicsApiFilter(), SIGNAL(graphicsApiFilterChanged()));
            technique.graphicsApiFilter()->setMajorVersion(3);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(technique.graphicsApiFilter()->majorVersion(), 3);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            technique.graphicsApiFilter()->setMajorVersion(3);

            // THEN
            QCOMPARE(technique.graphicsApiFilter()->majorVersion(), 3);
            QCOMPARE(spy.count(), 0);

            // WHEN
            technique.graphicsApiFilter()->setMinorVersion(2);

            // THEN
            QCOMPARE(technique.graphicsApiFilter()->minorVersion(), 2);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            technique.graphicsApiFilter()->setMinorVersion(2);

            // THEN
            QCOMPARE(technique.graphicsApiFilter()->minorVersion(), 2);
            QCOMPARE(spy.count(), 0);

            // WHEN
            technique.graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);

            // THEN
            QCOMPARE(technique.graphicsApiFilter()->profile(), Qt3DRender::QGraphicsApiFilter::CoreProfile);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            technique.graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);

            // THEN
            QCOMPARE(technique.graphicsApiFilter()->profile(), Qt3DRender::QGraphicsApiFilter::CoreProfile);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkRenderPassBookkeeping()
    {
        // GIVEN
        Qt3DRender::QTechnique technique;

        {
            // WHEN
            Qt3DRender::QRenderPass pass;
            technique.addRenderPass(&pass);

            // THEN
            QCOMPARE(technique.renderPasses().size(), 1);
        }

        // THEN -> should not crash
        QCOMPARE(technique.renderPasses().size(), 0);
    }

    void checkParameterBookkeeping()
    {
        // GIVEN
        Qt3DRender::QTechnique technique;

        {
            // WHEN
            Qt3DRender::QParameter parameter;
            technique.addParameter(&parameter);

            // THEN
            QCOMPARE(technique.parameters().size(), 1);
        }

        // THEN -> should not crash
        QCOMPARE(technique.parameters().size(), 0);
    }

    void checkFilterKeyBookkeeping()
    {
        // GIVEN
        Qt3DRender::QTechnique technique;

        {
            // WHEN
            Qt3DRender::QFilterKey filterKey;
            technique.addFilterKey(&filterKey);

            // THEN
            QCOMPARE(technique.filterKeys().size(), 1);
        }

        // THEN -> should not crash
        QCOMPARE(technique.filterKeys().size(), 0);
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QTechnique technique;
        Qt3DRender::QRenderPass pass;
        Qt3DRender::QParameter parameter;
        Qt3DRender::QFilterKey filterKey;

        technique.addRenderPass(&pass);
        technique.addParameter(&parameter);
        technique.addFilterKey(&filterKey);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&technique);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 4); // Technique + Pass + Parameter + FilterKey

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QTechniqueData>>(creationChanges.first());
            const Qt3DRender::QTechniqueData cloneData = creationChangeData->data;

            QCOMPARE(technique.graphicsApiFilter()->minorVersion(), cloneData.graphicsApiFilterData.m_minor);
            QCOMPARE(technique.graphicsApiFilter()->majorVersion(), cloneData.graphicsApiFilterData.m_major);
            QCOMPARE(technique.graphicsApiFilter()->profile(), cloneData.graphicsApiFilterData.m_profile);
            QCOMPARE(technique.graphicsApiFilter()->vendor(), cloneData.graphicsApiFilterData.m_vendor);
            QCOMPARE(technique.graphicsApiFilter()->extensions(), cloneData.graphicsApiFilterData.m_extensions);
            QCOMPARE(technique.graphicsApiFilter()->api(), cloneData.graphicsApiFilterData.m_api);
            QCOMPARE(technique.id(), creationChangeData->subjectId());
            QCOMPARE(technique.isEnabled(), true);
            QCOMPARE(technique.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(technique.metaObject(), creationChangeData->metaObject());
            QCOMPARE(technique.renderPasses().size(), cloneData.renderPassIds.size());
            QCOMPARE(technique.parameters().size(), cloneData.parameterIds.size());
            QCOMPARE(technique.filterKeys().size(), cloneData.filterKeyIds.size());
            QCOMPARE(pass.id(), cloneData.renderPassIds.first());
            QCOMPARE(parameter.id(), cloneData.parameterIds.first());
            QCOMPARE(filterKey.id(), cloneData.filterKeyIds.first());
        }

        // WHEN
        technique.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&technique);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 4); // Technique + Pass + Parameter + FilterKey

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QTechniqueData>>(creationChanges.first());
            const Qt3DRender::QTechniqueData cloneData = creationChangeData->data;

            QCOMPARE(technique.graphicsApiFilter()->minorVersion(), cloneData.graphicsApiFilterData.m_minor);
            QCOMPARE(technique.graphicsApiFilter()->majorVersion(), cloneData.graphicsApiFilterData.m_major);
            QCOMPARE(technique.graphicsApiFilter()->profile(), cloneData.graphicsApiFilterData.m_profile);
            QCOMPARE(technique.graphicsApiFilter()->vendor(), cloneData.graphicsApiFilterData.m_vendor);
            QCOMPARE(technique.graphicsApiFilter()->extensions(), cloneData.graphicsApiFilterData.m_extensions);
            QCOMPARE(technique.graphicsApiFilter()->api(), cloneData.graphicsApiFilterData.m_api);
            QCOMPARE(technique.id(), creationChangeData->subjectId());
            QCOMPARE(technique.isEnabled(), false);
            QCOMPARE(technique.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(technique.metaObject(), creationChangeData->metaObject());
            QCOMPARE(technique.renderPasses().size(), cloneData.renderPassIds.size());
            QCOMPARE(technique.parameters().size(), cloneData.parameterIds.size());
            QCOMPARE(technique.filterKeys().size(), cloneData.filterKeyIds.size());
            QCOMPARE(pass.id(), cloneData.renderPassIds.first());
            QCOMPARE(parameter.id(), cloneData.parameterIds.first());
            QCOMPARE(filterKey.id(), cloneData.filterKeyIds.first());
        }
    }

    void checkRenderPassUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QTechnique technique;
        Qt3DRender::QRenderPass pass;
        arbiter.setArbiterOnNode(&technique);

        {
            // WHEN
            technique.addRenderPass(&pass);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
            QCOMPARE(change->propertyName(), "pass");
            QCOMPARE(change->addedNodeId(), pass.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

            arbiter.events.clear();
        }

        {
            // WHEN
            technique.removeRenderPass(&pass);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
            QCOMPARE(change->propertyName(), "pass");
            QCOMPARE(change->removedNodeId(), pass.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyValueRemoved);

            arbiter.events.clear();
        }
    }

    void checkParameterUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QTechnique technique;
        Qt3DRender::QParameter parameter;
        arbiter.setArbiterOnNode(&technique);

        {
            // WHEN
            technique.addParameter(&parameter);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
            QCOMPARE(change->propertyName(), "parameter");
            QCOMPARE(change->addedNodeId(), parameter.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

            arbiter.events.clear();
        }

        {
            // WHEN
            technique.removeParameter(&parameter);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
            QCOMPARE(change->propertyName(), "parameter");
            QCOMPARE(change->removedNodeId(), parameter.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyValueRemoved);

            arbiter.events.clear();
        }
    }

    void checkFilterKeyUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QTechnique technique;
        Qt3DRender::QFilterKey filterKey;
        arbiter.setArbiterOnNode(&technique);

        {
            // WHEN
            technique.addFilterKey(&filterKey);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
            QCOMPARE(change->propertyName(), "filterKeys");
            QCOMPARE(change->addedNodeId(), filterKey.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

            arbiter.events.clear();
        }

        {
            // WHEN
            technique.removeFilterKey(&filterKey);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
            QCOMPARE(change->propertyName(), "filterKeys");
            QCOMPARE(change->removedNodeId(), filterKey.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyValueRemoved);

            arbiter.events.clear();
        }
    }

    void checkGraphicsAPIFilterUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QTechnique technique;
        arbiter.setArbiterOnNode(&technique);

        {
            // WHEN
            technique.graphicsApiFilter()->setMajorVersion(4);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "graphicsApiFilterData");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);
            Qt3DRender::GraphicsApiFilterData data = change->value().value<Qt3DRender::GraphicsApiFilterData>();
            QCOMPARE(data.m_major, 4);

            arbiter.events.clear();
        }
        {
            // WHEN
            technique.graphicsApiFilter()->setMinorVersion(5);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "graphicsApiFilterData");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);
            Qt3DRender::GraphicsApiFilterData data = change->value().value<Qt3DRender::GraphicsApiFilterData>();
            QCOMPARE(data.m_minor, 5);

            arbiter.events.clear();
        }
        {
            // WHEN
            technique.graphicsApiFilter()->setVendor(QStringLiteral("AMD"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "graphicsApiFilterData");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);
            Qt3DRender::GraphicsApiFilterData data = change->value().value<Qt3DRender::GraphicsApiFilterData>();
            QCOMPARE(data.m_vendor, QStringLiteral("AMD"));

            arbiter.events.clear();
        }
        {
            // WHEN
            technique.graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CompatibilityProfile);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "graphicsApiFilterData");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);
            Qt3DRender::GraphicsApiFilterData data = change->value().value<Qt3DRender::GraphicsApiFilterData>();
            QCOMPARE(data.m_profile, Qt3DRender::QGraphicsApiFilter::CompatibilityProfile);

            arbiter.events.clear();
        }
    }
};

QTEST_MAIN(tst_QTechnique)

#include "tst_qtechnique.moc"
