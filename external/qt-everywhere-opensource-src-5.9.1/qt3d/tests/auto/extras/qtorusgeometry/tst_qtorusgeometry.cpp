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
#include <Qt3DExtras/qtorusgeometry.h>
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
#include <qmath.h>

#include "geometrytesthelper.h"

class tst_QTorusGeometry : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultConstruction()
    {
        // WHEN
        Qt3DExtras::QTorusGeometry geometry;

        // THEN
        QCOMPARE(geometry.rings(), 16);
        QCOMPARE(geometry.slices(), 16);
        QCOMPARE(geometry.radius(), 1.0f);
        QCOMPARE(geometry.minorRadius(), 1.0f);
        QVERIFY(geometry.positionAttribute() != nullptr);
        QCOMPARE(geometry.positionAttribute()->name(), Qt3DRender::QAttribute::defaultPositionAttributeName());
        QVERIFY(geometry.normalAttribute() != nullptr);
        QCOMPARE(geometry.normalAttribute()->name(), Qt3DRender::QAttribute::defaultNormalAttributeName());
        QVERIFY(geometry.texCoordAttribute() != nullptr);
        QCOMPARE(geometry.texCoordAttribute()->name(), Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());
        // TODO: Expose tangent attribute in Qt 5.8 and see below
//        QVERIFY(geometry.tangentAttribute() != nullptr);
//        QCOMPARE(geometry.tangentAttribute()->name(), Qt3DRender::QAttribute::defaultTangentAttributeName());
        QVERIFY(geometry.indexAttribute() != nullptr);
    }

    void properties()
    {
        // GIVEN
        Qt3DExtras::QTorusGeometry geometry;

        {
            // WHEN
            QSignalSpy spy(&geometry, SIGNAL(ringsChanged(int)));
            const int newValue = 20;
            geometry.setRings(newValue);

            // THEN
            QCOMPARE(geometry.rings(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            geometry.setRings(newValue);

            // THEN
            QCOMPARE(geometry.rings(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&geometry, SIGNAL(slicesChanged(int)));
            const int newValue = 2.0f;
            geometry.setSlices(newValue);

            // THEN
            QCOMPARE(geometry.slices(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            geometry.setSlices(newValue);

            // THEN
            QCOMPARE(geometry.slices(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&geometry, SIGNAL(radiusChanged(float)));
            const float newValue = 2.0f;
            geometry.setRadius(newValue);

            // THEN
            QCOMPARE(geometry.radius(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            geometry.setRadius(newValue);

            // THEN
            QCOMPARE(geometry.radius(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&geometry, SIGNAL(minorRadiusChanged(float)));
            const float newValue = 0.25f;
            geometry.setMinorRadius(newValue);

            // THEN
            QCOMPARE(geometry.minorRadius(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            geometry.setMinorRadius(newValue);

            // THEN
            QCOMPARE(geometry.minorRadius(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void generatedGeometryShouldBeConsistent_data()
    {
        QTest::addColumn<int>("rings");
        QTest::addColumn<int>("slices");
        QTest::addColumn<float>("radius");
        QTest::addColumn<float>("minorRadius");
        QTest::addColumn<int>("triangleIndex");
        QTest::addColumn<QVector<quint16>>("indices");
        QTest::addColumn<QVector<QVector3D>>("positions");
        QTest::addColumn<QVector<QVector3D>>("normals");
        QTest::addColumn<QVector<QVector2D>>("texCoords");
        QTest::addColumn<QVector<QVector4D>>("tangents");

        {
            // Torus properties
            const int rings = 8;
            const int slices = 8;
            const float radius = 2.0f;
            const float minorRadius = 0.5f;

            // Angular factors for the vertices:
            // u iterates around the major radius
            // v iterates around the minor radius (around each ring)
            const float du = float(2.0 * M_PI / rings);
            const float dv = float(2.0 * M_PI / slices);
            const float u0 = 0.0f;
            const float u1 = du;
            const float v0 = 0.0f;
            const float v1 = dv;

            const float cosu0 = float(qCos(u0));
            const float sinu0 = float(qSin(u0));
            const float cosu1 = float(qCos(u1));
            const float sinu1 = float(qSin(u1));

            const float cosv0 = float(qCos(v0 + M_PI)); // Seam is on inner edge
            const float sinv0 = float(qSin(v0));
            const float cosv1 = float(qCos(v1 + M_PI));
            const float sinv1 = float(qSin(v1));

            // The triangle and indices
            const int triangleIndex = 0;
            const auto indices = (QVector<quint16>() << 0 << 1 << 9);

            // Calculate attributes for vertices A, B, and C of the triangle
            const float rA = radius + minorRadius * cosv0;
            const float rB = radius + minorRadius * cosv1;
            const float rC = radius + minorRadius * cosv0;

            const auto posA = QVector3D(rA * cosu0, rA * sinu0, minorRadius * sinv0);
            const auto posB = QVector3D(rB * cosu0, rB * sinu0, minorRadius * sinv1);
            const auto posC = QVector3D(rC * cosu1, rC * sinu1, minorRadius * sinv0);
            const auto positions = (QVector<QVector3D>() << posA << posB << posC);

            const auto nA = QVector3D(cosv0 * cosu0, cosv0 * sinu0, sinv0).normalized();
            const auto nB = QVector3D(cosv1 * cosu0, cosv1 * sinu0, sinv1).normalized();
            const auto nC = QVector3D(cosv0 * cosu1, cosv0 * sinu1, sinv0).normalized();
            const auto normals = (QVector<QVector3D>() << nA << nB << nC);

            const auto tcA = QVector2D(u0, v0) / float(2.0 * M_PI);
            const auto tcB = QVector2D(u0, v1) / float(2.0 * M_PI);
            const auto tcC = QVector2D(u1, v0) / float(2.0 * M_PI);
            const auto texCoords = (QVector<QVector2D>() << tcA << tcB << tcC);

            const auto tA = QVector4D(-sinu0, cosu0, 0.0f, 1.0f);
            const auto tB = QVector4D(-sinu0, cosu0, 0.0f, 1.0f);
            const auto tC = QVector4D(-sinu1, cosu1, 0.0f, 1.0f);
            const auto tangents = (QVector<QVector4D>() << tA << tB << tC);

            // Add the row
            QTest::newRow("8rings_8slices_firstTriangle")
                    << rings << slices << radius << minorRadius
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }

        {
            // Note: The vertices used in this test case are different than the
            // ones above. So, we cannot abstract this into a function easily.
            // Here we use the 2nd triangle in a rectangular face, the test above
            // uses the first triangle in the rectangular face.

            // Torus properties
            const int rings = 8;
            const int slices = 8;
            const float radius = 2.0f;
            const float minorRadius = 0.5f;

            // Angular factors for the vertices:
            // u iterates around the major radius
            // v iterates around the minor radius (around each ring)
            const float du = float(2.0 * M_PI / rings);
            const float dv = float(2.0 * M_PI / slices);
            const float u0 = 7.0f * du;
            const float u1 = float(2.0 * M_PI);
            const float v0 = 7.0f * dv;
            const float v1 = float(2.0 * M_PI);

            const float cosu0 = float(qCos(u0));
            const float sinu0 = float(qSin(u0));
            const float cosu1 = float(qCos(u1));
            const float sinu1 = float(qSin(u1));

            const float cosv0 = float(qCos(v0 + M_PI)); // Seam is on inner edge
            const float sinv0 = float(qSin(v0));
            const float cosv1 = float(qCos(v1 + M_PI));
            const float sinv1 = float(qSin(v1));

            // The triangle and indices
            const int triangleIndex = 127;
            const auto indices = (QVector<quint16>() << 71 << 80 << 79);

            // Calculate attributes for vertices A, B, and C of the triangle
            const float rA = radius + minorRadius * cosv1;
            const float rB = radius + minorRadius * cosv1;
            const float rC = radius + minorRadius * cosv0;

            const auto posA = QVector3D(rA * cosu0, rA * sinu0, minorRadius * sinv1);
            const auto posB = QVector3D(rB * cosu1, rB * sinu1, minorRadius * sinv1);
            const auto posC = QVector3D(rC * cosu1, rC * sinu1, minorRadius * sinv0);
            const auto positions = (QVector<QVector3D>() << posA << posB << posC);

            const auto nA = QVector3D(cosv1 * cosu0, cosv1 * sinu0, sinv1).normalized();
            const auto nB = QVector3D(cosv1 * cosu1, cosv1 * sinu1, sinv1).normalized();
            const auto nC = QVector3D(cosv0 * cosu1, cosv0 * sinu1, sinv0).normalized();
            const auto normals = (QVector<QVector3D>() << nA << nB << nC);

            const auto tcA = QVector2D(u0, v1) / float(2.0 * M_PI);
            const auto tcB = QVector2D(u1, v1) / float(2.0 * M_PI);
            const auto tcC = QVector2D(u1, v0) / float(2.0 * M_PI);
            const auto texCoords = (QVector<QVector2D>() << tcA << tcB << tcC);

            const auto tA = QVector4D(-sinu0, cosu1, 0.0f, 1.0f);
            const auto tB = QVector4D(-sinu1, cosu1, 0.0f, 1.0f);
            const auto tC = QVector4D(-sinu1, cosu1, 0.0f, 1.0f);
            const auto tangents = (QVector<QVector4D>() << tA << tB << tC);

            // Add the row
            QTest::newRow("8rings_8slices_lastTriangle")
                    << rings << slices << radius << minorRadius
                    << triangleIndex
                    << indices << positions << normals << texCoords << tangents;
        }
    }

    void generatedGeometryShouldBeConsistent()
    {
        // GIVEN
        Qt3DExtras::QTorusGeometry geometry;
        const QVector<Qt3DRender::QAttribute *> attributes = geometry.attributes();
        Qt3DRender::QAttribute *positionAttribute = geometry.positionAttribute();
        Qt3DRender::QAttribute *normalAttribute = geometry.normalAttribute();
        Qt3DRender::QAttribute *texCoordAttribute = geometry.texCoordAttribute();
//        Qt3DRender::QAttribute *tangentAttribute = geometry.tangentAttribute();
        Qt3DRender::QAttribute *indexAttribute = geometry.indexAttribute();

        // WHEN
        QFETCH(int, rings);
        QFETCH(int, slices);
        QFETCH(float, radius);
        QFETCH(float, minorRadius);
        geometry.setRings(rings);
        geometry.setSlices(slices);
        geometry.setRadius(radius);
        geometry.setMinorRadius(minorRadius);

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
//        QFETCH(QVector<QVector4D>, tangents);

        int i = 0;
        for (auto index : indices) {
            const auto testIndex = extractIndexData<quint16>(indexAttribute, 3 * triangleIndex + i);
            QCOMPARE(testIndex, indices.at(i));

            const auto position = extractVertexData<QVector3D, quint32>(positionAttribute, index);
            QVERIFY(qFuzzyCompare(position, positions.at(i)));

            const auto normal = extractVertexData<QVector3D, quint32>(normalAttribute, index);
            QVERIFY(qFuzzyCompare(normal, normals.at(i)));

            const auto texCoord = extractVertexData<QVector2D, quint32>(texCoordAttribute, index);
            QVERIFY(qFuzzyCompare(texCoord, texCoords.at(i)));

//            const auto tangent = extractVertexData<QVector4D, quint32>(tangentAttribute, index);
//            QVERIFY(qFuzzyCompare(tangent, tangents.at(i)));

            ++i;
        }
    }
};


QTEST_APPLESS_MAIN(tst_QTorusGeometry)

#include "tst_qtorusgeometry.moc"
