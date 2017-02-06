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

#include "tst_qgeorouterequest.h"

#include <QtPositioning/QGeoRectangle>

QT_USE_NAMESPACE

tst_QGeoRouteRequest::tst_QGeoRouteRequest()
{
}

void tst_QGeoRouteRequest::initTestCase()
{
}

void tst_QGeoRouteRequest::cleanupTestCase()
{
}

void tst_QGeoRouteRequest::init()
{
    qgeocoordinate = new QGeoCoordinate();
    qgeoboundingbox = new QGeoRectangle();
    qgeorouterequest = new QGeoRouteRequest();
}

void tst_QGeoRouteRequest::cleanup()
{
    delete qgeocoordinate;
    delete qgeoboundingbox;
    delete qgeorouterequest;
}

void tst_QGeoRouteRequest::constructor_waypoints()
{
    QGeoCoordinate *qgeocoord1 = new QGeoCoordinate(43.5435, 76.342);
    QGeoCoordinate *qgeocoord2 = new QGeoCoordinate(-43.5435, 176.342);
    QGeoCoordinate *qgeocoord3 = new QGeoCoordinate(-13.5435, +76.342);

    QList<QGeoCoordinate> waypoints;
    waypoints.append(*qgeocoord1);
    waypoints.append(*qgeocoord2);
    waypoints.append(*qgeocoord3);

    QGeoRouteRequest *copy = new QGeoRouteRequest(waypoints);

    QCOMPARE(copy->waypoints(), waypoints);
    QCOMPARE(copy->numberAlternativeRoutes(), 0);
    QCOMPARE(copy->routeOptimization(), QGeoRouteRequest::FastestRoute);
    QCOMPARE(copy->segmentDetail(), QGeoRouteRequest::BasicSegmentData);
    QCOMPARE(copy->travelModes(), QGeoRouteRequest::CarTravel);
    QCOMPARE(copy->maneuverDetail(), QGeoRouteRequest::BasicManeuvers);

    delete qgeocoord1;
    delete qgeocoord2;
    delete qgeocoord3;
    delete copy;
}

void tst_QGeoRouteRequest::constructor_orig_dest()
{
    QGeoCoordinate *qgeocoord1 = new QGeoCoordinate(43.5435, 76.342);
    QGeoCoordinate *qgeocoord2 = new QGeoCoordinate(-43.5435, 176.342);

    QGeoRouteRequest *copy = new QGeoRouteRequest(*qgeocoord1, *qgeocoord2);

    QList<QGeoCoordinate> waypoints;
    waypoints.append(*qgeocoord1);
    waypoints.append(*qgeocoord2);

    QCOMPARE(copy->waypoints(), waypoints);
    QCOMPARE(copy->numberAlternativeRoutes(), 0);
    QCOMPARE(copy->routeOptimization(), QGeoRouteRequest::FastestRoute);
    QCOMPARE(copy->segmentDetail(), QGeoRouteRequest::BasicSegmentData);
    QCOMPARE(copy->travelModes(), QGeoRouteRequest::CarTravel);
    QCOMPARE(copy->maneuverDetail(), QGeoRouteRequest::BasicManeuvers);

    delete qgeocoord1;
    delete qgeocoord2;
    delete copy;
}

void tst_QGeoRouteRequest::copy_constructor()
{
    QGeoRouteRequest *qgeorouterequestcopy = new QGeoRouteRequest(*qgeorouterequest);
    QCOMPARE(*qgeorouterequest, *qgeorouterequestcopy);
    delete qgeorouterequestcopy;
}

void tst_QGeoRouteRequest::destructor()
{
    QGeoRouteRequest *qgeorouterequestcopy;

    qgeorouterequestcopy = new QGeoRouteRequest();
    delete qgeorouterequestcopy;

    qgeorouterequestcopy = new QGeoRouteRequest(*qgeorouterequest);
    delete qgeorouterequestcopy;
}

