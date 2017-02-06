/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt scene graph research project.
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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <QtQuick/qsggeometry.h>

class GeometryTest : public QObject
{
    Q_OBJECT

public:

private Q_SLOTS:
    void testPoint2D();
    void testTexturedPoint2D();
    void testCustomGeometry();

private:
};

void GeometryTest::testPoint2D()
{
    QSGGeometry geometry(QSGGeometry::defaultAttributes_Point2D(), 4, 0);

    QCOMPARE(geometry.attributeCount(), 1);
    QCOMPARE(geometry.sizeOfVertex(), (int) sizeof(float) * 2);
    QCOMPARE(geometry.vertexCount(), 4);
    QCOMPARE(geometry.indexCount(), 0);
    QVERIFY(!geometry.indexData());

    QSGGeometry::updateRectGeometry(&geometry, QRectF(1, 2, 3, 4));

    QSGGeometry::Point2D *pts = geometry.vertexDataAsPoint2D();
    QVERIFY(pts != 0);

    QCOMPARE(pts[0].x, (float) 1);
    QCOMPARE(pts[0].y, (float) 2);
    QCOMPARE(pts[3].x, (float) 4);
    QCOMPARE(pts[3].y, (float) 6);

    // Verify that resize gives me enough allocated data without crashing...
    geometry.allocate(100, 100);
    pts = geometry.vertexDataAsPoint2D();
    quint16 *is = geometry.indexDataAsUShort();
    for (int i=0; i<100; ++i) {
        pts[i].x = i;
        pts[i].y = i + 100;
        is[i] = i;
    }
    QVERIFY(true);
}


void GeometryTest::testTexturedPoint2D()
{
    QSGGeometry geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4, 0);

    QCOMPARE(geometry.attributeCount(), 2);
    QCOMPARE(geometry.sizeOfVertex(), (int) sizeof(float) * 4);
    QCOMPARE(geometry.vertexCount(), 4);
    QCOMPARE(geometry.indexCount(), 0);
    QVERIFY(!geometry.indexData());

    QSGGeometry::updateTexturedRectGeometry(&geometry, QRectF(1, 2, 3, 4), QRectF(5, 6, 7, 8));

    QSGGeometry::TexturedPoint2D *pts = geometry.vertexDataAsTexturedPoint2D();
    QVERIFY(pts != 0);

    QCOMPARE(pts[0].x, (float) 1);
    QCOMPARE(pts[0].y, (float) 2);
    QCOMPARE(pts[0].tx, (float) 5);
    QCOMPARE(pts[0].ty, (float) 6);

    QCOMPARE(pts[3].x, (float) 4);
    QCOMPARE(pts[3].y, (float) 6);
    QCOMPARE(pts[3].tx, (float) 12);
    QCOMPARE(pts[3].ty, (float) 14);

    // Verify that resize gives me enough allocated data without crashing...
    geometry.allocate(100, 100);
    pts = geometry.vertexDataAsTexturedPoint2D();
    quint16 *is = geometry.indexDataAsUShort();
    for (int i=0; i<100; ++i) {
        pts[i].x = i;
        pts[i].y = i + 100;
        pts[i].tx = i + 200;
        pts[i].ty = i + 300;
        is[i] = i;
    }
    QVERIFY(true);
}

void GeometryTest::testCustomGeometry()
{
    struct V {
        float x, y;
        unsigned char r, g, b, a;
        float v1, v2, v3, v4;
    };

    static QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::create(0, 2, QSGGeometry::FloatType, false),
        QSGGeometry::Attribute::create(1, 4, QSGGeometry::UnsignedByteType, false),
        QSGGeometry::Attribute::create(2, 4, QSGGeometry::FloatType, false)
    };
    static QSGGeometry::AttributeSet set = { 4, 6 * sizeof(float) + 4 * sizeof(unsigned char), attributes };

    QSGGeometry geometry(set, 1000, 4000);

    // Verify that space has been allocated.
    quint16 *ii = geometry.indexDataAsUShort();
    for (int i=0; i<geometry.indexCount(); ++i) {
        ii[i] = i;
    }

    V *v = (V *) geometry.vertexData();
    for (int i=0; i<geometry.vertexCount(); ++i) {
        v[i].x = 0;
        v[i].y = 1;
        v[i].r = 2;
        v[i].g = 3;
        v[i].b = 4;
        v[i].a = 5;
        v[i].v1 = 6;
        v[i].v2 = 7;
        v[i].v3 = 8;
        v[i].v4 = 9;
    }

    // Verify the data's integrity
    for (int i=0; i<4000; ++i)
        QCOMPARE(ii[i], (quint16) i);
    for (int i=0; i<1000; ++i)
        QCOMPARE(v[i].v1, float(6));

}


QTEST_MAIN(GeometryTest);

#include "tst_geometry.moc"
