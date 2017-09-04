/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qgeometryfactory.h>
#include <Qt3DRender/qgeometry.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qbufferdatagenerator.h>
#include <Qt3DRender/private/qgeometryrenderer_p.h>
#include <Qt3DRender/private/qgeometry_p.h>
#include <Qt3DRender/private/qattribute_p.h>
#include <Qt3DRender/private/qbuffer_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DExtras/qspheremesh.h>
#include <Qt3DExtras/qcylindermesh.h>
#include <Qt3DExtras/qtorusmesh.h>
#include <Qt3DExtras/qcuboidmesh.h>
#include <Qt3DExtras/qplanemesh.h>


class tst_QDefaultMeshes: public QObject
{
    Q_OBJECT

public:
    tst_QDefaultMeshes()
    {
        qRegisterMetaType<Qt3DCore::QNode*>();
    }

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QGeometryRenderer *>("geomRenderer");
        QTest::newRow("QSphereMesh") << static_cast<Qt3DRender::QGeometryRenderer *>(new Qt3DExtras::QSphereMesh);
        QTest::newRow("QCylinderMesh") << static_cast<Qt3DRender::QGeometryRenderer *>(new Qt3DExtras::QCylinderMesh);
        QTest::newRow("QTorusMesh") << static_cast<Qt3DRender::QGeometryRenderer *>(new Qt3DExtras::QTorusMesh);
        QTest::newRow("QCuboidMesh") << static_cast<Qt3DRender::QGeometryRenderer *>(new Qt3DExtras::QCuboidMesh);
        QTest::newRow("QPlaneMesh") << static_cast<Qt3DRender::QGeometryRenderer *>(new Qt3DExtras::QPlaneMesh);
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QGeometryRenderer *, geomRenderer);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(geomRenderer);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QVERIFY(creationChanges.size() >= 1);

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QGeometryRendererData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QGeometryRendererData>>(creationChanges.first());
        const Qt3DRender::QGeometryRendererData &cloneData = creationChangeData->data;

        QCOMPARE(creationChangeData->subjectId(), geomRenderer->id());
        QCOMPARE(cloneData.instanceCount, geomRenderer->instanceCount());
        QCOMPARE(cloneData.vertexCount, geomRenderer->vertexCount());
        QCOMPARE(cloneData.indexOffset, geomRenderer->indexOffset());
        QCOMPARE(cloneData.firstInstance, geomRenderer->firstInstance());
        QCOMPARE(cloneData.restartIndexValue, geomRenderer->restartIndexValue());
        QCOMPARE(cloneData.primitiveRestart, geomRenderer->primitiveRestartEnabled());
        QCOMPARE(cloneData.primitiveType, geomRenderer->primitiveType());
        QCOMPARE(cloneData.geometryFactory, geomRenderer->geometryFactory());

        if (geomRenderer->geometryFactory()) {
            QVERIFY(cloneData.geometryFactory);
            QVERIFY(*cloneData.geometryFactory == *geomRenderer->geometryFactory());
        }
    }
};

QTEST_MAIN(tst_QDefaultMeshes)

#include "tst_qdefaultmeshes.moc"
