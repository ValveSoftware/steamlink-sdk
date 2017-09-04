/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire
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
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qtransform.h>

#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/private/filterentitybycomponentjob_p.h>
#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qcameralens.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qobjectpicker.h>
#include <Qt3DRender/qcomputecommand.h>

#include <Qt3DRender/private/transform_p.h>
#include <Qt3DRender/private/geometryrenderer_p.h>
#include <Qt3DRender/private/computecommand_p.h>
#include <Qt3DRender/private/objectpicker_p.h>
#include <Qt3DRender/private/cameralens_p.h>
#include <Qt3DRender/private/objectpicker_p.h>
#include "testaspect.h"

class tst_FilterEntityByComponent : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void filterEntities()
    {
        // GIVEN
        Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();
        Qt3DCore::QEntity *childEntity1 = new Qt3DCore::QEntity(rootEntity);
        Qt3DCore::QEntity *childEntity2 = new Qt3DCore::QEntity(rootEntity);
        Qt3DCore::QEntity *childEntity3 = new Qt3DCore::QEntity(rootEntity);

        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform(rootEntity);
        Qt3DRender::QGeometryRenderer *geomRenderer = new Qt3DRender::QGeometryRenderer(rootEntity);
        Qt3DRender::QCameraLens *cameraLens = new Qt3DRender::QCameraLens(rootEntity);
        Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial(rootEntity);
        Qt3DRender::QObjectPicker *objectPicker = new Qt3DRender::QObjectPicker(rootEntity);
        Qt3DRender::QComputeCommand *computeCommand = new Qt3DRender::QComputeCommand(rootEntity);

        childEntity1->addComponent(transform);
        childEntity1->addComponent(geomRenderer);
        childEntity1->addComponent(material);

        childEntity2->addComponent(cameraLens);
        childEntity2->addComponent(transform);

        childEntity3->addComponent(objectPicker);
        childEntity3->addComponent(computeCommand);

        QScopedPointer<Qt3DRender::TestAspect> aspect(new Qt3DRender::TestAspect(rootEntity));

        {
            // WHEN
            Qt3DRender::Render::FilterEntityByComponentJob<Qt3DRender::Render::Transform> filterJob;
            filterJob.setManager(aspect->nodeManagers()->renderNodesManager());
            filterJob.run();

            // THEN
            QCOMPARE(filterJob.filteredEntities().size(), 2);
            QCOMPARE(filterJob.filteredEntities().first()->peerId(), childEntity1->id());
            QCOMPARE(filterJob.filteredEntities().last()->peerId(), childEntity2->id());
        }

        {
            // WHEN
            Qt3DRender::Render::FilterEntityByComponentJob<Qt3DRender::Render::Transform, Qt3DRender::Render::Material> filterJob;
            filterJob.setManager(aspect->nodeManagers()->renderNodesManager());
            filterJob.run();

            // THEN
            QCOMPARE(filterJob.filteredEntities().size(), 1);
            QCOMPARE(filterJob.filteredEntities().first()->peerId(), childEntity1->id());
        }

        {
            // WHEN
            Qt3DRender::Render::FilterEntityByComponentJob<Qt3DRender::Render::Transform, Qt3DRender::Render::Material, Qt3DRender::Render::ComputeCommand> filterJob;
            filterJob.setManager(aspect->nodeManagers()->renderNodesManager());
            filterJob.run();

            // THEN
            QCOMPARE(filterJob.filteredEntities().size(), 0);
        }

        {
            // WHEN
            Qt3DRender::Render::FilterEntityByComponentJob<Qt3DRender::Render::ComputeCommand> filterJob;
            filterJob.setManager(aspect->nodeManagers()->renderNodesManager());
            filterJob.run();

            // THEN
            QCOMPARE(filterJob.filteredEntities().size(), 1);
            QCOMPARE(filterJob.filteredEntities().first()->peerId(), childEntity3->id());
        }
    }
};

QTEST_MAIN(tst_FilterEntityByComponent)

#include "tst_filterentitybycomponent.moc"
