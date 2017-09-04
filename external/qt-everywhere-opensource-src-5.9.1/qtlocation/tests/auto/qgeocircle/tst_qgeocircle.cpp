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

#include <QtTest/QtTest>
#include <QtPositioning/QGeoCoordinate>
#include <QtPositioning/QGeoCircle>
#include <QtPositioning/QGeoRectangle>

QT_USE_NAMESPACE

class tst_QGeoCircle : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructor();
    void centerRadiusConstructor();
    void assignment();

    void comparison();
    void type();

    void radius();
    void center();

    void translate_data();
    void translate();

    void valid_data();
    void valid();

    void empty_data();
    void empty();

    void contains_data();
    void contains();

    void boundingGeoRectangle_data();
    void boundingGeoRectangle();

    void extendCircle();
    void extendCircle_data();

    void areaComparison();
    void areaComparison_data();

    void boxComparison();
    void boxComparison_data();
};

void tst_QGeoCircle::defaultConstructor()
{
    QGeoCircle c;
    QVERIFY(!c.center().isValid());
    QCOMPARE(c.radius(), qreal(-1.0));
}

void tst_QGeoCircle::centerRadiusConstructor()
{
    QGeoCircle c(QGeoCoordinate(1,1), qreal(50.0));
    QCOMPARE(c.center(), QGeoCoordinate(1,1));
    QCOMPARE(c.radius(), qreal(50.0));
}

void tst_QGeoCircle::assignment()
{
    QGeoCircle c1 = QGeoCircle(QGeoCoordinate(10.0, 0.0), 20.0);
    QGeoCircle c2 = QGeoCircle(QGeoCoordinate(20.0, 0.0), 30.0);

    QVERIFY(c1 != c2);

    c2 = c1;
    QCOMPARE(c2.center(), QGeoCoordinate(10.0, 0.0));
    QCOMPARE(c2.radius(), 20.0);
    QCOMPARE(c1, c2);

    c2.setCenter(QGeoCoordinate(30.0, 0.0));
    c2.setRadius(15.0);
    QCOMPARE(c1.center(), QGeoCoordinate(10.0, 0.0));
    QCOMPARE(c1.radius(), 20.0);

    // Assign c1 to an area
    QGeoShape area = c1;
    QCOMPARE(area.type(), c1.type());
    QVERIFY(area == c1);

    // Assign the area back to a bounding circle
    QGeoCircle ca = area;
    QCOMPARE(ca.center(), c1.center());
    QCOMPARE(ca.radius(), c1.radius());

    // Check that the copy is not modified when modifying the original.
    c1.setCenter(QGeoCoordinate(15.0, 15.0));
    QVERIFY(ca.center() != c1.center());
    QVERIFY(ca != c1);
}

void tst_QGeoCircle::comparison()
{
    QGeoCircle c1(QGeoCoordinate(1,1), qreal(50.0));
    QGeoCircle c2(QGeoCoordinate(1,1), qreal(50.0));
    QGeoCircle c3(QGeoCoordinate(1,1), qreal(35.0));
    QGeoCircle c4(QGeoCoordinate(1,2), qreal(50.0));

    QVERIFY(c1 == c2);
    QVERIFY(!(c1 != c2));

    QVERIFY(!(c1 == c3));
    QVERIFY(c1 != c3);

    QVERIFY(!(c1 == c4));
    QVERIFY(c1 != c4);

    QVERIFY(!(c2 == c3));
    QVERIFY(c2 != c3);

    QGeoRectangle b1(QGeoCoordinate(20,20),QGeoCoordinate(10,30));
    QVERIFY(!(c1 == b1));
    QVERIFY(c1 != b1);

    QGeoShape *c2Ptr = &c2;
    QVERIFY(c1 == *c2Ptr);
    QVERIFY(!(c1 != *c2Ptr));

    QGeoShape *c3Ptr = &c3;
    QVERIFY(!(c1 == *c3Ptr));
    QVERIFY(c1 != *c3Ptr);
}

void tst_QGeoCircle::type()
{
    QGeoCircle c;
    QCOMPARE(c.type(), QGeoShape::CircleType);
}

