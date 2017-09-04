/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtPositioning/private/qdoublevector2d_p.h>
#include <QtPositioning/private/qdoublevector3d_p.h>

QT_USE_NAMESPACE

class tst_doubleVectors : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // 2D
    void constructor2dTest();
    void basicFunctions2dTest();
    void unaryOperator2dTest();
    void binaryOperator2dTest();

    // 3D
    void constructor3dTest();
    void basicFunctions3dTest();
    void unaryOperator3dTest();
    void binaryOperator3dTest();
};

// DoubleVector2D

void tst_doubleVectors::constructor2dTest()
{
    // empty constructor, since it sets to 0, we should check, in case people rely on it
    QDoubleVector2D v1;
    QCOMPARE(v1.x(), 0.0);
    QCOMPARE(v1.y(), 0.0);
    QCOMPARE(v1.isNull(), true);
    v1 = QDoubleVector2D(1.1, -2.5); // assignment and constructor
    QCOMPARE(v1.x(), 1.1);
    QCOMPARE(v1.y(), -2.5);
    QDoubleVector2D v2(v1); // copy constructor
    QCOMPARE(v2.x(), 1.1);
    QCOMPARE(v2.y(), -2.5);
    const QDoubleVector3D v3d(2.2, 3.3, 4.4);
    QDoubleVector2D v3(v3d); // constructor from 3d vector, just copies x and y
    QCOMPARE(v3.x(), 2.2);
    QCOMPARE(v3.y(), 3.3);
    QCOMPARE(v3.isNull(), false);
}

void tst_doubleVectors::basicFunctions2dTest()
{
    QDoubleVector2D v1;
    v1.setX(3.0);
    v1.setY(4.0);
    QCOMPARE(v1.x(), 3.0);
    QCOMPARE(v1.y(), 4.0);
    QCOMPARE(v1.length(), 5.0);
    QDoubleVector2D v2 = v1.normalized();
    QCOMPARE(v1.lengthSquared(), 25.0);
    v1.normalize();
    QCOMPARE(v1.x(), 3.0/5.0);
    QCOMPARE(v1.y(), 4.0/5.0);
    QCOMPARE(v2.x(), 3.0/5.0);
    QCOMPARE(v2.y(), 4.0/5.0);

    QDoubleVector3D v3d = v1.toVector3D();
    QCOMPARE(v3d.x(), 3.0/5.0);
    QCOMPARE(v3d.y(), 4.0/5.0);
    QCOMPARE(v3d.z(), 0.0);
}

void tst_doubleVectors::unaryOperator2dTest()
{
    QDoubleVector2D v1(1.1, 2.2);
    QDoubleVector2D v2 = -v1;
    QCOMPARE(v2.x(), -1.1);
    QCOMPARE(v2.y(), -2.2);

    v1 *= 2.0;
    QCOMPARE(v1.x(), 2.2);
    QCOMPARE(v1.y(), 4.4);

    v2 /= 2.0;
    QCOMPARE(v2.x(), -0.55);
    QCOMPARE(v2.y(), -1.1);

    v1 += v2;
    QCOMPARE(v1.x(), 1.65);
    QCOMPARE(v1.y(), 3.3);

    v1 -= v2;
    QCOMPARE(v1.x(), 2.2);
    QCOMPARE(v1.y(), 4.4);

    v1 *= v2;
    QCOMPARE(v1.x(), -1.21);
    QCOMPARE(v1.y(), -4.84);
}

void tst_doubleVectors::binaryOperator2dTest()
{
    QDoubleVector2D v1(1.1, 2.2);
    QDoubleVector2D v2(3.4, 4.4);
    QDoubleVector2D v3 = v1 + v2;
    QCOMPARE(v3.x(), 4.5);
    QCOMPARE(v3.y(), 6.6);

    QDoubleVector2D v4 = v1 - v2;
    QCOMPARE(v4.x(), -2.3);
    QCOMPARE(v4.y(), -2.2);

    QDoubleVector2D v5 = v2 * 2;
    QCOMPARE(v5.x(), 6.8);
    QCOMPARE(v5.y(), 8.8);

    QDoubleVector2D v6 = 2 * v2;
    QCOMPARE(v6.x(), 6.8);
    QCOMPARE(v6.y(), 8.8);

    QDoubleVector2D v7 = v2 / 2;
    QCOMPARE(v7.x(), 1.7);
    QCOMPARE(v7.y(), 2.2);

    double d = QDoubleVector2D::dotProduct(v1, v2);
    QCOMPARE(d, 13.42);

    QCOMPARE(v5 == v6, true);
    QCOMPARE(v5 != v6, false);
    QCOMPARE(v6 == v7, false);
    QCOMPARE(v6 != v7, true);
}



// DoubleVector3D