void tst_QGeoRouteRequest::excludeAreas()
{
    qgeocoordinate->setLatitude(13.3851);
    qgeocoordinate->setLongitude(52.5312);

    QGeoCoordinate *qgeocoordinatecopy = new QGeoCoordinate(34.324 , -110.32);

    QGeoRectangle *qgeoboundingboxcopy = new QGeoRectangle(*qgeocoordinate, 0.4, 0.4);
    QGeoRectangle *qgeoboundingboxcopy2 = new QGeoRectangle(*qgeocoordinatecopy, 1.2, 0.9);
    QList<QGeoRectangle> areas;
    areas.append(*qgeoboundingboxcopy);
    areas.append(*qgeoboundingboxcopy2);

    qgeorouterequest->setExcludeAreas(areas);

    QCOMPARE(qgeorouterequest->excludeAreas(), areas);

    delete qgeoboundingboxcopy;
    delete qgeoboundingboxcopy2;
    delete qgeocoordinatecopy;
}

void tst_QGeoRouteRequest::numberAlternativeRoutes()
{
    qgeorouterequest->setNumberAlternativeRoutes(0);
    QCOMPARE(qgeorouterequest->numberAlternativeRoutes(), 0);

    qgeorouterequest->setNumberAlternativeRoutes(24);
    QCOMPARE(qgeorouterequest->numberAlternativeRoutes(), 24);

    qgeorouterequest->setNumberAlternativeRoutes(-12);

    QCOMPARE(qgeorouterequest->numberAlternativeRoutes(), 0);
}

void tst_QGeoRouteRequest::routeOptimization()
{
    QFETCH(QGeoRouteRequest::RouteOptimization, optimization);

    QCOMPARE(qgeorouterequest->routeOptimization(),QGeoRouteRequest::FastestRoute);

    qgeorouterequest->setRouteOptimization(optimization);
    QCOMPARE(qgeorouterequest->routeOptimization(), optimization);
}

void tst_QGeoRouteRequest::routeOptimization_data()
{
    QTest::addColumn<QGeoRouteRequest::RouteOptimization>("optimization");

    QTest::newRow("optimization1") << QGeoRouteRequest::FastestRoute;
    QTest::newRow("optimization2") << QGeoRouteRequest::ShortestRoute;
    QTest::newRow("optimization3") << QGeoRouteRequest::MostEconomicRoute;
    QTest::newRow("optimization4") << QGeoRouteRequest::MostScenicRoute;

}

void tst_QGeoRouteRequest::segmentDetail()
{
    QFETCH(QGeoRouteRequest::SegmentDetail, detail);

    QCOMPARE(qgeorouterequest->segmentDetail(), QGeoRouteRequest::BasicSegmentData);

    qgeorouterequest->setSegmentDetail(detail);
    QCOMPARE(qgeorouterequest->segmentDetail(), detail);
}

void tst_QGeoRouteRequest::segmentDetail_data()
{
    QTest::addColumn<QGeoRouteRequest::SegmentDetail>("detail");

    QTest::newRow("detail1") << QGeoRouteRequest::NoSegmentData;
    QTest::newRow("detail2") << QGeoRouteRequest::BasicSegmentData;
}

void tst_QGeoRouteRequest::travelModes()
{
    QFETCH(QGeoRouteRequest::TravelMode,mode);

    QCOMPARE(qgeorouterequest->travelModes(), QGeoRouteRequest::CarTravel);

    qgeorouterequest->setTravelModes(mode);
    QCOMPARE(qgeorouterequest->travelModes(), mode);
}

void tst_QGeoRouteRequest::travelModes_data()
{
    QTest::addColumn<QGeoRouteRequest::TravelMode>("mode");

    QTest::newRow("mode1") << QGeoRouteRequest::CarTravel;
    QTest::newRow("mode2") << QGeoRouteRequest::PedestrianTravel;
    QTest::newRow("mode3") << QGeoRouteRequest::BicycleTravel;
    QTest::newRow("mode4") << QGeoRouteRequest::PublicTransitTravel;
    QTest::newRow("mode5") << QGeoRouteRequest::TruckTravel;
}