void tst_QGeoCircle::radius()
{
    QGeoCircle c;
    c.setRadius(1.0);
    QCOMPARE(c.radius(), qreal(1.0));
    c.setRadius(5.0);
    QCOMPARE(c.radius(), qreal(5.0));
}

void tst_QGeoCircle::center()
{
    QGeoCircle c;
    c.setCenter(QGeoCoordinate(1,1));
    QCOMPARE(c.center(), QGeoCoordinate(1,1));

    QGeoShape shape = c;
    QCOMPARE(shape.center(), c.center());

    c.setCenter(QGeoCoordinate(5,10));
    QCOMPARE(c.center(), QGeoCoordinate(5,10));
}

void tst_QGeoCircle::translate_data()
{
    QTest::addColumn<QGeoCoordinate>("center");
    QTest::addColumn<qreal>("radius");
    QTest::addColumn<double>("lat");
    QTest::addColumn<double>("lon");
    QTest::addColumn<QGeoCoordinate>("newCenter");

    QTest::newRow("from 0,0") << QGeoCoordinate(0,0) << qreal(10.0) <<
                                 5.0 << 5.0 << QGeoCoordinate(5.0, 5.0);
    QTest::newRow("across 0,0") << QGeoCoordinate(-2, -2) << qreal(20.0) <<
                                   5.0 << 5.0 << QGeoCoordinate(3.0, 3.0);
    QTest::newRow("backwards across 0,0") << QGeoCoordinate(5,5) << qreal(50.0)
                                              << -13.0 << 5.0
                                              << QGeoCoordinate(-8.0, 10.0);
}

void tst_QGeoCircle::translate()
{
    QFETCH(QGeoCoordinate, center);
    QFETCH(qreal, radius);
    QFETCH(double, lat);
    QFETCH(double, lon);
    QFETCH(QGeoCoordinate, newCenter);

    QGeoCircle c(center, radius);
    QGeoCircle d = c;

    c.translate(lat, lon);

    QCOMPARE(c.radius(), radius);
    QCOMPARE(c.center(), newCenter);

    c = d.translated(lat, lon);
    d.setRadius(1.0);

    QCOMPARE(c.radius(), radius);
    QCOMPARE(d.center(), center);
    QCOMPARE(c.center(), newCenter);
}

void tst_QGeoCircle::valid_data()
{
    QTest::addColumn<QGeoCoordinate>("center");
    QTest::addColumn<qreal>("radius");
    QTest::addColumn<bool>("valid");

    QTest::newRow("default") << QGeoCoordinate() << qreal(-1.0) << false;
    QTest::newRow("empty coord") << QGeoCoordinate() << qreal(5.0) << false;
    QTest::newRow("NaN coord") << QGeoCoordinate(500, 500) << qreal(5.0) << false;
    QTest::newRow("bad radius") << QGeoCoordinate(10, 10) << qreal(-5.0) << false;
    QTest::newRow("NaN radius") << QGeoCoordinate(10, 10) << qreal(qQNaN()) << false;
    QTest::newRow("zero radius") << QGeoCoordinate(10, 10) << qreal(0.0) << true;
    QTest::newRow("good") << QGeoCoordinate(10, 10) << qreal(5.0) << true;
}

void tst_QGeoCircle::valid()
{
    QFETCH(QGeoCoordinate, center);
    QFETCH(qreal, radius);
    QFETCH(bool, valid);

    QGeoCircle c(center, radius);
    QCOMPARE(c.isValid(), valid);

    QGeoShape area = c;
    QCOMPARE(area.isValid(), valid);
}

