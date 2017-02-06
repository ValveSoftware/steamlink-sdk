/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
#include <Qt3DRender/private/qray3d_p.h>

class tst_QRay3D : public QObject
{
    Q_OBJECT
public:
    tst_QRay3D() {}
    ~tst_QRay3D() {}

private Q_SLOTS:
    void create_data();
    void create();
    void projection_data();
    void projection();
    void point_data();
    void point();
    void contains_point_data();
    void contains_point();
    void contains_ray_data();
    void contains_ray();
    void distance_data();
    void distance();
    void compare();
    void dataStream();
    void transform_data();
    void transform();
    void properties();
    void metaTypes();
    void shouldNotAllowNullDirection();
};

// Fix the problem where a compared value happens to be zero (and
// you cannot always predict this, and should not predict it
// since then you produce self-fulling prophecies instead of tests).
// In that case qFuzzyCompare has a completely strict criterion since
// it finds the "fudge factor" by multiplying by zero...
static inline bool fuzzyCompare(float p1, float p2)
{
    float fac = qMin(qAbs(p1), qAbs(p2));
    return (qAbs(p1 - p2) <= (qIsNull(fac) ? 0.00001f : 0.00001f * fac));
}

static inline bool fuzzyCompare(const QVector3D &lhs, const QVector3D &rhs)
{
    if (fuzzyCompare(lhs.x(), rhs.x()) &&
            fuzzyCompare(lhs.y(), rhs.y()) &&
            fuzzyCompare(lhs.z(), rhs.z()))
        return true;
#ifndef QT_NO_DEBUG_STREAM
    qWarning() << "actual:" << lhs;
    qWarning() << "expected:" << rhs;
#endif
    return false;
}

void tst_QRay3D::create_data()
{
    QTest::addColumn<QVector3D>("point");
    QTest::addColumn<QVector3D>("direction");

    // normalized direction vectors
    QTest::newRow("line on x-axis from origin")
            << QVector3D()
            << QVector3D(1.0f, 0.0f, 0.0f);

    QTest::newRow("line parallel -z-axis from 3,3,3")
            << QVector3D(3.0f, 3.0f, 3.0f)
            << QVector3D(0.0f, 0.0f, -1.0f);

    QTest::newRow("vertical line (parallel to y-axis)")
            << QVector3D(0.5f, 0.0f, 0.5f)
            << QVector3D(0.0f, 1.0f, 0.0f);

    QTest::newRow("equidistant from all 3 axes")
            << QVector3D(0.5f, 0.0f, 0.5f)
            << QVector3D(0.57735026919f, 0.57735026919f, 0.57735026919f);

    // non-normalized direction vectors
    QTest::newRow("line on x-axis from origin - B")
            << QVector3D()
            << QVector3D(2.0f, 0.0f, 0.0f);

    QTest::newRow("line parallel -z-axis from 3,3,3 - B")
            << QVector3D(3.0f, 3.0f, 3.0f)
            << QVector3D(0.0f, 0.0f, -0.7f);

    QTest::newRow("vertical line (parallel to y-axis) - B")
            << QVector3D(0.5f, 0.0f, 0.5f)
            << QVector3D(0.0f, 5.3f, 0.0f);

    QTest::newRow("equidistant from all 3 axes - B")
            << QVector3D(0.5f, 0.0f, 0.5f)
            << QVector3D(1.0f, 1.0f, 1.0f);

    QTest::newRow("negative direction")
            << QVector3D(-3.0f, -3.0f, -3.0f)
            << QVector3D(-1.2f, -1.8f, -2.4f);
}

