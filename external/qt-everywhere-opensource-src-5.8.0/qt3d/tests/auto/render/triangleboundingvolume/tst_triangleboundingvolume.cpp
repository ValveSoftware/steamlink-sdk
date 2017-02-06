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

#include <QtTest/QtTest>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/private/triangleboundingvolume_p.h>
#include <Qt3DRender/private/qraycastingservice_p.h>
#include <Qt3DRender/private/qray3d_p.h>
#include <Qt3DRender/qcameralens.h>
#include <Qt3DRender/qcamera.h>
#include <Qt3DRender/private/qboundingvolume_p.h>

class tst_TriangleBoundingVolume : public QObject
{
    Q_OBJECT
public:
    tst_TriangleBoundingVolume() {}
    ~tst_TriangleBoundingVolume() {}

private Q_SLOTS:
    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::Render::TriangleBoundingVolume volume = Qt3DRender::Render::TriangleBoundingVolume(Qt3DCore::QNodeId(),
                                                                                                       QVector3D(),
                                                                                                       QVector3D(),
                                                                                                       QVector3D());

        // THEN
        QCOMPARE(volume.id(), Qt3DCore::QNodeId());
        QCOMPARE(volume.a(), QVector3D());
        QCOMPARE(volume.b(), QVector3D());
        QCOMPARE(volume.c(), QVector3D());
        QCOMPARE(volume.type(), Qt3DRender::QBoundingVolume::Triangle);
    }

    void transformed_data()
    {
        QTest::addColumn<QVector3D>("a");
        QTest::addColumn<QVector3D>("b");
        QTest::addColumn<QVector3D>("c");

        QTest::addColumn<QVector3D>("transformedA");
        QTest::addColumn<QVector3D>("transformedB");
        QTest::addColumn<QVector3D>("transformedC");

        QTest::newRow("onFarPlane")
                << QVector3D(-1.0, 1.0, 0.0)
                << QVector3D(0.0, -1.0, 0.0)
                << QVector3D(1.0, 1.0, 0.0)
                << QVector3D(-1.0, 1.0, -40.0)
                << QVector3D(0.0, -1.0, -40.0)
                << QVector3D(1.0, 1.0, -40.0);

        QTest::newRow("onNearPlane")
                << QVector3D(-1.0, 1.0, 40.0)
                << QVector3D(0.0, -1.0, 40.0)
                << QVector3D(1.0, 1.0, 40.0)
                << QVector3D(-1.0, 1.0, 0.0)
                << QVector3D(0.0, -1.0, 0.0)
                << QVector3D(1.0, 1.0, 0.0);


    }

    void transformed()
    {
        // GIVEN
        QFETCH(QVector3D, a);
        QFETCH(QVector3D, b);
        QFETCH(QVector3D, c);
        QFETCH(QVector3D, transformedA);
        QFETCH(QVector3D, transformedB);
        QFETCH(QVector3D, transformedC);
        Qt3DRender::Render::TriangleBoundingVolume volume(Qt3DCore::QNodeId(),
                                                          a,
                                                          b,
                                                          c);
        Qt3DRender::QCamera camera;
        camera.setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
        camera.setFieldOfView(45.0f);
        camera.setAspectRatio(800.0f/600.0f);
        camera.setNearPlane(0.1f);
        camera.setFarPlane(1000.0f);
        camera.setPosition(QVector3D(0.0f, 0.0f, 40.0f));
        camera.setUpVector(QVector3D(0.0f, 1.0f, 0.0f));
        camera.setViewCenter(QVector3D(0.0f, 0.0f, 0.0f));

        const QMatrix4x4 viewMatrix = camera.viewMatrix();

        // WHEN
        volume.transform(viewMatrix);

        // THEN
        QCOMPARE(transformedA, volume.a());
        QCOMPARE(transformedB, volume.b());
        QCOMPARE(transformedC, volume.c());
    }

    void intersects_data()
    {
        QTest::addColumn<Qt3DRender::QRay3D>("ray");
        QTest::addColumn<QVector3D>("a");
        QTest::addColumn<QVector3D>("b");
        QTest::addColumn<QVector3D>("c");
        QTest::addColumn<QVector3D>("uvw");
        QTest::addColumn<float>("t");
        QTest::addColumn<bool>("isIntersecting");

        const float farPlaneDistance = 40.0;

        QTest::newRow("halfway_center")
                << Qt3DRender::QRay3D(QVector3D(), QVector3D(0.0, 0.0, 1.0), farPlaneDistance)
                << QVector3D(3.0, 1.5, 20.0)
                << QVector3D(0.0, -1.5, 20.0)
                << QVector3D(-3, 1.5, 20.0)
                << QVector3D(0.25, 0.5, 0.25)
                << 0.5f
                << true;
        QTest::newRow("miss_halfway_center_too_short")
                << Qt3DRender::QRay3D(QVector3D(), QVector3D(0.0, 0.0, 1.0), farPlaneDistance * 0.25f)
                << QVector3D(3.0, 1.5, 20.0)
                << QVector3D(0.0, -1.5, 20.0)
                << QVector3D(-3, 1.5, 20.0)
                << QVector3D()
                << 0.0f
                << false;
        QTest::newRow("far_center")
                << Qt3DRender::QRay3D(QVector3D(), QVector3D(0.0, 0.0, 1.0), farPlaneDistance)
                << QVector3D(3.0, 1.5, 40.0)
                << QVector3D(0.0, -1.5, 40.0)
                << QVector3D(-3, 1.5, 40.0)
                << QVector3D(0.25, 0.5, 0.25)
                << 1.0f
                << true;
        QTest::newRow("near_center")
                << Qt3DRender::QRay3D(QVector3D(), QVector3D(0.0, 0.0, 1.0), 1.0f)
                << QVector3D(3.0, 1.5, 0.0)
                << QVector3D(0.0, -1.5, 0.0)
                << QVector3D(-3, 1.5, 0.0)
                << QVector3D(0.25, 0.5, 0.25)
                << 0.0f
                << true;
        QTest::newRow("above_miss_center")
                << Qt3DRender::QRay3D(QVector3D(0.0, 2.0, 0.0), QVector3D(0.0, 2.0, 1.0), 1.0f)
                << QVector3D(3.0, 1.5, 0.0)
                << QVector3D(0.0, -1.5, 0.0)
                << QVector3D(-3, 1.5, 0.0)
                << QVector3D()
                << 0.0f
                << false;
        QTest::newRow("below_miss_center")
                << Qt3DRender::QRay3D(QVector3D(0.0, -2.0, 0.0), QVector3D(0.0, -2.0, 1.0), 1.0f)
                << QVector3D(3.0, 1.5, 0.0)
                << QVector3D(0.0, -1.5, 0.0)
                << QVector3D(-3, 1.5, 0.0)
                << QVector3D()
                << 0.0f
                << false;
    }

    void intersects()
    {
        // GIVEN
        QFETCH(Qt3DRender::QRay3D, ray);
        QFETCH(QVector3D, a);
        QFETCH(QVector3D, b);
        QFETCH(QVector3D, c);
        QFETCH(QVector3D, uvw);
        QFETCH(float, t);
        QFETCH(bool, isIntersecting);

        // WHEN
        QVector3D tmp_uvw;
        float tmp_t;
        const bool shouldBeIntersecting = Qt3DRender::Render::intersectsSegmentTriangle(ray,
                                                                                        a, b, c,
                                                                                        tmp_uvw,
                                                                                        tmp_t);

        // THEN
        QCOMPARE(shouldBeIntersecting, isIntersecting);
        if (isIntersecting) {
            QVERIFY(qFuzzyCompare(uvw, tmp_uvw));
            QVERIFY(qFuzzyCompare(t, tmp_t));
        }
    }
};

QTEST_APPLESS_MAIN(tst_TriangleBoundingVolume)

#include "tst_triangleboundingvolume.moc"