void tst_QGeoCircle::empty_data()
{
    QTest::addColumn<QGeoCoordinate>("center");
    QTest::addColumn<qreal>("radius");
    QTest::addColumn<bool>("empty");

    QTest::newRow("default") << QGeoCoordinate() << qreal(-1.0) << true;
    QTest::newRow("empty coord") << QGeoCoordinate() << qreal(5.0) << true;
    QTest::newRow("NaN coord") << QGeoCoordinate(500, 500) << qreal(5.0) << true;
    QTest::newRow("bad radius") << QGeoCoordinate(10, 10) << qreal(-5.0) << true;
    QTest::newRow("NaN radius") << QGeoCoordinate(10, 10) << qreal(qQNaN()) << true;
    QTest::newRow("zero radius") << QGeoCoordinate(10, 10) << qreal(0.0) << true;
    QTest::newRow("good") << QGeoCoordinate(10, 10) << qreal(5.0) << false;
}

void tst_QGeoCircle::empty()
{
    QFETCH(QGeoCoordinate, center);
    QFETCH(qreal, radius);
    QFETCH(bool, empty);

    QGeoCircle c(center, radius);
    QCOMPARE(c.isEmpty(), empty);

    QGeoShape area = c;
    QCOMPARE(area.isEmpty(), empty);
}

void tst_QGeoCircle::contains_data()
{
    QTest::addColumn<QGeoCoordinate>("center");
    QTest::addColumn<qreal>("radius");
    QTest::addColumn<QGeoCoordinate>("probe");
    QTest::addColumn<bool>("result");

    QTest::newRow("own center") << QGeoCoordinate(1,1) << qreal(100.0) <<
                                   QGeoCoordinate(1,1) << true;
    QTest::newRow("over the hills") << QGeoCoordinate(1,1) << qreal(100.0) <<
                                       QGeoCoordinate(30, 40) << false;
    QTest::newRow("at 0.5*radius") << QGeoCoordinate(1,1) << qreal(100.0) <<
                                      QGeoCoordinate(1.00015374,1.00015274) << true;
    QTest::newRow("at 0.99*radius") << QGeoCoordinate(1,1) << qreal(100.0) <<
                                       QGeoCoordinate(1.00077538, 0.99955527) << true;
    QTest::newRow("at 1.01*radius") << QGeoCoordinate(1,1) << qreal(100.0) <<
                                       QGeoCoordinate(1.00071413, 0.99943423) << false;
    // TODO: add tests for edge circle cases: cross 1 pole, cross both poles
}

void tst_QGeoCircle::contains()
{
    QFETCH(QGeoCoordinate, center);
    QFETCH(qreal, radius);
    QFETCH(QGeoCoordinate, probe);
    QFETCH(bool, result);

    QGeoCircle c(center, radius);
    QCOMPARE(c.contains(probe), result);

    QGeoShape area = c;
    QCOMPARE(area.contains(probe), result);
}

void tst_QGeoCircle::boundingGeoRectangle_data()
{
    QTest::addColumn<QGeoCoordinate>("center");
    QTest::addColumn<qreal>("radius");
    QTest::addColumn<QGeoCoordinate>("probe");
    QTest::addColumn<bool>("result");

    QTest::newRow("own center") << QGeoCoordinate(1,1) << qreal(100.0) <<
                                   QGeoCoordinate(1,1) << true;
    QTest::newRow("over the hills") << QGeoCoordinate(1,1) << qreal(100.0) <<
                                       QGeoCoordinate(30, 40) << false;
    QTest::newRow("at 0.5*radius") << QGeoCoordinate(1,1) << qreal(100.0) <<
                                      QGeoCoordinate(1.00015374,1.00015274) << true;
    QTest::newRow("at 0.99*radius") << QGeoCoordinate(1,1) << qreal(100.0) <<
                                       QGeoCoordinate(1.00077538, 0.99955527) << true;
    QTest::newRow("Outside the box") << QGeoCoordinate(1,1) << qreal(100.0) <<
                                       QGeoCoordinate(1.00071413, 0.99903423) << false;
    // TODO: add tests for edge circle cases: cross 1 pole, cross both poles
}

void tst_QGeoCircle::boundingGeoRectangle()
{
    QFETCH(QGeoCoordinate, center);
    QFETCH(qreal, radius);
    QFETCH(QGeoCoordinate, probe);
    QFETCH(bool, result);

    QGeoCircle c(center, radius);
    QGeoRectangle box = c.boundingGeoRectangle();
    QCOMPARE(box.contains(probe), result);
}