void tst_QRay3D::create()
{
    QFETCH(QVector3D, point);
    QFETCH(QVector3D, direction);
    Qt3DRender::QRay3D ray(point, direction);
    QVERIFY(fuzzyCompare(ray.direction(), direction));
    QVERIFY(fuzzyCompare(ray.origin(), point));

    Qt3DRender::QRay3D ray2;
    QCOMPARE(ray2.origin(), QVector3D(0, 0, 0));
    QCOMPARE(ray2.direction(), QVector3D(0, 0, 1));
    ray2.setOrigin(point);
    ray2.setDirection(direction);
    QVERIFY(fuzzyCompare(ray.direction(), direction));
    QVERIFY(fuzzyCompare(ray.origin(), point));
}

void tst_QRay3D::projection_data()
{
    QTest::addColumn<QVector3D>("point");
    QTest::addColumn<QVector3D>("direction");
    QTest::addColumn<QVector3D>("vector");
    QTest::addColumn<QVector3D>("expected");

    QTest::newRow("line on x-axis from origin")
            << QVector3D()
            << QVector3D(2.0f, 0.0f, 0.0f)
            << QVector3D(0.6f, 0.0f, 0.0f)
            << QVector3D(0.6f, 0.0f, 0.0f);

    QTest::newRow("line parallel -z-axis from 3,3,3")
            << QVector3D(3.0f, 3.0f, 3.0f)
            << QVector3D(0.0f, 0.0f, -0.7f)
            << QVector3D(3.0f, 3.0f, 2.4f)
            << QVector3D(0.0f, 0.0f, 2.4f);

    QTest::newRow("vertical line (parallel to y-axis)")
            << QVector3D(0.5f, 0.0f, 0.5f)
            << QVector3D(0.0f, 5.3f, 0.0f)
            << QVector3D(0.5f, 0.6f, 0.5f)
            << QVector3D(0.0f, 0.6f, 0.0f);

    QTest::newRow("equidistant from all 3 axes, project y-axis (with some z & x)")
            << QVector3D(0.5f, 0.0f, 0.5f)
            << QVector3D(1.0f, 1.0f, 1.0f)
            << QVector3D(0.5f, 5.0f, 0.5f)
            << QVector3D(2.0f, 2.0f, 2.0f);

    QTest::newRow("negative direction line, project +ve y-axis (with some z & x)")
            << QVector3D(-3.0f, -3.0f, -3.0f)
            << QVector3D(-1.2f, -1.8f, -2.4f)
            << QVector3D(0.5f, 5.0f, 0.5f)
            << QVector3D(1.241379261016846f, 1.862068772315979f, 2.48275852203369f);
}

void tst_QRay3D::projection()
{
    QFETCH(QVector3D, point);
    QFETCH(QVector3D, direction);
    QFETCH(QVector3D, vector);
    QFETCH(QVector3D, expected);
    Qt3DRender::QRay3D line(point, direction);
    QVector3D result = line.project(vector);
    QVERIFY(fuzzyCompare(result, expected));
}

void tst_QRay3D::point_data()
{
    QTest::addColumn<QVector3D>("point");
    QTest::addColumn<QVector3D>("direction");
    QTest::addColumn<QVector3D>("point_on_line_pos_0_6");
    QTest::addColumn<QVector3D>("point_on_line_neg_7_2");

    QTest::newRow("line on x-axis from origin")
            << QVector3D()
            << QVector3D(2.0f, 0.0f, 0.0f)
            << QVector3D(1.2f, 0.0f, 0.0f)
            << QVector3D(-14.4f, 0.0f, 0.0f);

    QTest::newRow("line parallel -z-axis from 3,3,3")
            << QVector3D(3.0f, 3.0f, 3.0f)
            << QVector3D(0.0f, 0.0f, -0.7f)
            << QVector3D(3.0f, 3.0f, 2.58f)
            << QVector3D(3.0f, 3.0f, 8.04f);

    QTest::newRow("vertical line (parallel to y-axis)")
            << QVector3D(0.5f, 0.0f, 0.5f)
            << QVector3D(0.0f, 5.3f, 0.0f)
            << QVector3D(0.5f, 3.18f, 0.5f)
            << QVector3D(0.5f, -38.16f, 0.5f);

    QTest::newRow("equidistant from all 3 axes")
            << QVector3D(0.5f, 0.0f, 0.5f)
            << QVector3D(1.0f, 1.0f, 1.0f)
            << QVector3D(1.1f, 0.6f, 1.1f)
            << QVector3D(-6.7f, -7.2f, -6.7f);

    QTest::newRow("negative direction")
            << QVector3D(-3.0f, -3.0f, -3.0f)
            << QVector3D(-1.2f, -1.8f, -2.4f)
            << QVector3D(-3.72f, -4.08f, -4.44f)
            << QVector3D(5.64f, 9.96f, 14.28f);
}

