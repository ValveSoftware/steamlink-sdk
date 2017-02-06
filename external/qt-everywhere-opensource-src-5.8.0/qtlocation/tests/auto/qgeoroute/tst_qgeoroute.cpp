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

#include "tst_qgeoroute.h"


tst_QGeoRoute::tst_QGeoRoute()
{
}

void tst_QGeoRoute::initTestCase()
{
}

void tst_QGeoRoute::cleanupTestCase()
{
}

void tst_QGeoRoute::init()
{
    qgeoroute = new QGeoRoute();
    qgeocoordinate = new QGeoCoordinate();
}

void tst_QGeoRoute::cleanup()
{
    delete qgeoroute;
    delete qgeocoordinate;
}

void tst_QGeoRoute::constructor()
{
    QString empty = "";
    QGeoRectangle *boundingbox = new QGeoRectangle();

    QCOMPARE(qgeoroute->bounds(), *boundingbox);
    QCOMPARE(qgeoroute->distance(), qreal(0.0));
    QCOMPARE(qgeoroute->path().size(), 0);
    QCOMPARE(qgeoroute->routeId(), empty);
    QCOMPARE(qgeoroute->travelTime(), 0);

    delete boundingbox;
}

void tst_QGeoRoute::copy_constructor()
{
    QGeoRoute *qgeoroutecopy = new QGeoRoute(*qgeoroute);
    QCOMPARE(*qgeoroute, *qgeoroutecopy);
    delete qgeoroutecopy;
}

void tst_QGeoRoute::destructor()
{
    QGeoRoute *qgeoroutecopy;

    qgeoroutecopy = new QGeoRoute();
    delete qgeoroutecopy;

    qgeoroutecopy = new QGeoRoute(*qgeoroute);
    delete qgeoroutecopy;
}

void tst_QGeoRoute::bounds()
{
    qgeocoordinate->setLatitude(13.3851);
    qgeocoordinate->setLongitude(52.5312);

    QGeoRectangle *qgeoboundingbox = new QGeoRectangle(*qgeocoordinate,0.4,0.4);

    qgeoroute->setBounds(*qgeoboundingbox);

    QCOMPARE(qgeoroute->bounds(), *qgeoboundingbox);

    qgeoboundingbox->setWidth(23.1);

    QVERIFY(qgeoroute->bounds().width() != qgeoboundingbox->width());

    delete qgeoboundingbox;
}

void tst_QGeoRoute::distance()
{
    qreal distance = 0.0;

    qgeoroute->setDistance(distance);
    QCOMPARE(qgeoroute->distance(), distance);

    distance = 34.4324;
    QVERIFY(qgeoroute->distance() != distance);

    qgeoroute->setDistance(distance);
    QCOMPARE(qgeoroute->distance(), distance);
}

void tst_QGeoRoute::path()
{
    QFETCH(QList<double>, coordinates);

    QList<QGeoCoordinate> path;

    for (int i = 0; i < coordinates.size(); i += 2) {
        path.append(QGeoCoordinate(coordinates.at(i),coordinates.at(i+1)));
    }

    qgeoroute->setPath(path);

    QList<QGeoCoordinate> pathRetrieved = qgeoroute->path();
    QCOMPARE(pathRetrieved, path);

    for (int i = 0; i < pathRetrieved.size(); i++) {
        QCOMPARE(pathRetrieved.at(i), path.at(i));
    }
}

void tst_QGeoRoute::path_data()
{
    QTest::addColumn<QList<double> >("coordinates");

    QList<double> coordinates;

    coordinates << 0.0 << 0.0;
    QTest::newRow("path1") << coordinates ;

    coordinates << -23.23 << 54.45345;
    QTest::newRow("path2") << coordinates ;

    coordinates << -85.4324 << -121.343;
    QTest::newRow("path3") << coordinates ;

    coordinates << 1.323 << 12.323;
    QTest::newRow("path4") << coordinates ;

    coordinates << 1324.323 << 143242.323;
    QTest::newRow("path5") << coordinates ;
}

