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
#include <QObject>
#include <Qt3DExtras/qcuboidgeometry.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qbufferdatagenerator.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qvector2d.h>
#include <QtGui/qvector3d.h>
#include <QtGui/qvector4d.h>
#include <QtCore/qdebug.h>
#include <QtCore/qsharedpointer.h>
#include <QSignalSpy>

#include "geometrytesthelper.h"

class tst_QCuboidGeometry : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultConstruction()
    {
        // WHEN
        Qt3DExtras::QCuboidGeometry geometry;

        // THEN
        QCOMPARE(geometry.xExtent(), 1.0f);
        QCOMPARE(geometry.yExtent(), 1.0f);
        QCOMPARE(geometry.zExtent(), 1.0f);
        QCOMPARE(geometry.xyMeshResolution(), QSize(2, 2));
        QCOMPARE(geometry.yzMeshResolution(), QSize(2, 2));
        QCOMPARE(geometry.xzMeshResolution(), QSize(2, 2));
        QVERIFY(geometry.positionAttribute() != nullptr);
        QCOMPARE(geometry.positionAttribute()->name(), Qt3DRender::QAttribute::defaultPositionAttributeName());
        QVERIFY(geometry.normalAttribute() != nullptr);
        QCOMPARE(geometry.normalAttribute()->name(), Qt3DRender::QAttribute::defaultNormalAttributeName());
        QVERIFY(geometry.texCoordAttribute() != nullptr);
        QCOMPARE(geometry.texCoordAttribute()->name(), Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());
        QVERIFY(geometry.tangentAttribute() != nullptr);
        QCOMPARE(geometry.tangentAttribute()->name(), Qt3DRender::QAttribute::defaultTangentAttributeName());
        QVERIFY(geometry.indexAttribute() != nullptr);
    }

    void properties()
    {
        // GIVEN
        Qt3DExtras::QCuboidGeometry geometry;

        {
            // WHEN
            QSignalSpy spy(&geometry, SIGNAL(xExtentChanged(float)));
            const float newValue = 2.0f;
            geometry.setXExtent(newValue);

            // THEN
            QCOMPARE(geometry.xExtent(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            geometry.setXExtent(newValue);

            // THEN
            QCOMPARE(geometry.xExtent(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&geometry, SIGNAL(yExtentChanged(float)));
            const float newValue = 2.0f;
            geometry.setYExtent(newValue);

            // THEN
            QCOMPARE(geometry.yExtent(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            geometry.setYExtent(newValue);

            // THEN
            QCOMPARE(geometry.yExtent(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&geometry, SIGNAL(zExtentChanged(float)));
            const float newValue = 2.0f;
            geometry.setZExtent(newValue);

            // THEN
            QCOMPARE(geometry.zExtent(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            geometry.setZExtent(newValue);

            // THEN
            QCOMPARE(geometry.zExtent(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&geometry, SIGNAL(xyMeshResolutionChanged(QSize)));
            const auto newValue = QSize(4, 8);
            geometry.setXYMeshResolution(newValue);

            // THEN
            QCOMPARE(geometry.xyMeshResolution(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            geometry.setXYMeshResolution(newValue);

            // THEN
            QCOMPARE(geometry.xyMeshResolution(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&geometry, SIGNAL(yzMeshResolutionChanged(QSize)));
            const auto newValue = QSize(4, 8);
            geometry.setYZMeshResolution(newValue);

            // THEN
            QCOMPARE(geometry.yzMeshResolution(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            geometry.setYZMeshResolution(newValue);

            // THEN
            QCOMPARE(geometry.yzMeshResolution(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&geometry, SIGNAL(xzMeshResolutionChanged(QSize)));
            const auto newValue = QSize(4, 8);
            geometry.setXZMeshResolution(newValue);

            // THEN
            QCOMPARE(geometry.xzMeshResolution(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            geometry.setXZMeshResolution(newValue);

            // THEN
            QCOMPARE(geometry.xzMeshResolution(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void generatedGeometryShouldBeConsistent_data()
    {
        QTest::addColumn<float>("xExtent");
        QTest::addColumn<float>("yExtent");
        QTest::addColumn<float>("zExtent");
        QTest::addColumn<QSize>("xyMeshResolution");
        QTest::addColumn<QSize>("yzMeshResolution");
        QTest::addColumn<QSize>("xzMeshResolution");
        QTest::addColumn<int>("triangleIndex");
        QTest::addColumn<QVector<quint16>>("indices");
        QTest::addColumn<QVector<QVector3D>>("positions");
        QTest::addColumn<QVector<QVector3D>>("normals");
        QTest::addColumn<QVector<QVector2D>>("texCoords");
        QTest::addColumn<QVector<QVector4D>>("tangents");

        {
            const int triangleIndex = 0;
            const auto indices = (QVector<quint16>() << 0 << 1 << 2);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(0.5f, -0.5f, -0.5f)
                    << QVector3D(0.5f, 0.5f, -0.5f)
                    << QVector3D(0.5f, -0.5f, 0.5f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(1.0f, 0.0f, 0.0f)
                    << QVector3D(1.0f, 0.0f, 0.0f)
                    << QVector3D(1.0f, 0.0f, 0.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(1.0f, 0.0f)
                    << QVector2D(1.0f, 1.0f)
                    << QVector2D(0.0f, 0.0f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(0.0f, 0.0f, -1.0f, 1.0f)
                    << QVector4D(0.0f, 0.0f, -1.0f, 1.0f)
                    << QVector4D(0.0f, 0.0f, -1.0f, 1.0f));
            QTest::newRow("default_positiveX_firstTriangle")
                    << 1.0f << 1.0f << 1.0f
                    << QSize(2,2) << QSize(2,2) << QSize(2,2)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 3;
            const auto indices = (QVector<quint16>() << 6 << 5 << 7);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(-0.5f, -0.5f, -0.5f)
                    << QVector3D(-0.5f, 0.5f, 0.5f)
                    << QVector3D(-0.5f, 0.5f, -0.5f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(-1.0f, 0.0f, 0.0f)
                    << QVector3D(-1.0f, 0.0f, 0.0f)
                    << QVector3D(-1.0f, 0.0f, 0.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(0.0f, 0.0f)
                    << QVector2D(1.0f, 1.0f)
                    << QVector2D(0.0f, 1.0f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(0.0f, 0.0f, 1.0f, 1.0f)
                    << QVector4D(0.0f, 0.0f, 1.0f, 1.0f)
                    << QVector4D(0.0f, 0.0f, 1.0f, 1.0f));
            QTest::newRow("default_negativeX_lastTriangle")
                    << 1.0f << 1.0f << 1.0f
                    << QSize(2,2) << QSize(2,2) << QSize(2,2)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 4;
            const auto indices = (QVector<quint16>() << 8 << 9 << 10);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(-0.5f, 0.5f, 0.5f)
                    << QVector3D(0.5f, 0.5f, 0.5f)
                    << QVector3D(-0.5f, 0.5f, -0.5f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(0.0f, 1.0f, 0.0f)
                    << QVector3D(0.0f, 1.0f, 0.0f)
                    << QVector3D(0.0f, 1.0f, 0.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(0.0f, 0.0f)
                    << QVector2D(1.0f, 0.0f)
                    << QVector2D(0.0f, 1.0f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f));
            QTest::newRow("default_positiveY_firstTriangle")
                    << 1.0f << 1.0f << 1.0f
                    << QSize(2,2) << QSize(2,2) << QSize(2,2)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 7;
            const auto indices = (QVector<quint16>() << 14 << 13 << 15);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(-0.5f, -0.5f, 0.5f)
                    << QVector3D(0.5f, -0.5f, -0.5f)
                    << QVector3D(0.5f, -0.5f, 0.5f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(0.0f, -1.0f, 0.0f)
                    << QVector3D(0.0f, -1.0f, 0.0f)
                    << QVector3D(0.0f, -1.0f, 0.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(0.0f, 1.0f)
                    << QVector2D(1.0f, 0.0f)
                    << QVector2D(1.0f, 1.0f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f));
            QTest::newRow("default_negativeY_lastTriangle")
                    << 1.0f << 1.0f << 1.0f
                    << QSize(2,2) << QSize(2,2) << QSize(2,2)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 8;
            const auto indices = (QVector<quint16>() << 16 << 17 << 18);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(-0.5f, -0.5f, 0.5f)
                    << QVector3D(0.5f, -0.5f, 0.5f)
                    << QVector3D(-0.5f, 0.5f, 0.5f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(0.0f, 0.0f, 1.0f)
                    << QVector3D(0.0f, 0.0f, 1.0f)
                    << QVector3D(0.0f, 0.0f, 1.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(0.0f, 0.0f)
                    << QVector2D(1.0f, 0.0f)
                    << QVector2D(0.0f, 1.0f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f));
            QTest::newRow("default_positiveZ_firstTriangle")
                    << 1.0f << 1.0f << 1.0f
                    << QSize(2,2) << QSize(2,2) << QSize(2,2)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 11;
            const auto indices = (QVector<quint16>() << 22 << 21 << 23);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(0.5f, 0.5f, -0.5f)
                    << QVector3D(-0.5f, -0.5f, -0.5f)
                    << QVector3D(-0.5f, 0.5f, -0.5f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(0.0f, 0.0f, -1.0f)
                    << QVector3D(0.0f, 0.0f, -1.0f)
                    << QVector3D(0.0f, 0.0f, -1.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(0.0f, 1.0f)
                    << QVector2D(1.0f, 0.0f)
                    << QVector2D(1.0f, 1.0f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(-1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(-1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(-1.0f, 0.0f, 0.0f, 1.0f));
            QTest::newRow("default_negativeZ_lastTriangle")
                    << 1.0f << 1.0f << 1.0f
                    << QSize(2,2) << QSize(2,2) << QSize(2,2)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 0;
            const auto indices = (QVector<quint16>() << 0 << 1 << 2);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(1.0f, -1.5f, -2.5f)
                    << QVector3D(1.0f, 1.5f, -2.5f)
                    << QVector3D(1.0f, -1.5f, -1.25f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(1.0f, 0.0f, 0.0f)
                    << QVector3D(1.0f, 0.0f, 0.0f)
                    << QVector3D(1.0f, 0.0f, 0.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(1.0f, 0.0f)
                    << QVector2D(1.0f, 1.0f)
                    << QVector2D(0.75f, 0.0f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(0.0f, 0.0f, -1.0f, 1.0f)
                    << QVector4D(0.0f, 0.0f, -1.0f, 1.0f)
                    << QVector4D(0.0f, 0.0f, -1.0f, 1.0f));
            QTest::newRow("default_positiveX_firstTriangle_nonSymmetric")
                    << 2.0f << 3.0f << 5.0f
                    << QSize(2,3) << QSize(2,5) << QSize(2,9)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 15;
            const auto indices = (QVector<quint16>() << 18 << 17 << 19);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(-1.0f, -1.5f, -2.5f)
                    << QVector3D(-1.0f, 1.5f, -1.25f)
                    << QVector3D(-1.0f, 1.5f, -2.5f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(-1.0f, 0.0f, 0.0f)
                    << QVector3D(-1.0f, 0.0f, 0.0f)
                    << QVector3D(-1.0f, 0.0f, 0.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(0.0f, 0.0f)
                    << QVector2D(0.25f, 1.0f)
                    << QVector2D(0.0f, 1.0f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(0.0f, 0.0f, 1.0f, 1.0f)
                    << QVector4D(0.0f, 0.0f, 1.0f, 1.0f)
                    << QVector4D(0.0f, 0.0f, 1.0f, 1.0f));
            QTest::newRow("default_negativeX_lastTriangle_nonSymmetric")
                    << 2.0f << 3.0f << 5.0f
                    << QSize(2,3) << QSize(2,5) << QSize(2,9)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 16;
            const auto indices = (QVector<quint16>() << 20 << 21 << 22);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(-1.0f, 1.5f, 2.5f)
                    << QVector3D(1.0f, 1.5f, 2.5f)
                    << QVector3D(-1.0f, 1.5f, 1.875f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(0.0f, 1.0f, 0.0f)
                    << QVector3D(0.0f, 1.0f, 0.0f)
                    << QVector3D(0.0f, 1.0f, 0.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(0.0f, 0.0f)
                    << QVector2D(1.0f, 0.0f)
                    << QVector2D(0.0f, 0.125f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f));
            QTest::newRow("default_positiveY_firstTriangle_nonSymmetric")
                    << 2.0f << 3.0f << 5.0f
                    << QSize(2,3) << QSize(2,5) << QSize(2,9)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 47;
            const auto indices = (QVector<quint16>() << 54 << 53 << 55);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(-1.0f, -1.5f, 2.5f)
                    << QVector3D(1.0f, -1.5f, 1.875f)
                    << QVector3D(1.0f, -1.5f, 2.5f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(0.0f, -1.0f, 0.0f)
                    << QVector3D(0.0f, -1.0f, 0.0f)
                    << QVector3D(0.0f, -1.0f, 0.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(0.0f, 1.0f)
                    << QVector2D(1.0f, 0.875f)
                    << QVector2D(1.0f, 1.0f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f));
            QTest::newRow("default_negativeY_lastTriangle_nonSymmetric")
                    << 2.0f << 3.0f << 5.0f
                    << QSize(2,3) << QSize(2,5) << QSize(2,9)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 48;
            const auto indices = (QVector<quint16>() << 56 << 57 << 58);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(-1.0f, -1.5f, 2.5f)
                    << QVector3D(1.0f, -1.5f, 2.5f)
                    << QVector3D(-1.0f, 0.0f, 2.5f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(0.0f, 0.0f, 1.0f)
                    << QVector3D(0.0f, 0.0f, 1.0f)
                    << QVector3D(0.0f, 0.0f, 1.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(0.0f, 0.0f)
                    << QVector2D(1.0f, 0.0f)
                    << QVector2D(0.0f, 0.5f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(1.0f, 0.0f, 0.0f, 1.0f));
            QTest::newRow("default_positiveZ_firstTriangle_nonSymmetric")
                    << 2.0f << 3.0f << 5.0f
                    << QSize(2,3) << QSize(2,5) << QSize(2,9)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            const int triangleIndex = 55;
            const auto indices = (QVector<quint16>() << 66 << 65 << 67);
            const auto positions = (QVector<QVector3D>()
                    << QVector3D(1.0f, 1.5f, -2.5f)
                    << QVector3D(-1.0f, 0.0f, -2.5f)
                    << QVector3D(-1.0f, 1.5f, -2.5f));
            const auto normals = (QVector<QVector3D>()
                    << QVector3D(0.0f, 0.0f, -1.0f)
                    << QVector3D(0.0f, 0.0f, -1.0f)
                    << QVector3D(0.0f, 0.0f, -1.0f));
            const auto texCoords = (QVector<QVector2D>()
                    << QVector2D(0.0f, 1.0f)
                    << QVector2D(1.0f, 0.5f)
                    << QVector2D(1.0f, 1.0f));
            const auto tangents = (QVector<QVector4D>()
                    << QVector4D(-1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(-1.0f, 0.0f, 0.0f, 1.0f)
                    << QVector4D(-1.0f, 0.0f, 0.0f, 1.0f));
            QTest::newRow("default_negativeZ_lastTriangle_nonSymmetric")
                    << 2.0f << 3.0f << 5.0f
                    << QSize(2,3) << QSize(2,5) << QSize(2,9)
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }
    }

    void generatedGeometryShouldBeConsistent()
    {
        // GIVEN
        Qt3DExtras::QCuboidGeometry geometry;
        const QVector<Qt3DRender::QAttribute *> attributes = geometry.attributes();
        Qt3DRender::QAttribute *positionAttribute = geometry.positionAttribute();
        Qt3DRender::QAttribute *normalAttribute = geometry.normalAttribute();
        Qt3DRender::QAttribute *texCoordAttribute = geometry.texCoordAttribute();
        Qt3DRender::QAttribute *tangentAttribute = geometry.tangentAttribute();
        Qt3DRender::QAttribute *indexAttribute = geometry.indexAttribute();

        // WHEN
        QFETCH(float, xExtent);
        QFETCH(float, yExtent);
        QFETCH(float, zExtent);
        QFETCH(QSize, xyMeshResolution);
        QFETCH(QSize, yzMeshResolution);
        QFETCH(QSize, xzMeshResolution);
        geometry.setXExtent(xExtent);
        geometry.setYExtent(yExtent);
        geometry.setZExtent(zExtent);
        geometry.setXYMeshResolution(xyMeshResolution);
        geometry.setYZMeshResolution(yzMeshResolution);
        geometry.setXZMeshResolution(xzMeshResolution);

        generateGeometry(geometry);

        // THEN

        // Check buffer of each attribute is valid and actually has some data
        for (const auto &attribute : attributes) {
            Qt3DRender::QBuffer *buffer = attribute->buffer();
            QVERIFY(buffer != nullptr);
            QVERIFY(buffer->data().size() != 0);
        }

        // Check some data in the buffers

        // Check specific indices and vertex attributes of triangle under test
        QFETCH(int, triangleIndex);
        QFETCH(QVector<quint16>, indices);
        QFETCH(QVector<QVector3D>, positions);
        QFETCH(QVector<QVector3D>, normals);
        QFETCH(QVector<QVector2D>, texCoords);
        QFETCH(QVector<QVector4D>, tangents);

        int i = 0;
        for (auto index : indices) {
            const auto testIndex = extractIndexData<quint16>(indexAttribute, 3 * triangleIndex + i);
            QCOMPARE(testIndex, indices.at(i));

            const auto position = extractVertexData<QVector3D, quint32>(positionAttribute, index);
            QCOMPARE(position, positions.at(i));

            const auto normal = extractVertexData<QVector3D, quint32>(normalAttribute, index);
            QCOMPARE(normal, normals.at(i));

            const auto texCoord = extractVertexData<QVector2D, quint32>(texCoordAttribute, index);
            QCOMPARE(texCoord, texCoords.at(i));

            const auto tangent = extractVertexData<QVector4D, quint32>(tangentAttribute, index);
            QCOMPARE(tangent, tangents.at(i));

            ++i;
        }
    }
};


QTEST_APPLESS_MAIN(tst_QCuboidGeometry)

#include "tst_qcuboidgeometry.moc"