void tst_QRay3D::point()
{
    QFETCH(QVector3D, point);
    QFETCH(QVector3D, direction);
    QFETCH(QVector3D, point_on_line_pos_0_6);
    QFETCH(QVector3D, point_on_line_neg_7_2);
    Qt3DRender::QRay3D line(point, direction);
    QVERIFY(fuzzyCompare(line.point(0.6f), point_on_line_pos_0_6));
    QVERIFY(fuzzyCompare(line.point(-7.2f), point_on_line_neg_7_2));
    QVERIFY(fuzzyCompare(line.projectedDistance(point_on_line_pos_0_6), 0.6f));
    QVERIFY(fuzzyCompare(line.projectedDistance(point_on_line_neg_7_2), -7.2f));
}

void tst_QRay3D::contains_point_data()
{
    QTest::addColumn<QVector3D>("origin");
    QTest::addColumn<QVector3D>("direction");
    QTest::addColumn<QVector3D>("point");
    QTest::addColumn<bool>("contains");

    QTest::newRow("bogus this line with null direction")
            << QVector3D(1.0, 3.0, 3.0)
            << QVector3D(0.0, 0.0, 0.0)
            << QVector3D(1.0, 2.0, 4.0)
            << false;

    QTest::newRow("point at the origin")
            << QVector3D(0.0, 0.0, 0.0)
            << QVector3D(1.0, 3.0, 3.0)
            << QVector3D(0.0, 0.0, 0.0)
            << true;

    QTest::newRow("close to the origin")
            << QVector3D(1.0, 1.0, 1.0)
            << QVector3D(1.0, 3.0, 3.0)
            << QVector3D(1.0005f, 1.0005f, 1.0)
            << false;

    QTest::newRow("45 line line in plane x=1")
            << QVector3D(1.0, 3.0, 3.0)
            << QVector3D(0.0, -1.0, -1.0)
            << QVector3D(1.0, 4.0, 4.0)
            << true;
    {
        // This is to prove that the constructed approach give the
        // same results
        QVector3D p(1.0, 3.0, 3.0);
        QVector3D v(0.0, -1.0, -1.0);

        QTest::newRow("constructed 45 line line in plane x=1")
                << p
                << v
                << p + v
                << true;
    }

    QTest::newRow("intersection with negative s in plane z=-1")
        << QVector3D(1.0f, 2.0f, -1.0f)
        << QVector3D(1.0f, 1.0f, 0.0f)
        << QVector3D(2.0f, 1.0f, 0.0f)
        << false;

    QTest::newRow("45 angled line")
        << QVector3D(3.0f, 0.0f, -1.0f)
        << QVector3D(1.0f, -1.0f, 1.0f)
        << QVector3D(6.0f, -3.0f, 2.0f)
        << true;

    {
        QVector3D p(-10.0, 3.0, 3.0);
        QVector3D v(0.0, 20.0, -1.0);
        QTest::newRow("constructed vector close to axis")
                << p
                << v
                << p + v * 3.0
                << true;
    }

    {
        QVector3D p(1.0, 3.0, 3.0);
        QVector3D v(40.0, 500.0, -1.0);
        QTest::newRow("constructed larger values close to axis")
                << p
                << v
                << p + v
                << true;
    }
}

