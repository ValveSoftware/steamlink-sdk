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
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qdepthtest.h>
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/private/qrenderpass_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QRenderPass : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DRender::QShaderProgram *>("QShaderProgram *");
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QRenderPass renderPass;

        // THEN
        QVERIFY(renderPass.shaderProgram() == nullptr);
        QCOMPARE(renderPass.filterKeys().size(), 0);
        QCOMPARE(renderPass.renderStates().size(), 0);
        QCOMPARE(renderPass.parameters().size(), 0);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QRenderPass renderPass;

        {
            // WHEN
            QSignalSpy spy(&renderPass, SIGNAL(shaderProgramChanged(QShaderProgram *)));
            Qt3DRender::QShaderProgram newValue;
            renderPass.setShaderProgram(&newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderPass.shaderProgram(), &newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderPass.setShaderProgram(&newValue);

            // THEN
            QCOMPARE(renderPass.shaderProgram(), &newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            Qt3DRender::QFilterKey newValue;
            renderPass.addFilterKey(&newValue);

            // THEN
            QCOMPARE(renderPass.filterKeys().size(), 1);

            // WHEN
            renderPass.addFilterKey(&newValue);

            // THEN
            QCOMPARE(renderPass.filterKeys().size(), 1);

            // WHEN
            renderPass.removeFilterKey(&newValue);

            // THEN
            QCOMPARE(renderPass.filterKeys().size(), 0);
        }
        {
            // WHEN
            Qt3DRender::QDepthTest newValue;
            renderPass.addRenderState(&newValue);

            // THEN
            QCOMPARE(renderPass.renderStates().size(), 1);

            // WHEN
            renderPass.addRenderState(&newValue);

            // THEN
            QCOMPARE(renderPass.renderStates().size(), 1);

            // WHEN
            renderPass.removeRenderState(&newValue);

            // THEN
            QCOMPARE(renderPass.renderStates().size(), 0);
        }
        {
            // WHEN
            Qt3DRender::QParameter newValue;
            renderPass.addParameter(&newValue);

            // THEN
            QCOMPARE(renderPass.parameters().size(), 1);

            // WHEN
            renderPass.addParameter(&newValue);

            // THEN
            QCOMPARE(renderPass.parameters().size(), 1);

            // WHEN
            renderPass.removeParameter(&newValue);

            // THEN
            QCOMPARE(renderPass.parameters().size(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QRenderPass renderPass;

        Qt3DRender::QShaderProgram shader;
        renderPass.setShaderProgram(&shader);
        Qt3DRender::QFilterKey filterKey;
        renderPass.addFilterKey(&filterKey);
        Qt3DRender::QDepthTest renderState;
        renderPass.addRenderState(&renderState);
        Qt3DRender::QParameter parameter;
        renderPass.addParameter(&parameter);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&renderPass);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 5); // RenderPass + Shader + FilterKey + Parameter + State

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QRenderPassData>>(creationChanges.first());
            const Qt3DRender::QRenderPassData cloneData = creationChangeData->data;

            QCOMPARE(renderPass.shaderProgram()->id(), cloneData.shaderId);
            QCOMPARE(cloneData.filterKeyIds.size(), 1);
            QCOMPARE(filterKey.id(), cloneData.filterKeyIds.first());
            QCOMPARE(cloneData.renderStateIds.size(), 1);
            QCOMPARE(renderState.id(), cloneData.renderStateIds.first());
            QCOMPARE(cloneData.parameterIds.size(), 1);
            QCOMPARE(parameter.id(), cloneData.parameterIds.first());
            QCOMPARE(renderPass.id(), creationChangeData->subjectId());
            QCOMPARE(renderPass.isEnabled(), true);
            QCOMPARE(renderPass.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(renderPass.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        renderPass.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&renderPass);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 5); // RenderPass + Shader + FilterKey + Parameter + State

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QRenderPassData>>(creationChanges.first());
            const Qt3DRender::QRenderPassData cloneData = creationChangeData->data;

            QCOMPARE(renderPass.shaderProgram()->id(), cloneData.shaderId);
            QCOMPARE(cloneData.filterKeyIds.size(), 1);
            QCOMPARE(filterKey.id(), cloneData.filterKeyIds.first());
            QCOMPARE(cloneData.renderStateIds.size(), 1);
            QCOMPARE(renderState.id(), cloneData.renderStateIds.first());
            QCOMPARE(cloneData.parameterIds.size(), 1);
            QCOMPARE(parameter.id(), cloneData.parameterIds.first());
            QCOMPARE(renderPass.id(), creationChangeData->subjectId());
            QCOMPARE(renderPass.isEnabled(), false);
            QCOMPARE(renderPass.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(renderPass.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkShaderProgramBookkeeping()
    {
        // GIVEN
        Qt3DRender::QRenderPass renderPass;

        {
            // WHEN
            Qt3DRender::QShaderProgram shaderProgram;
            renderPass.setShaderProgram(&shaderProgram);

            QCOMPARE(renderPass.shaderProgram(), &shaderProgram);
        }

        // THEN -> should not crash
        QVERIFY(renderPass.shaderProgram() == nullptr);
    }

    void checkFilterKeyBookkeeping()
    {
        // GIVEN
        Qt3DRender::QRenderPass renderPass;

        {
            // WHEN
            Qt3DRender::QFilterKey filterKey;
            renderPass.addFilterKey(&filterKey);

            QCOMPARE(renderPass.filterKeys().size(), 1);
        }

        // THEN -> should not crash
        QCOMPARE(renderPass.filterKeys().size(), 0);
    }

    void checkRenderStateBookkeeping()
    {
        // GIVEN
        Qt3DRender::QRenderPass renderPass;

        {
            // WHEN
            Qt3DRender::QDepthTest renderState;
            renderPass.addRenderState(&renderState);

            QCOMPARE(renderPass.renderStates().size(), 1);
        }

        // THEN -> should not crash
        QCOMPARE(renderPass.renderStates().size(), 0);
    }

    void checkParameterBookkeeping()
    {
        // GIVEN
        Qt3DRender::QRenderPass renderPass;

        {
            // WHEN
            Qt3DRender::QParameter parameter;
            renderPass.addParameter(&parameter);

            QCOMPARE(renderPass.parameters().size(), 1);
        }

        // THEN -> should not crash
        QCOMPARE(renderPass.parameters().size(), 0);
    }

    void checkShaderProgramUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderPass renderPass;
        Qt3DRender::QShaderProgram shaderProgram;
        arbiter.setArbiterOnNode(&renderPass);

        {
            // WHEN
            renderPass.setShaderProgram(&shaderProgram);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "shaderProgram");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), renderPass.shaderProgram()->id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderPass.setShaderProgram(&shaderProgram);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkFilterKeyUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderPass renderPass;
        Qt3DRender::QFilterKey filterKey;
        arbiter.setArbiterOnNode(&renderPass);

        {
            // WHEN
            renderPass.addFilterKey(&filterKey);
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
            renderPass.removeFilterKey(&filterKey);
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

    void checkRenderStateUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderPass renderPass;
        Qt3DRender::QDepthTest renderState;
        arbiter.setArbiterOnNode(&renderPass);

        {
            // WHEN
            renderPass.addRenderState(&renderState);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
            QCOMPARE(change->propertyName(), "renderState");
            QCOMPARE(change->addedNodeId(), renderState.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderPass.removeRenderState(&renderState);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
            QCOMPARE(change->propertyName(), "renderState");
            QCOMPARE(change->removedNodeId(), renderState.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyValueRemoved);

            arbiter.events.clear();
        }

    }

    void checkParameterUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderPass renderPass;
        Qt3DRender::QParameter parameter;
        arbiter.setArbiterOnNode(&renderPass);

        {
            // WHEN
            renderPass.addParameter(&parameter);
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
            renderPass.removeParameter(&parameter);
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

};

QTEST_MAIN(tst_QRenderPass)

#include "tst_qrenderpass.moc"