void tst_QGeoRoute::request()
{
    qgeocoordinate->setLatitude(65.654);
    qgeocoordinate->setLongitude(0.4324);

    QGeoCoordinate *qgeocoordinatecopy = new QGeoCoordinate(34.54 , -21.32);

    QList<QGeoCoordinate> path;
    path.append(*qgeocoordinate);
    path.append(*qgeocoordinatecopy);

    qgeorouterequest = new QGeoRouteRequest(path);

    qgeoroute->setRequest(*qgeorouterequest);

    QCOMPARE(qgeoroute->request(), *qgeorouterequest);

    QGeoCoordinate *qgeocoordinatecopy2 = new QGeoCoordinate(4.7854 , -121.32);
    path.append(*qgeocoordinatecopy2);

    QGeoRouteRequest *qgeorouterequestcopy = new QGeoRouteRequest(path);

    QVERIFY(qgeoroute->request() != *qgeorouterequestcopy);

    delete qgeocoordinatecopy;
    delete qgeocoordinatecopy2;
    delete qgeorouterequest;
    delete qgeorouterequestcopy;
}

void tst_QGeoRoute::routeId()
{
    QString text = "routeId 4504";

    qgeoroute->setRouteId(text);

    QCOMPARE(qgeoroute->routeId(), text);

    text = "routeId 1111";
    QVERIFY(qgeoroute->routeId() != text);

}

void tst_QGeoRoute::firstrouteSegments()
{
    qgeoroutesegment = new QGeoRouteSegment();
    qgeoroutesegment->setDistance(35.453);
    qgeoroutesegment->setTravelTime(56);

    qgeoroute->setFirstRouteSegment(*qgeoroutesegment);

    QCOMPARE(qgeoroute->firstRouteSegment(), *qgeoroutesegment);

    QGeoRouteSegment *qgeoroutesegmentcopy = new QGeoRouteSegment ();
    qgeoroutesegmentcopy->setDistance(435.432);
    qgeoroutesegmentcopy->setTravelTime(786);

    QVERIFY(qgeoroute->firstRouteSegment() != *qgeoroutesegmentcopy);

    delete qgeoroutesegment;
    delete qgeoroutesegmentcopy;

}

void tst_QGeoRoute::travelMode()
{
    QFETCH(QGeoRouteRequest::TravelMode, mode);

    qgeoroute->setTravelMode(mode);
    QCOMPARE(qgeoroute->travelMode(), mode);
}
void tst_QGeoRoute::travelMode_data()
{
    QTest::addColumn<QGeoRouteRequest::TravelMode>("mode");

    QTest::newRow("mode1") << QGeoRouteRequest::CarTravel;
    QTest::newRow("mode2") << QGeoRouteRequest::PedestrianTravel;
    QTest::newRow("mode3") << QGeoRouteRequest::BicycleTravel;
    QTest::newRow("mode4") << QGeoRouteRequest::PublicTransitTravel;
    QTest::newRow("mode5") << QGeoRouteRequest::TruckTravel;
}

void tst_QGeoRoute::travelTime()
{
    int time = 0;
    qgeoroute->setTravelTime(time);

    QCOMPARE (qgeoroute->travelTime(), time);

    time = 35;

    QVERIFY (qgeoroute->travelTime() != time);

    qgeoroute->setTravelTime(time);
    QCOMPARE (qgeoroute->travelTime(), time);
}

void tst_QGeoRoute::operators()
{
    QGeoRoute *qgeoroutecopy = new QGeoRoute(*qgeoroute);

    QVERIFY(qgeoroute->operator ==(*qgeoroutecopy));
    QVERIFY(!qgeoroute->operator !=(*qgeoroutecopy));

    qgeoroute->setDistance(543.324);
    qgeoroute->setRouteId("RouteId 111");
    qgeoroute->setTravelMode(QGeoRouteRequest::PedestrianTravel);
    qgeoroute->setTravelTime(10);

    qgeoroutecopy->setDistance(12.21);
    qgeoroutecopy->setRouteId("RouteId 666");
    qgeoroutecopy->setTravelMode(QGeoRouteRequest::BicycleTravel);
    qgeoroutecopy->setTravelTime(99);

    QEXPECT_FAIL("", "QGeoRoute equality operators broken", Continue);
    QVERIFY(!(qgeoroute->operator ==(*qgeoroutecopy)));
    QEXPECT_FAIL("", "QGeoRoute equality operators broken", Continue);
    QVERIFY(qgeoroute->operator !=(*qgeoroutecopy));

    *qgeoroutecopy = qgeoroutecopy->operator =(*qgeoroute);
    QVERIFY(qgeoroute->operator ==(*qgeoroutecopy));
    QVERIFY(!qgeoroute->operator !=(*qgeoroutecopy));

    delete qgeoroutecopy;
}



QTEST_APPLESS_MAIN(tst_QGeoRoute);