void tst_QGeoRouteRequest::waypoints()
{
    QFETCH(QList<double>, coordinates);

    QList<QGeoCoordinate> waypoints;

    for (int i = 0; i < coordinates.size(); i += 2) {
        waypoints.append(QGeoCoordinate(coordinates.at(i), coordinates.at(i+1)));
    }

    qgeorouterequest->setWaypoints(waypoints);

    QList<QGeoCoordinate> waypointsRetrieved = qgeorouterequest->waypoints();
    QCOMPARE(waypointsRetrieved, waypoints);

    for (int i=0; i < waypointsRetrieved.size(); i++) {
        QCOMPARE(waypointsRetrieved.at(i), waypoints.at(i));
    }
}

void tst_QGeoRouteRequest::waypoints_data()
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

void tst_QGeoRouteRequest::maneuverDetail()
{
    QFETCH(QGeoRouteRequest::ManeuverDetail,maneuver);

    QCOMPARE(qgeorouterequest->maneuverDetail(), QGeoRouteRequest::BasicManeuvers);

    qgeorouterequest->setManeuverDetail(maneuver);
    QCOMPARE(qgeorouterequest->maneuverDetail(), maneuver);
}

void tst_QGeoRouteRequest::maneuverDetail_data()
{
    QTest::addColumn<QGeoRouteRequest::ManeuverDetail>("maneuver");

    QTest::newRow("maneuver1") << QGeoRouteRequest::NoManeuvers;
    QTest::newRow("maneuver2") << QGeoRouteRequest::BasicManeuvers;

}

void tst_QGeoRouteRequest::featureWeight_data()
{
    QTest::addColumn<QGeoRouteRequest::FeatureType>("type");
    QTest::addColumn<bool>("checkTypes");
    QTest::addColumn<QGeoRouteRequest::FeatureWeight>("initialWeight");
    QTest::addColumn<QGeoRouteRequest::FeatureWeight>("newWeight");
    QTest::addColumn<QGeoRouteRequest::FeatureWeight>("expectWeight");

    QTest::newRow("NoFeature") << QGeoRouteRequest::NoFeature << false
                               << QGeoRouteRequest::NeutralFeatureWeight
                               << QGeoRouteRequest::PreferFeatureWeight
                               << QGeoRouteRequest::NeutralFeatureWeight;
    QTest::newRow("TollFeature") << QGeoRouteRequest::TollFeature << true
                                 << QGeoRouteRequest::NeutralFeatureWeight
                                 << QGeoRouteRequest::PreferFeatureWeight
                                 << QGeoRouteRequest::PreferFeatureWeight;
    QTest::newRow("HighwayFeature") << QGeoRouteRequest::HighwayFeature << true
                                    << QGeoRouteRequest::NeutralFeatureWeight
                                    << QGeoRouteRequest::RequireFeatureWeight
                                    << QGeoRouteRequest::RequireFeatureWeight;
}

void tst_QGeoRouteRequest::featureWeight()
{
    QFETCH(QGeoRouteRequest::FeatureType, type);
    QFETCH(bool, checkTypes);
    QFETCH(QGeoRouteRequest::FeatureWeight, initialWeight);
    QFETCH(QGeoRouteRequest::FeatureWeight, newWeight);
    QFETCH(QGeoRouteRequest::FeatureWeight, expectWeight);

    QCOMPARE(qgeorouterequest->featureWeight(type), initialWeight);
    qgeorouterequest->setFeatureWeight(type, newWeight);
    QCOMPARE(qgeorouterequest->featureWeight(type), expectWeight);

    if (checkTypes)
        QVERIFY(qgeorouterequest->featureTypes().contains(type));
}


QTEST_APPLESS_MAIN(tst_QGeoRouteRequest);