void tst_QRay3D::contains_point()
{
    QFETCH(QVector3D, origin);
    QFETCH(QVector3D, direction);
    QFETCH(QVector3D, point);
    QFETCH(bool, contains);

    Qt3DRender::QRay3D line(origin, direction);
    QCOMPARE(line.contains(point), contains);
}

void tst_QRay3D::contains_ray_data()
{
    contains_point_data();
}

void tst_QRay3D::contains_ray()
{
    QFETCH(QVector3D, origin);
    QFETCH(QVector3D, direction);
    QFETCH(QVector3D, point);
    QFETCH(bool, contains);

    Qt3DRender::QRay3D line(origin, direction);
    if (contains) {
        Qt3DRender::QRay3D line2(point, direction);
        QVERIFY(line.contains(line2));
        QVERIFY(line2.contains(line));

        // Reversed direction is also contained.
        Qt3DRender::QRay3D line3(point, -direction);
        QVERIFY(line.contains(line2));
        QVERIFY(line2.contains(line));

        // Different direction.
        Qt3DRender::QRay3D line4(point, QVector3D(direction.y(), direction.x(), direction.z()));
        QVERIFY(!line.contains(line4));
        QVERIFY(!line4.contains(line));
    } else {
        Qt3DRender::QRay3D line2(point, direction);
        QVERIFY(!line.contains(line2));
        QVERIFY(!line2.contains(line));
    }
}

void tst_QRay3D::distance_data()
{
    QTest::addColumn<QVector3D>("origin");
    QTest::addColumn<QVector3D>("direction");
    QTest::addColumn<QVector3D>("point");
    QTest::addColumn<float>("distance");

    QTest::newRow("axis-x")
        << QVector3D(6.0f, 0.0f, 0.0f)
        << QVector3D(1.0f, 0.0f, 0.0f)
        << QVector3D(0.0f, 0.0f, 0.0f)
        << 0.0f;

    QTest::newRow("axis-x to 1")
        << QVector3D(6.0f, 0.0f, 0.0f)
        << QVector3D(1.0f, 0.0f, 0.0f)
        << QVector3D(0.0f, 1.0f, 0.0f)
        << 1.0f;

    QTest::newRow("neg-axis-y")
        << QVector3D(0.0f, 6.0f, 0.0f)
        << QVector3D(0.0f, -1.5f, 0.0f)
        << QVector3D(0.0f, 100.0f, 0.0f)
        << 0.0f;

    QTest::newRow("neg-axis-y to 2")
        << QVector3D(0.0f, 6.0f, 0.0f)
        << QVector3D(0.0f, -1.5f, 0.0f)
        << QVector3D(2.0f, 0.0f, 0.0f)
        << 2.0f;
}

void tst_QRay3D::distance()
{
    QFETCH(QVector3D, origin);
    QFETCH(QVector3D, direction);
    QFETCH(QVector3D, point);
    QFETCH(float, distance);

    Qt3DRender::QRay3D line(origin, direction);
    QCOMPARE(line.distance(point), distance);
}

void tst_QRay3D::compare()
{
    Qt3DRender::QRay3D ray1(QVector3D(10, 20, 30), QVector3D(-3, -4, -5));
    Qt3DRender::QRay3D ray2(QVector3D(10, 20, 30), QVector3D(1.5f, 2.0f, 2.5f));
    Qt3DRender::QRay3D ray3(QVector3D(0, 20, 30), QVector3D(-3, -4, -5));
    QVERIFY(ray1 == ray1);
    QVERIFY(!(ray1 != ray1));
    QVERIFY(qFuzzyCompare(ray1, ray1));
    QVERIFY(ray1 != ray2);
    QVERIFY(!(ray1 == ray2));
    QVERIFY(!qFuzzyCompare(ray1, ray2));
    QVERIFY(ray1 != ray3);
    QVERIFY(!(ray1 == ray3));
    QVERIFY(!qFuzzyCompare(ray1, ray3));
}

