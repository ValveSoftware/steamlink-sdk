/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
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
#include <Qt3DRender/qcameralens.h>
#include <Qt3DRender/private/qcameralens_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QCameraLens : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DRender::QCameraLens::ProjectionType>("QCameraLens::ProjectionType");
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QCameraLens cameraLens;

        // THEN
        QCOMPARE(cameraLens.projectionType(), Qt3DRender::QCameraLens::PerspectiveProjection);
        QCOMPARE(cameraLens.nearPlane(), 0.1f);
        QCOMPARE(cameraLens.farPlane(), 1024.0f);
        QCOMPARE(cameraLens.fieldOfView(), 25.0f);
        QCOMPARE(cameraLens.aspectRatio(), 1.0f);
        QCOMPARE(cameraLens.left(), -0.5f);
        QCOMPARE(cameraLens.right(), 0.5f);
        QCOMPARE(cameraLens.bottom(), -0.5f);
        QCOMPARE(cameraLens.top(), 0.5f);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QCameraLens cameraLens;

        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(projectionTypeChanged(QCameraLens::ProjectionType)));
            const Qt3DRender::QCameraLens::ProjectionType newValue = Qt3DRender::QCameraLens::OrthographicProjection;
            cameraLens.setProjectionType(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(cameraLens.projectionType(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            cameraLens.setProjectionType(newValue);

            // THEN
            QCOMPARE(cameraLens.projectionType(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(nearPlaneChanged(float)));
            const float newValue = 10.0f;
            cameraLens.setNearPlane(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(cameraLens.nearPlane(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            cameraLens.setNearPlane(newValue);

            // THEN
            QCOMPARE(cameraLens.nearPlane(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(farPlaneChanged(float)));
            const float newValue = 1.0f;
            cameraLens.setFarPlane(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(cameraLens.farPlane(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            cameraLens.setFarPlane(newValue);

            // THEN
            QCOMPARE(cameraLens.farPlane(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(fieldOfViewChanged(float)));
            const float newValue = 5.0f;
            cameraLens.setFieldOfView(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(cameraLens.fieldOfView(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            cameraLens.setFieldOfView(newValue);

            // THEN
            QCOMPARE(cameraLens.fieldOfView(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(aspectRatioChanged(float)));
            const float newValue = 4.0f / 3.0f;
            cameraLens.setAspectRatio(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(cameraLens.aspectRatio(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            cameraLens.setAspectRatio(newValue);

            // THEN
            QCOMPARE(cameraLens.aspectRatio(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(leftChanged(float)));
            const float newValue = 0.0f;
            cameraLens.setLeft(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(cameraLens.left(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            cameraLens.setLeft(newValue);

            // THEN
            QCOMPARE(cameraLens.left(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(rightChanged(float)));
            const float newValue = 1.0f;
            cameraLens.setRight(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(cameraLens.right(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            cameraLens.setRight(newValue);

            // THEN
            QCOMPARE(cameraLens.right(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(bottomChanged(float)));
            const float newValue = 2.0f;
            cameraLens.setBottom(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(cameraLens.bottom(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            cameraLens.setBottom(newValue);

            // THEN
            QCOMPARE(cameraLens.bottom(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(topChanged(float)));
            const float newValue = -2.0f;
            cameraLens.setTop(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(cameraLens.top(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            cameraLens.setTop(newValue);

            // THEN
            QCOMPARE(cameraLens.top(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(projectionMatrixChanged(QMatrix4x4)));
            QMatrix4x4 newValue;
            newValue.translate(5.0f, 2.0f, 4.3f);
            cameraLens.setProjectionMatrix(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(cameraLens.projectionMatrix(), newValue);
            QCOMPARE(cameraLens.projectionType(), Qt3DRender::QCameraLens::CustomProjection);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            cameraLens.setProjectionMatrix(newValue);

            // THEN
            QCOMPARE(cameraLens.projectionMatrix(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkSetOrthographicProjection()
    {
        // GIVEN
        Qt3DRender::QCameraLens cameraLens;

        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(projectionMatrixChanged(QMatrix4x4)));
            cameraLens.setOrthographicProjection(-1.0f, 1.0f, -1.0f, 1.0f, 0.5f, 50.0f);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(spy.count(), 8); // Triggered for each property being set + 1
            QCOMPARE(cameraLens.projectionType(), Qt3DRender::QCameraLens::OrthographicProjection);
            QCOMPARE(cameraLens.nearPlane(), 0.5f);
            QCOMPARE(cameraLens.farPlane(), 50.0f);
            QCOMPARE(cameraLens.left(), -1.0f);
            QCOMPARE(cameraLens.right(), 1.0f);
            QCOMPARE(cameraLens.bottom(), -1.0f);
            QCOMPARE(cameraLens.top(), 1.0f);
        }
    }

    void checkSetPerspectiveProjection()
    {
        // GIVEN
        Qt3DRender::QCameraLens cameraLens;

        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(projectionMatrixChanged(QMatrix4x4)));
            cameraLens.setPerspectiveProjection(20.0f, 16.0f / 9.0f, 0.5f, 50.0f);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(spy.count(), 5); // Triggered for each property being set (- projectionTye which is the default value) + 1
            QCOMPARE(cameraLens.projectionType(), Qt3DRender::QCameraLens::PerspectiveProjection);
            QCOMPARE(cameraLens.nearPlane(), 0.5f);
            QCOMPARE(cameraLens.farPlane(), 50.0f);
            QCOMPARE(cameraLens.fieldOfView(), 20.0f);
            QCOMPARE(cameraLens.aspectRatio(), 16.0f / 9.0f);
        }
    }

    void checkSetFrustumProjection()
    {
        // GIVEN
        Qt3DRender::QCameraLens cameraLens;

        {
            // WHEN
            QSignalSpy spy(&cameraLens, SIGNAL(projectionMatrixChanged(QMatrix4x4)));
            cameraLens.setFrustumProjection(-1.0f, 1.0f, -1.0f, 1.0f, 0.5f, 50.0f);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(spy.count(), 8); // Triggered for each property being set + 1
            QCOMPARE(cameraLens.projectionType(), Qt3DRender::QCameraLens::FrustumProjection);
            QCOMPARE(cameraLens.nearPlane(), 0.5f);
            QCOMPARE(cameraLens.farPlane(), 50.0f);
            QCOMPARE(cameraLens.left(), -1.0f);
            QCOMPARE(cameraLens.right(), 1.0f);
            QCOMPARE(cameraLens.bottom(), -1.0f);
            QCOMPARE(cameraLens.top(), 1.0f);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QCameraLens cameraLens;

        cameraLens.setNearPlane(0.5);
        cameraLens.setFarPlane(1005.0f);
        cameraLens.setFieldOfView(35.0f);
        cameraLens.setAspectRatio(16.0f/9.0f);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&cameraLens);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QCameraLensData>>(creationChanges.first());
            const Qt3DRender::QCameraLensData cloneData = creationChangeData->data;

            QCOMPARE(cameraLens.projectionMatrix(), cloneData.projectionMatrix);
            QCOMPARE(cameraLens.id(), creationChangeData->subjectId());
            QCOMPARE(cameraLens.isEnabled(), true);
            QCOMPARE(cameraLens.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(cameraLens.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        cameraLens.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&cameraLens);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QCameraLensData>>(creationChanges.first());
            const Qt3DRender::QCameraLensData cloneData = creationChangeData->data;

            QCOMPARE(cameraLens.projectionMatrix(), cloneData.projectionMatrix);
            QCOMPARE(cameraLens.id(), creationChangeData->subjectId());
            QCOMPARE(cameraLens.isEnabled(), false);
            QCOMPARE(cameraLens.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(cameraLens.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkProjectionTypeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QCameraLens cameraLens;
        arbiter.setArbiterOnNode(&cameraLens);

        {
            // WHEN
            cameraLens.setProjectionType(Qt3DRender::QCameraLens::FrustumProjection);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "projectionMatrix");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            cameraLens.setProjectionType(Qt3DRender::QCameraLens::FrustumProjection);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkNearPlaneUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QCameraLens cameraLens;
        arbiter.setArbiterOnNode(&cameraLens);

        {
            // WHEN
            cameraLens.setNearPlane(5.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "projectionMatrix");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            cameraLens.setNearPlane(5.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkFarPlaneUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QCameraLens cameraLens;
        arbiter.setArbiterOnNode(&cameraLens);

        {
            // WHEN
            cameraLens.setFarPlane(5.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "projectionMatrix");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            cameraLens.setFarPlane(5.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkFieldOfViewUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QCameraLens cameraLens;
        arbiter.setArbiterOnNode(&cameraLens);

        {
            // WHEN
            cameraLens.setFieldOfView(5.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "projectionMatrix");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            cameraLens.setFieldOfView(5.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkAspectRatioUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QCameraLens cameraLens;
        arbiter.setArbiterOnNode(&cameraLens);

        {
            // WHEN
            cameraLens.setAspectRatio(9.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "projectionMatrix");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            cameraLens.setAspectRatio(9.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkLeftUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QCameraLens cameraLens;
        arbiter.setArbiterOnNode(&cameraLens);

        {
            // WHEN
            cameraLens.setLeft(0.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "projectionMatrix");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            cameraLens.setLeft(0.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkRightUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QCameraLens cameraLens;
        arbiter.setArbiterOnNode(&cameraLens);

        {
            // WHEN
            cameraLens.setRight(24.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "projectionMatrix");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            cameraLens.setRight(24.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkBottomUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QCameraLens cameraLens;
        arbiter.setArbiterOnNode(&cameraLens);

        {
            // WHEN
            cameraLens.setBottom(-12.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "projectionMatrix");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            cameraLens.setBottom(-12.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkTopUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QCameraLens cameraLens;
        arbiter.setArbiterOnNode(&cameraLens);

        {
            // WHEN
            cameraLens.setTop(12.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "projectionMatrix");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            cameraLens.setTop(12.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkProjectionMatrixUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QCameraLens cameraLens;
        arbiter.setArbiterOnNode(&cameraLens);

        QMatrix4x4 m;
        m.translate(-5.0f, 5.0f, 25.0f);

        {
            // WHEN
            cameraLens.setProjectionMatrix(m);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "projectionMatrix");
            QCOMPARE(change->value().value<QMatrix4x4>(), m);
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            cameraLens.setProjectionMatrix(m);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QCameraLens)

#include "tst_qcameralens.moc"