void tst_QGeoCircle::extendCircle()
{
    QFETCH(QGeoCircle, circle);
    QFETCH(QGeoCoordinate, coord);
    QFETCH(bool, containsFirst);
    QFETCH(bool, containsExtended);

    QCOMPARE(circle.contains(coord), containsFirst);
    circle.extendCircle(coord);
    QCOMPARE(circle.contains(coord), containsExtended);

}

void tst_QGeoCircle::extendCircle_data()
{
    QTest::addColumn<QGeoCircle>("circle");
    QTest::addColumn<QGeoCoordinate>("coord");
    QTest::addColumn<bool>("containsFirst");
    QTest::addColumn<bool>("containsExtended");

    QGeoCoordinate co1(20.0, 20.0);

    QTest::newRow("own center")
            << QGeoCircle(co1, 100)
            << QGeoCoordinate(20.0, 20.0)
            << true
            << true;
    QTest::newRow("inside")
            << QGeoCircle(co1, 100)
            << QGeoCoordinate(20.0001, 20.0001)
            << true
            << true;
    QTest::newRow("far away")
            << QGeoCircle(co1, 100)
            << QGeoCoordinate(50.0001, 50.0001)
            << false
            << true;
    QTest::newRow("invalid circle")
            << QGeoCircle()
            << QGeoCoordinate(20.0, 20.0)
            << false
            << false;
    QTest::newRow("invalid coordinate")
            << QGeoCircle(co1, 100)
            << QGeoCoordinate(99.0, 190.0)
            << false
            << false;
}

void tst_QGeoCircle::areaComparison_data()
{
    QTest::addColumn<QGeoShape>("area");
    QTest::addColumn<QGeoCircle>("circle");
    QTest::addColumn<bool>("equal");

    QGeoCircle c1(QGeoCoordinate(10.0, 0.0), 10.0);
    QGeoCircle c2(QGeoCoordinate(20.0, 10.0), 20.0);
    QGeoRectangle b(QGeoCoordinate(10.0, 0.0), QGeoCoordinate(0.0, 10.0));

    QTest::newRow("default constructed") << QGeoShape() << QGeoCircle() << false;
    QTest::newRow("c1 c1") << QGeoShape(c1) << c1 << true;
    QTest::newRow("c1 c2") << QGeoShape(c1) << c2 << false;
    QTest::newRow("c2 c1") << QGeoShape(c2) << c1 << false;
    QTest::newRow("c2 c2") << QGeoShape(c2) << c2 << true;
    QTest::newRow("b c1") << QGeoShape(b) << c1 << false;
}

void tst_QGeoCircle::areaComparison()
{
    QFETCH(QGeoShape, area);
    QFETCH(QGeoCircle, circle);
    QFETCH(bool, equal);

    QCOMPARE((area == circle), equal);
    QCOMPARE((area != circle), !equal);

    QCOMPARE((circle == area), equal);
    QCOMPARE((circle != area), !equal);
}

void tst_QGeoCircle::boxComparison_data()
{
    QTest::addColumn<QGeoRectangle>("box");
    QTest::addColumn<QGeoCircle>("circle");
    QTest::addColumn<bool>("equal");

    QGeoCircle c(QGeoCoordinate(10.0, 0.0), 10.0);
    QGeoRectangle b(QGeoCoordinate(10.0, 0.0), QGeoCoordinate(0.0, 10.0));

    QTest::newRow("default constructed") << QGeoRectangle() << QGeoCircle() << false;
    QTest::newRow("b c") << b << c << false;
}

void tst_QGeoCircle::boxComparison()
{
    QFETCH(QGeoRectangle, box);
    QFETCH(QGeoCircle, circle);
    QFETCH(bool, equal);

    QCOMPARE((box == circle), equal);
    QCOMPARE((box != circle), !equal);

    QCOMPARE((circle == box), equal);
    QCOMPARE((circle != box), !equal);
}

QTEST_MAIN(tst_QGeoCircle)
#include "tst_qgeocircle.moc"