void tst_doubleVectors::constructor3dTest()
{
    // empty constructor, since it sets to 0, we should check, in case people rely on it
    QDoubleVector3D v1;
    QCOMPARE(v1.x(), 0.0);
    QCOMPARE(v1.y(), 0.0);
    QCOMPARE(v1.z(), 0.0);
    QCOMPARE(v1.isNull(), true);
    v1 = QDoubleVector3D(1.1, -2.5, 3.2); // assignment and constructor
    QCOMPARE(v1.x(), 1.1);
    QCOMPARE(v1.y(), -2.5);
    QCOMPARE(v1.z(), 3.2);
    QDoubleVector3D v2(v1); // copy constructor
    QCOMPARE(v2.x(), 1.1);
    QCOMPARE(v2.y(), -2.5);
    QCOMPARE(v2.z(), 3.2);
    const QDoubleVector2D v2d(2.2, 3.3);
    QDoubleVector3D v3(v2d); // constructor from 3d vector, just copies x and y
    QCOMPARE(v3.x(), 2.2);
    QCOMPARE(v3.y(), 3.3);
    QCOMPARE(v3.z(), 0.0);
    QCOMPARE(v3.isNull(), false);
    const QDoubleVector2D v2d2(2.2, 3.3);
    QDoubleVector3D v4(v2d2, -13.6); // constructor from 2d vector
    QCOMPARE(v4.x(), 2.2);
    QCOMPARE(v4.y(), 3.3);
    QCOMPARE(v4.z(), -13.6);
}

void tst_doubleVectors::basicFunctions3dTest()
{
    QDoubleVector3D v1;
    v1.setX(2.0);
    v1.setY(3.0);
    v1.setZ(6.0);
    QCOMPARE(v1.x(), 2.0);
    QCOMPARE(v1.y(), 3.0);
    QCOMPARE(v1.z(), 6.0);
    QCOMPARE(v1.length(), 7.0);
    QDoubleVector3D v2 = v1.normalized();
    QCOMPARE(v1.lengthSquared(), 49.0);
    v1.normalize();
    QCOMPARE(v1.x(), 2.0/7.0);
    QCOMPARE(v1.y(), 3.0/7.0);
    QCOMPARE(v1.z(), 6.0/7.0);
    QCOMPARE(v2.x(), 2.0/7.0);
    QCOMPARE(v2.y(), 3.0/7.0);
    QCOMPARE(v2.z(), 6.0/7.0);

    QDoubleVector2D v2d = v1.toVector2D();
    QCOMPARE(v2d.x(), 2.0/7.0);
    QCOMPARE(v2d.y(), 3.0/7.0);
}

void tst_doubleVectors::unaryOperator3dTest()
{
    QDoubleVector3D v1(1.1, 2.2, 3.3);
    QDoubleVector3D v2 = -v1;
    QCOMPARE(v2.x(), -1.1);
    QCOMPARE(v2.y(), -2.2);
    QCOMPARE(v2.z(), -3.3);

    v1 *= 2.0;
    QCOMPARE(v1.x(), 2.2);
    QCOMPARE(v1.y(), 4.4);
    QCOMPARE(v1.z(), 6.6);

    v2 /= 2.0;
    QCOMPARE(v2.x(), -0.55);
    QCOMPARE(v2.y(), -1.1);
    QCOMPARE(v2.z(), -1.65);

    v1 += v2;
    QCOMPARE(v1.x(), 1.65);
    QCOMPARE(v1.y(), 3.3);
    QCOMPARE(v1.z(), 4.95);

    v1 -= v2;
    QCOMPARE(v1.x(), 2.2);
    QCOMPARE(v1.y(), 4.4);
    QCOMPARE(v1.z(), 6.6);

    v1 *= v2;
    QCOMPARE(v1.x(), -1.21);
    QCOMPARE(v1.y(), -4.84);
    QCOMPARE(v1.z(), -10.89);
}

void tst_doubleVectors::binaryOperator3dTest()
{
    QDoubleVector3D v1(1.1, 2.2, 3.3);
    QDoubleVector3D v2(3.4, 4.4, 5.5);
    QDoubleVector3D v3 = v1 + v2;
    QCOMPARE(v3.x(), 4.5);
    QCOMPARE(v3.y(), 6.6);
    QCOMPARE(v3.z(), 8.8);

    QDoubleVector3D v4 = v1 - v2;
    QCOMPARE(v4.x(), -2.3);
    QCOMPARE(v4.y(), -2.2);
    QCOMPARE(v4.z(), -2.2);

    QDoubleVector3D v5 = v2 * 2;
    QCOMPARE(v5.x(), 6.8);
    QCOMPARE(v5.y(), 8.8);
    QCOMPARE(v5.z(), 11.0);

    QDoubleVector3D v6 = 2 * v2;
    QCOMPARE(v6.x(), 6.8);
    QCOMPARE(v6.y(), 8.8);
    QCOMPARE(v6.z(), 11.0);

    QDoubleVector3D v7 = v2 / 2;
    QCOMPARE(v7.x(), 1.7);
    QCOMPARE(v7.y(), 2.2);
    QCOMPARE(v7.z(), 2.75);

    double d = QDoubleVector3D::dotProduct(v1, v2);
    QCOMPARE(d, 31.57);

    QCOMPARE(v5 == v6, true);
    QCOMPARE(v5 != v6, false);
    QCOMPARE(v6 == v7, false);
    QCOMPARE(v6 != v7, true);
}

QTEST_APPLESS_MAIN(tst_doubleVectors)

#include "tst_doublevectors.moc"
