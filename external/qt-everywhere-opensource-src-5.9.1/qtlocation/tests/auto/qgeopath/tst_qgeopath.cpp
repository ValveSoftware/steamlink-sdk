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
#include <QtPositioning/QGeoRectangle>
#include <QtPositioning/QGeoPath>

QT_USE_NAMESPACE

class tst_QGeoPath : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructor();
    void listConstructor();
    void assignment();

    void comparison();
    void type();

    void path();
    void width();

    void translate_data();
    void translate();

    void valid_data();
    void valid();

    void contains_data();
    void contains();

    void boundingGeoRectangle_data();
    void boundingGeoRectangle();

    void extendShape();
    void extendShape_data();
};

void tst_QGeoPath::defaultConstructor()
{
    QGeoPath p;
    QVERIFY(!p.path().size());
    QCOMPARE(p.width(), qreal(0.0));
}

void tst_QGeoPath::listConstructor()
{
    QList<QGeoCoordinate> coords;
    coords.append(QGeoCoordinate(1,1));
    coords.append(QGeoCoordinate(2,2));
    coords.append(QGeoCoordinate(3,0));

    QGeoPath p(coords, 1.0);
    QCOMPARE(p.width(), qreal(1.0));
    QCOMPARE(p.path().size(), 3);

    for (const QGeoCoordinate &c : coords) {
        QCOMPARE(p.path().contains(c), true);
    }
}

void tst_QGeoPath::assignment()
{
    QGeoPath p1;
    QList<QGeoCoordinate> coords;
    coords.append(QGeoCoordinate(1,1));
    coords.append(QGeoCoordinate(2,2));
    coords.append(QGeoCoordinate(3,0));
    QGeoPath p2(coords, 1.0);

    QVERIFY(p1 != p2);

    p1 = p2;
    QCOMPARE(p1.path(), coords);
    QCOMPARE(p1.width(), 1.0);
    QCOMPARE(p1, p2);

    // Assign c1 to an area
    QGeoShape area = p1;
    QCOMPARE(area.type(), p1.type());
    QVERIFY(area == p1);

    // Assign the area back to a bounding circle
    QGeoPath p3 = area;
    QCOMPARE(p3.path(), coords);
    QCOMPARE(p3.width(), 1.0);

    // Check that the copy is not modified when modifying the original.
    p1.setWidth(2.0);
    QVERIFY(p3.width() != p1.width());
    QVERIFY(p3 != p1);
}

