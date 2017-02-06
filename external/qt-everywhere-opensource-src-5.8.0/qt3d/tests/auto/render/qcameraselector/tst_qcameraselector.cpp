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
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DRender/qcameraselector.h>
#include <Qt3DRender/private/qcameraselector_p.h>
#include <Qt3DCore/qentity.h>

#include "testpostmanarbiter.h"

class tst_QCameraSelector: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QCameraSelector *>("cameraSelector");
        QTest::addColumn<Qt3DCore::QEntity *>("camera");

        Qt3DRender::QCameraSelector *defaultConstructed = new Qt3DRender::QCameraSelector();
        QTest::newRow("defaultConstructed") << defaultConstructed << static_cast<Qt3DCore::QEntity *>(nullptr);

        Qt3DRender::QCameraSelector *selector1 = new Qt3DRender::QCameraSelector();
        Qt3DCore::QEntity *camera1 = new Qt3DCore::QEntity();
        QTest::newRow("valid camera") << selector1 << camera1;
        selector1->setCamera(camera1);
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QCameraSelector *, cameraSelector);
        QFETCH(Qt3DCore::QEntity *, camera);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(cameraSelector);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + (camera ? 1 : 0));

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QCameraSelectorData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QCameraSelectorData>>(creationChanges.first());
        const Qt3DRender::QCameraSelectorData &cloneData = creationChangeData->data;

        QCOMPARE(cameraSelector->id(), creationChangeData->subjectId());
        QCOMPARE(cameraSelector->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(cameraSelector->metaObject(), creationChangeData->metaObject());
        QCOMPARE(cameraSelector->camera() ? cameraSelector->camera()->id() : Qt3DCore::QNodeId(), cloneData.cameraId);

        delete cameraSelector;
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QCameraSelector> cameraSelector(new Qt3DRender::QCameraSelector());
        arbiter.setArbiterOnNode(cameraSelector.data());

        // WHEN
        Qt3DCore::QEntity *camera = new Qt3DCore::QEntity();
        cameraSelector->setCamera(camera);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "camera");
        QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), camera->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        cameraSelector->setCamera(camera);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        Qt3DCore::QEntity *camera2 = new Qt3DCore::QEntity();
        cameraSelector->setCamera(camera2);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "camera");
        QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), camera2->id());
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);
        arbiter.events.clear();

        // WHEN
        cameraSelector->setCamera(nullptr);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "camera");
        QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), Qt3DCore::QNodeId());
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }

    void checkCameraBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QCameraSelector> cameraSelector(new Qt3DRender::QCameraSelector);
        {
            // WHEN
            Qt3DCore::QEntity camera;
            cameraSelector->setCamera(&camera);

            // THEN
            QCOMPARE(camera.parent(), cameraSelector.data());
            QCOMPARE(cameraSelector->camera(), &camera);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(cameraSelector->camera() == nullptr);

        {
            // WHEN
            Qt3DRender::QCameraSelector someOtherCameraSelector;
            QScopedPointer<Qt3DCore::QEntity> camera(new Qt3DCore::QEntity(&someOtherCameraSelector));
            cameraSelector->setCamera(camera.data());

            // THEN
            QCOMPARE(camera->parent(), &someOtherCameraSelector);
            QCOMPARE(cameraSelector->camera(), camera.data());

            // WHEN
            cameraSelector.reset();
            camera.reset();

            // THEN Should not crash when the camera is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QCameraSelector)

#include "tst_qcameraselector.moc"
