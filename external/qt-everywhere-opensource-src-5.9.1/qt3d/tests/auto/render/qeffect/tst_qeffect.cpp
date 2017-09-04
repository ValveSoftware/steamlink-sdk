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
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/private/qeffect_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QEffect : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QEffect effect;

        // THEN
        QCOMPARE(effect.parameters().size(), 0);
        QCOMPARE(effect.techniques().size(), 0);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QEffect effect;

        {
            // WHEN
            Qt3DRender::QParameter newValue;
            effect.addParameter(&newValue);

            // THEN
            QCOMPARE(effect.parameters().size(), 1);

            // WHEN
            effect.addParameter(&newValue);

            // THEN
            QCOMPARE(effect.parameters().size(), 1);

            // WHEN
            effect.removeParameter(&newValue);

            // THEN
            QCOMPARE(effect.parameters().size(), 0);
        }
        {
            // WHEN
            Qt3DRender::QTechnique newValue;
            effect.addTechnique(&newValue);

            // THEN
            QCOMPARE(effect.techniques().size(), 1);

            // WHEN
            effect.addTechnique(&newValue);

            // THEN
            QCOMPARE(effect.techniques().size(), 1);

            // WHEN
            effect.removeTechnique(&newValue);

            // THEN
            QCOMPARE(effect.techniques().size(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QEffect effect;

        Qt3DRender::QParameter parameter;
        effect.addParameter(&parameter);
        Qt3DRender::QTechnique technique;
        effect.addTechnique(&technique);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&effect);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 3); // Effect + Parameter + Technique

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QEffectData>>(creationChanges.first());
            const Qt3DRender::QEffectData cloneData = creationChangeData->data;

            QCOMPARE(cloneData.parameterIds.size(), 1);
            QCOMPARE(parameter.id(), cloneData.parameterIds.first());
            QCOMPARE(cloneData.techniqueIds.size(), 1);
            QCOMPARE(technique.id(), cloneData.techniqueIds.first());
            QCOMPARE(effect.id(), creationChangeData->subjectId());
            QCOMPARE(effect.isEnabled(), true);
            QCOMPARE(effect.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(effect.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        effect.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&effect);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 3); // Effect + Parameter + Technique

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QEffectData>>(creationChanges.first());
            const Qt3DRender::QEffectData cloneData = creationChangeData->data;

            QCOMPARE(cloneData.parameterIds.size(), 1);
            QCOMPARE(parameter.id(), cloneData.parameterIds.first());
            QCOMPARE(cloneData.techniqueIds.size(), 1);
            QCOMPARE(technique.id(), cloneData.techniqueIds.first());
            QCOMPARE(effect.id(), creationChangeData->subjectId());
            QCOMPARE(effect.isEnabled(), false);
            QCOMPARE(effect.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(effect.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkParameterBookkeeping()
    {
        // GIVEN
        Qt3DRender::QEffect effect;

        {
            // WHEN
            Qt3DRender::QParameter parameter;
            effect.addParameter(&parameter);

            QCOMPARE(effect.parameters().size(), 1);

        }

        // THEN -> should not crash
        QCOMPARE(effect.parameters().size(), 0);

    }

    void checkTechniqueBookkeeping()
    {
        // GIVEN
        Qt3DRender::QEffect effect;

        {
            // WHEN
            Qt3DRender::QTechnique technique;
            effect.addTechnique(&technique);

            QCOMPARE(effect.techniques().size(), 1);

        }

        // THEN -> should not crash
        QCOMPARE(effect.techniques().size(), 0);

    }

    void checkParameterUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QEffect effect;
        Qt3DRender::QParameter parameter;
        arbiter.setArbiterOnNode(&effect);

        {
            // WHEN
            effect.addParameter(&parameter);
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
            effect.removeParameter(&parameter);
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

    void checkTechniqueUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QEffect effect;
        Qt3DRender::QTechnique technique;
        arbiter.setArbiterOnNode(&effect);

        {
            // WHEN
            effect.addTechnique(&technique);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
            QCOMPARE(change->propertyName(), "technique");
            QCOMPARE(change->addedNodeId(), technique.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);

            arbiter.events.clear();
        }

        {
            // WHEN
            effect.removeTechnique(&technique);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
            QCOMPARE(change->propertyName(), "technique");
            QCOMPARE(change->removedNodeId(), technique.id());
            QCOMPARE(change->type(), Qt3DCore::PropertyValueRemoved);

            arbiter.events.clear();
        }

    }

};

QTEST_MAIN(tst_QEffect)

#include "tst_qeffect.moc"