void tst_QGeoPath::comparison()
{
    QList<QGeoCoordinate> coords;
    coords.append(QGeoCoordinate(1,1));
    coords.append(QGeoCoordinate(2,2));
    coords.append(QGeoCoordinate(3,0));
    QList<QGeoCoordinate> coords2;
    coords2.append(QGeoCoordinate(3,1));
    coords2.append(QGeoCoordinate(4,2));
    coords2.append(QGeoCoordinate(3,0));
    QGeoPath c1(coords, qreal(50.0));
    QGeoPath c2(coords, qreal(50.0));
    QGeoPath c3(coords, qreal(35.0));
    QGeoPath c4(coords2, qreal(50.0));

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

void tst_QGeoPath::type()
{
    QGeoPath c;
    QCOMPARE(c.type(), QGeoShape::PathType);
}

void tst_QGeoPath::path()
{
    QList<QGeoCoordinate> coords;
    coords.append(QGeoCoordinate(1,1));
    coords.append(QGeoCoordinate(2,2));
    coords.append(QGeoCoordinate(3,0));

    QGeoPath p;
    p.setPath(coords);
    QCOMPARE(p.path().size(), 3);

    for (const QGeoCoordinate &c : coords) {
        QCOMPARE(p.path().contains(c), true);
    }
}

void tst_QGeoPath::width()
{
    QGeoPath p;
    p.setWidth(10.0);
    QCOMPARE(p.width(), qreal(10.0));
}

void tst_QGeoPath::translate_data()
{
    QTest::addColumn<QGeoCoordinate>("c1");
    QTest::addColumn<QGeoCoordinate>("c2");
    QTest::addColumn<QGeoCoordinate>("c3");
    QTest::addColumn<double>("lat");
    QTest::addColumn<double>("lon");

    QTest::newRow("Simple") << QGeoCoordinate(1,1) << QGeoCoordinate(2,2) <<
                                 QGeoCoordinate(3,0) << 5.0 << 4.0;
    QTest::newRow("Backward") << QGeoCoordinate(1,1) << QGeoCoordinate(2,2) <<
                                 QGeoCoordinate(3,0) << -5.0 << -4.0;
}

void tst_QGeoPath::translate()
{
    QFETCH(QGeoCoordinate, c1);
    QFETCH(QGeoCoordinate, c2);
    QFETCH(QGeoCoordinate, c3);
    QFETCH(double, lat);
    QFETCH(double, lon);

    QList<QGeoCoordinate> coords;
    coords.append(c1);
    coords.append(c2);
    coords.append(c3);
    QGeoPath p(coords);

    p.translate(lat, lon);

    for (int i = 0; i < p.path().size(); i++) {
        QCOMPARE(coords[i].latitude(), p.path()[i].latitude() - lat );
        QCOMPARE(coords[i].longitude(), p.path()[i].longitude() - lon );
    }
}

void tst_QGeoPath::valid_data()
{
    QTest::addColumn<QGeoCoordinate>("c1");
    QTest::addColumn<QGeoCoordinate>("c2");
    QTest::addColumn<QGeoCoordinate>("c3");
    QTest::addColumn<qreal>("width");
    QTest::addColumn<bool>("valid");

    QTest::newRow("empty coords") << QGeoCoordinate() << QGeoCoordinate() << QGeoCoordinate() << qreal(5.0) << false;
    QTest::newRow("invalid coord") << QGeoCoordinate(50, 50) << QGeoCoordinate(60, 60) << QGeoCoordinate(700, 700) << qreal(5.0) << false;
    QTest::newRow("bad width") << QGeoCoordinate(10, 10) << QGeoCoordinate(11, 11) << QGeoCoordinate(10, 12) << qreal(-5.0) << true;
    QTest::newRow("NaN width") << QGeoCoordinate(10, 10) << QGeoCoordinate(11, 11) << QGeoCoordinate(10, 12) << qreal(qQNaN()) << true;
    QTest::newRow("zero width") << QGeoCoordinate(10, 10) << QGeoCoordinate(11, 11) << QGeoCoordinate(10, 12) << qreal(0) << true;
    QTest::newRow("good") << QGeoCoordinate(10, 10) << QGeoCoordinate(11, 11) << QGeoCoordinate(10, 12) << qreal(5) << true;
}

void tst_QGeoPath::valid()
{
    QFETCH(QGeoCoordinate, c1);
    QFETCH(QGeoCoordinate, c2);
    QFETCH(QGeoCoordinate, c3);
    QFETCH(qreal, width);
    QFETCH(bool, valid);

    QList<QGeoCoordinate> coords;
    coords.append(c1);
    coords.append(c2);
    coords.append(c3);
    QGeoPath p(coords, width);

    QCOMPARE(p.isValid(), valid);

    QGeoShape area = p;
    QCOMPARE(area.isValid(), valid);
}

void tst_QGeoPath::contains_data()
{
    QTest::addColumn<QGeoCoordinate>("c1");
    QTest::addColumn<QGeoCoordinate>("c2");
    QTest::addColumn<QGeoCoordinate>("c3");
    QTest::addColumn<qreal>("width");
    QTest::addColumn<QGeoCoordinate>("probe");
    QTest::addColumn<bool>("result");

    QList<QGeoCoordinate> c;
    c.append(QGeoCoordinate(1,1));
    c.append(QGeoCoordinate(2,2));
    c.append(QGeoCoordinate(3,0));

    QTest::newRow("One of the points") << c[0] << c[1] << c[2] << 0.0 << QGeoCoordinate(2, 2) << true;
    QTest::newRow("Not so far away") << c[0] << c[1] << c[2] << 0.0 << QGeoCoordinate(0.8, 0.8) << false;
    QTest::newRow("Not so far away and large line") << c[0] << c[1] << c[2] << 100000.0 << QGeoCoordinate(0.8, 0.8) << true;
}

void tst_QGeoPath::contains()
{
    QFETCH(QGeoCoordinate, c1);
    QFETCH(QGeoCoordinate, c2);
    QFETCH(QGeoCoordinate, c3);
    QFETCH(qreal, width);
    QFETCH(QGeoCoordinate, probe);
    QFETCH(bool, result);

    QList<QGeoCoordinate> coords;
    coords.append(c1);
    coords.append(c2);
    coords.append(c3);
    QGeoPath p(coords, width);

    QCOMPARE(p.contains(probe), result);

    QGeoShape area = p;
    QCOMPARE(area.contains(probe), result);
}

void tst_QGeoPath::boundingGeoRectangle_data()
{
    QTest::addColumn<QGeoCoordinate>("c1");
    QTest::addColumn<QGeoCoordinate>("c2");
    QTest::addColumn<QGeoCoordinate>("c3");
    QTest::addColumn<qreal>("width");
    QTest::addColumn<QGeoCoordinate>("probe");
    QTest::addColumn<bool>("result");

    QList<QGeoCoordinate> c;
    c.append(QGeoCoordinate(1,1));
    c.append(QGeoCoordinate(2,2));
    c.append(QGeoCoordinate(3,0));

    QTest::newRow("One of the points") << c[0] << c[1] << c[2] << 0.0 << QGeoCoordinate(2, 2) << true;
    QTest::newRow("Not so far away") << c[0] << c[1] << c[2] << 0.0 << QGeoCoordinate(0, 0) << false;
    QTest::newRow("Inside the bounds") << c[0] << c[1] << c[2] << 100.0 << QGeoCoordinate(1, 0) << true;
    QTest::newRow("Inside the bounds") << c[0] << c[1] << c[2] << 100.0 << QGeoCoordinate(1.1, 0.1) << true;
}

void tst_QGeoPath::boundingGeoRectangle()
{
    QFETCH(QGeoCoordinate, c1);
    QFETCH(QGeoCoordinate, c2);
    QFETCH(QGeoCoordinate, c3);
    QFETCH(qreal, width);
    QFETCH(QGeoCoordinate, probe);
    QFETCH(bool, result);

    QList<QGeoCoordinate> coords;
    coords.append(c1);
    coords.append(c2);
    coords.append(c3);
    QGeoPath p(coords, width);

    QGeoRectangle box = p.boundingGeoRectangle();
    QCOMPARE(box.contains(probe), result);
}

void tst_QGeoPath::extendShape()
{
    QFETCH(QGeoCoordinate, c1);
    QFETCH(QGeoCoordinate, c2);
    QFETCH(QGeoCoordinate, c3);
    QFETCH(qreal, width);
    QFETCH(QGeoCoordinate, probe);
    QFETCH(bool, before);
    QFETCH(bool, after);

    QList<QGeoCoordinate> coords;
    coords.append(c1);
    coords.append(c2);
    coords.append(c3);
    QGeoPath p(coords, width);


    QCOMPARE(p.contains(probe), before);
    p.extendShape(probe);
    QCOMPARE(p.contains(probe), after);
}

void tst_QGeoPath::extendShape_data()
{
    QTest::addColumn<QGeoCoordinate>("c1");
    QTest::addColumn<QGeoCoordinate>("c2");
    QTest::addColumn<QGeoCoordinate>("c3");
    QTest::addColumn<qreal>("width");
    QTest::addColumn<QGeoCoordinate>("probe");
    QTest::addColumn<bool>("before");
    QTest::addColumn<bool>("after");

    QList<QGeoCoordinate> c;
    c.append(QGeoCoordinate(1,1));
    c.append(QGeoCoordinate(2,2));
    c.append(QGeoCoordinate(3,0));

    QTest::newRow("One of the points") << c[0] << c[1] << c[2] << 0.0 << QGeoCoordinate(2, 2) << true << true;
    QTest::newRow("Not so far away") << c[0] << c[1] << c[2] << 0.0 << QGeoCoordinate(0, 0) << false << true;
    QTest::newRow("Not so far away and large line") << c[0] << c[1] << c[2] << 100000.0 << QGeoCoordinate(0.8, 0.8) << true << true;
}

QTEST_MAIN(tst_QGeoPath)
#include "tst_qgeopath.moc"
