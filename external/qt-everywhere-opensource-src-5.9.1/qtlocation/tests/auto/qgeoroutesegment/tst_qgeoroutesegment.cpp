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

#include "tst_qgeoroutesegment.h"

QT_USE_NAMESPACE

tst_QGeoRouteSegment::tst_QGeoRouteSegment()
{
}

void tst_QGeoRouteSegment::initTestCase()
{
}

void tst_QGeoRouteSegment::cleanupTestCase()
{
}

void tst_QGeoRouteSegment::init()
{
    qgeocoordinate = new QGeoCoordinate();
    qgeoroutesegment = new QGeoRouteSegment();
    qgeomaneuver = new QGeoManeuver();
}

void tst_QGeoRouteSegment::cleanup()
{
    delete qgeocoordinate;
    delete qgeoroutesegment;
}

void tst_QGeoRouteSegment::constructor()
{
    QVERIFY(!qgeoroutesegment->isValid());
    QCOMPARE(qgeoroutesegment->distance(), qreal(0.0));
    QCOMPARE(qgeoroutesegment->maneuver(),*qgeomaneuver);
    QCOMPARE(qgeoroutesegment->travelTime(),0);
}

void tst_QGeoRouteSegment::copy_constructor()
{
    QGeoRouteSegment *qgeoroutesegmentcopy = new QGeoRouteSegment(*qgeoroutesegment);

    QCOMPARE(*qgeoroutesegment, *qgeoroutesegmentcopy);

    QVERIFY(!qgeoroutesegmentcopy->isValid());
    QCOMPARE(qgeoroutesegmentcopy->distance(), qreal(0.0));
    QCOMPARE(qgeoroutesegmentcopy->maneuver(), *qgeomaneuver);
    QCOMPARE(qgeoroutesegmentcopy->travelTime(), 0);

    delete qgeoroutesegmentcopy;
}

void tst_QGeoRouteSegment::destructor()
{
    QGeoRouteSegment *qgeoroutesegmentcopy;

    qgeoroutesegmentcopy = new QGeoRouteSegment();
    delete qgeoroutesegmentcopy;

    qgeoroutesegmentcopy = new QGeoRouteSegment(*qgeoroutesegment);
    delete qgeoroutesegmentcopy;
}


void tst_QGeoRouteSegment::travelTime()
{
    QFETCH(int, traveltime);

    QGeoRouteSegment sgmt;
    QVERIFY(!sgmt.isValid());

    sgmt.setTravelTime(traveltime);

    QVERIFY(sgmt.isValid());
    QCOMPARE(sgmt.travelTime(), traveltime);
}

void tst_QGeoRouteSegment::travelTime_data()
{
    QTest::addColumn<int>("traveltime");

    QTest::newRow("travel1") << 0;
    QTest::newRow("travel2") << -50;
    QTest::newRow("travel3") << 324556;
}

void tst_QGeoRouteSegment::distance()
{
    QFETCH(qreal, distance);

    QGeoRouteSegment sgmt;
    QVERIFY(!sgmt.isValid());

    sgmt.setDistance(distance);

    QVERIFY(sgmt.isValid());
    QCOMPARE(sgmt.distance(), distance);
}

void tst_QGeoRouteSegment::distance_data()
{
    QTest::addColumn<qreal>("distance");

    QTest::newRow("distance1") << qreal(0.0) ;
    QTest::newRow("distance2") << qreal(-50.3435) ;
    QTest::newRow("distance3") << qreal(324556.543534) ;
}


void tst_QGeoRouteSegment::path()
{
    QFETCH(QList<double>, coordinates);

    QGeoRouteSegment sgmt;
    QVERIFY(!sgmt.isValid());

    QList<QGeoCoordinate> path;

    for (int i = 0; i < coordinates.size(); i += 2) {
        path.append(QGeoCoordinate(coordinates.at(i), coordinates.at(i+1)));
    }

    sgmt.setPath(path);
    QVERIFY(sgmt.isValid());

    QList<QGeoCoordinate> pathretrieved = sgmt.path();
    QCOMPARE(pathretrieved, path);

    for (int i = 0; i < pathretrieved.size(); i++) {
        QCOMPARE(pathretrieved.at(i), path.at(i));
    }
}

void tst_QGeoRouteSegment::path_data()
{
    QTest::addColumn<QList<double> >("coordinates");

    QList<double> coordinates;

    coordinates << 0.0 << 0.0;
    QTest::newRow("path1") << coordinates;

    coordinates << -23.23 << 54.45345;
    QTest::newRow("path2") << coordinates;

    coordinates << -85.4324 << -121.343;
    QTest::newRow("path3") << coordinates;

    coordinates << 1.323 << 12.323;
    QTest::newRow("path4") << coordinates;

    coordinates << 1324.323 << 143242.323;
    QTest::newRow("path5") << coordinates;
}