void tst_QRay3D::dataStream()
{
#ifndef QT_NO_DATASTREAM
    Qt3DRender::QRay3D ray(QVector3D(1.0f, 2.0f, 3.0f), QVector3D(4.0f, 5.0f, 6.0f));

    QByteArray data;
    {
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << ray;
    }

    Qt3DRender::QRay3D ray2;
    {
        QDataStream stream2(data);
        stream2 >> ray2;
    }

    QVERIFY(ray == ray2);
#endif
}

void tst_QRay3D::transform_data()
{
    create_data();
}

void tst_QRay3D::transform()
{
    QFETCH(QVector3D, point);
    QFETCH(QVector3D, direction);

    QMatrix4x4 m;
    m.translate(-1.0f, 2.5f, 5.0f);
    m.rotate(45.0f, 1.0f, 1.0f, 1.0f);
    m.scale(23.5f);

    Qt3DRender::QRay3D ray1(point, direction);
    Qt3DRender::QRay3D ray2(ray1);
    Qt3DRender::QRay3D ray3;

    ray1.transform(m);
    ray3 = ray2.transformed(m);

    QVERIFY(fuzzyCompare(ray1.origin(), ray3.origin()));
    QVERIFY(fuzzyCompare(ray1.direction(), ray3.direction()));

    QVERIFY(fuzzyCompare(ray1.origin(), m * point));
    QVERIFY(fuzzyCompare(ray1.direction(), m.mapVector(direction)));
}

class tst_QRay3DProperties : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Qt3DRender::QRay3D ray READ ray WRITE setRay)
public:
    tst_QRay3DProperties(QObject *parent = 0) : QObject(parent) {}

    Qt3DRender::QRay3D ray() const { return r; }
    void setRay(const Qt3DRender::QRay3D& value) { r = value; }

private:
    Qt3DRender::QRay3D r;
};

// Test getting and setting properties via the metaobject system.
void tst_QRay3D::properties()
{
    tst_QRay3DProperties obj;

    qRegisterMetaType<Qt3DRender::QRay3D>();

    obj.setRay(Qt3DRender::QRay3D(QVector3D(1, 2, 3), QVector3D(4, 5, 6)));

    Qt3DRender::QRay3D r = qvariant_cast<Qt3DRender::QRay3D>(obj.property("ray"));
    QCOMPARE(r.origin(), QVector3D(1, 2, 3));
    QCOMPARE(r.direction(), QVector3D(4, 5, 6));

    obj.setProperty("ray",
                    qVariantFromValue
                        (Qt3DRender::QRay3D(QVector3D(-1, -2, -3), QVector3D(-4, -5, -6))));

    r = qvariant_cast<Qt3DRender::QRay3D>(obj.property("ray"));
    QCOMPARE(r.origin(), QVector3D(-1, -2, -3));
    QCOMPARE(r.direction(), QVector3D(-4, -5, -6));
}

void tst_QRay3D::metaTypes()
{
    int id = qMetaTypeId<Qt3DRender::QRay3D>();
    QVERIFY(QMetaType::type("Qt3DRender::QRay3D") == id);
    QCOMPARE(QByteArray(QMetaType::typeName(id)), QByteArray("Qt3DRender::QRay3D"));
    QVERIFY(QMetaType::isRegistered(id));
}

void tst_QRay3D::shouldNotAllowNullDirection()
{
    // GIVEN
    Qt3DRender::QRay3D ray;

    QCOMPARE(ray.origin(), QVector3D(0, 0, 0));
    QCOMPARE(ray.direction(), QVector3D(0, 0, 1));

    // WHEN
    ray.setDirection(QVector3D(0, 0, 0));

    // THEN
    QCOMPARE(ray.direction(), QVector3D(0, 0, 1));
}

QTEST_APPLESS_MAIN(tst_QRay3D)

#include "tst_qray3d.moc"