void tst_QGeoRouteSegment::nextroutesegment()
{
    QGeoRouteSegment sgmt;
    QVERIFY(!sgmt.isValid());

    QGeoRouteSegment *qgeoroutesegmentcopy = new QGeoRouteSegment();
    qgeoroutesegmentcopy->setDistance(45.34);

    sgmt.setNextRouteSegment(*qgeoroutesegmentcopy);

    QVERIFY(sgmt.isValid());
    QCOMPARE(sgmt.nextRouteSegment(), *qgeoroutesegmentcopy);

    QCOMPARE((sgmt.nextRouteSegment()).distance(),
             qgeoroutesegmentcopy->distance());
    delete qgeoroutesegmentcopy;

}

void tst_QGeoRouteSegment::maneuver()
{
    QGeoRouteSegment sgmt;
    QVERIFY(!sgmt.isValid());

    sgmt.setManeuver(*qgeomaneuver);
    QCOMPARE(sgmt.maneuver(), *qgeomaneuver);
    QVERIFY(sgmt.isValid());
}

void tst_QGeoRouteSegment::operators()
{
    //Create a copy and see that they are the same
    QGeoRouteSegment *qgeoroutesegmentcopy = new QGeoRouteSegment(*qgeoroutesegment);
    QGeoRouteSegment *trueSgmtCopy = new QGeoRouteSegment();

    QVERIFY(qgeoroutesegment->operator ==(*qgeoroutesegmentcopy));
    QVERIFY(!qgeoroutesegment->operator !=(*qgeoroutesegmentcopy));
    QVERIFY(!qgeoroutesegment->isValid());
    QVERIFY(!qgeoroutesegmentcopy->isValid());

    //Same segment ? content is the same but pointer different
    QVERIFY(qgeoroutesegment->operator ==(*trueSgmtCopy));
    QVERIFY(!qgeoroutesegment->operator !=(*trueSgmtCopy));
    QVERIFY(!trueSgmtCopy->isValid());

    QFETCH(double, distance);
    QFETCH(int, traveltime);
    QFETCH(QList<double>, coordinates);

    QList<QGeoCoordinate> path;

    for (int i = 0; i < coordinates.size(); i += 2) {
        path.append(QGeoCoordinate(coordinates.at(i), coordinates.at(i+1)));
    }

    qgeoroutesegment->setDistance(distance);
    qgeoroutesegment->setTravelTime(traveltime);
    qgeoroutesegment->setPath(path);

    trueSgmtCopy->setDistance(distance);
    trueSgmtCopy->setTravelTime(traveltime);
    trueSgmtCopy->setPath(path);

    QCOMPARE(qgeoroutesegment->distance(), distance);
    QCOMPARE(qgeoroutesegment->travelTime(), traveltime);
    QCOMPARE(qgeoroutesegment->path(), path);

    QCOMPARE(qgeoroutesegmentcopy->distance(), distance);
    QCOMPARE(qgeoroutesegmentcopy->travelTime(), traveltime);
    QCOMPARE(qgeoroutesegmentcopy->path(), path);

    QCOMPARE(trueSgmtCopy->distance(), distance);
    QCOMPARE(trueSgmtCopy->travelTime(), traveltime);
    QCOMPARE(trueSgmtCopy->path(), path);

    //Same as based off copy constructor (d-pointer shared)
    QVERIFY(qgeoroutesegment->operator==(*qgeoroutesegmentcopy));
    QVERIFY(!qgeoroutesegment->operator!=(*qgeoroutesegmentcopy));

    //Same as based off same content although different d-pointer
    QVERIFY(qgeoroutesegment->operator==(*trueSgmtCopy));
    QVERIFY(!qgeoroutesegment->operator!=(*trueSgmtCopy));

    const int newTravelTime = 111;
    trueSgmtCopy->setTravelTime(newTravelTime);
    QCOMPARE(trueSgmtCopy->travelTime(), newTravelTime);

    //different d-pointer and different content
    QVERIFY(!qgeoroutesegment->operator==(*trueSgmtCopy));
    QVERIFY(qgeoroutesegment->operator!=(*trueSgmtCopy));

    //Assign one segment to the other and test that they are the same again
    *qgeoroutesegmentcopy = qgeoroutesegmentcopy->operator =(*qgeoroutesegment);
    QVERIFY(qgeoroutesegment->operator==(*qgeoroutesegmentcopy));
    QVERIFY(!qgeoroutesegment->operator!=(*qgeoroutesegmentcopy));

    *qgeoroutesegmentcopy = qgeoroutesegmentcopy->operator =(*trueSgmtCopy);
    QVERIFY(trueSgmtCopy->operator==(*qgeoroutesegmentcopy));
    QVERIFY(!trueSgmtCopy->operator!=(*qgeoroutesegmentcopy));

    delete trueSgmtCopy;
    delete qgeoroutesegmentcopy;
}

void tst_QGeoRouteSegment::operators_data()
{
    QTest::addColumn<double>("distance");
    QTest::addColumn<int>("traveltime");
    QTest::addColumn<QList<double> >("coordinates");

    QList<double> coordinates;

    coordinates << 0.0 << 0.0 << 23.234 << -121.767 << 8.43534 << 32.789;
    QTest::newRow("set1") << 143545.43 << 45665 << coordinates;

    coordinates << 42.343 << -38.657;
    QTest::newRow("set2") << 745654.43 << 786585 << coordinates;
}

QTEST_APPLESS_MAIN(tst_QGeoRouteSegment);
