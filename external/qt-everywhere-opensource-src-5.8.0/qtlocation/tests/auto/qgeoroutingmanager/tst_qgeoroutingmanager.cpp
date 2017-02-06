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

//TESTED_COMPONENT=src/location

#include "tst_qgeoroutingmanager.h"

QT_USE_NAMESPACE


void tst_QGeoRoutingManager::initTestCase()
{
    /*
     * Set custom path since CI doesn't install test plugins
     */
#ifdef Q_OS_WIN
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() +
                                     QStringLiteral("/../../../../plugins"));
#else
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath()
                                     + QStringLiteral("/../../../plugins"));
#endif
    tst_QGeoRoutingManager::loadRoutingManager();
}

void tst_QGeoRoutingManager::cleanupTestCase()
{
    //delete qgeoroutingmanager;
    delete qgeoserviceprovider;
}

void tst_QGeoRoutingManager::init()
{
}

void tst_QGeoRoutingManager::cleanup()
{
}

void tst_QGeoRoutingManager::loadRoutingManager()
{
    QStringList providers = QGeoServiceProvider::availableServiceProviders();
    QVERIFY(providers.contains("georoute.test.plugin"));

    qgeoserviceprovider = new QGeoServiceProvider("georoute.test.plugin");
    QVERIFY(qgeoserviceprovider);
    QCOMPARE(qgeoserviceprovider->error(), QGeoServiceProvider::NotSupportedError);
    qgeoserviceprovider->setAllowExperimental(true);

    QCOMPARE(qgeoserviceprovider->routingFeatures(),
             QGeoServiceProvider::OfflineRoutingFeature
             | QGeoServiceProvider::AlternativeRoutesFeature
             | QGeoServiceProvider::RouteUpdatesFeature
             | QGeoServiceProvider::ExcludeAreasRoutingFeature);
    QCOMPARE(qgeoserviceprovider->error(), QGeoServiceProvider::NoError);

    qgeoroutingmanager = qgeoserviceprovider->routingManager();
    QVERIFY(qgeoroutingmanager);

}

void tst_QGeoRoutingManager::supports()
{
    QCOMPARE(qgeoroutingmanager->supportedTravelModes(),QGeoRouteRequest::PedestrianTravel);
    QCOMPARE(qgeoroutingmanager->supportedFeatureTypes(),QGeoRouteRequest::TollFeature);
    QCOMPARE(qgeoroutingmanager->supportedFeatureWeights(),QGeoRouteRequest::PreferFeatureWeight);
    QCOMPARE(qgeoroutingmanager->supportedRouteOptimizations(),QGeoRouteRequest::FastestRoute);
    QCOMPARE(qgeoroutingmanager->supportedSegmentDetails(),QGeoRouteRequest::BasicSegmentData);
    QCOMPARE(qgeoroutingmanager->supportedManeuverDetails(),QGeoRouteRequest::BasicManeuvers);
}

void tst_QGeoRoutingManager::locale()
{
    QLocale german = QLocale(QLocale::German, QLocale::Germany);
    QLocale english = QLocale(QLocale::C, QLocale::AnyCountry);

    qgeoroutingmanager->setLocale(german);

    QCOMPARE(qgeoroutingmanager->locale(), german);

    QVERIFY(qgeoroutingmanager->locale() != english);

    QLocale en_UK = QLocale(QLocale::English, QLocale::UnitedKingdom);
    qgeoroutingmanager->setLocale(en_UK);
    QCOMPARE(qgeoroutingmanager->measurementSystem(), en_UK.measurementSystem());
    qgeoroutingmanager->setMeasurementSystem(QLocale::MetricSystem);
    QCOMPARE(qgeoroutingmanager->measurementSystem(), QLocale::MetricSystem);
    QVERIFY(qgeoroutingmanager->locale().measurementSystem() != qgeoroutingmanager->measurementSystem());
}

void tst_QGeoRoutingManager::name()
{
    QString name = "georoute.test.plugin";
    QCOMPARE(qgeoroutingmanager->managerName(), name);
}

void tst_QGeoRoutingManager::version()
{
    QCOMPARE(qgeoroutingmanager->managerVersion(), 100);
}

void tst_QGeoRoutingManager::calculate()
{
    QString error = "no error";
    origin = new QGeoCoordinate(12.12 , 23.23);
    destination = new QGeoCoordinate(34.34 , 89.32);
    request = new QGeoRouteRequest(*origin, *destination);

    reply = qgeoroutingmanager->calculateRoute(*request);

    QCOMPARE(reply->error(), QGeoRouteReply::NoError);
    QCOMPARE(reply->errorString(), error);

    delete origin;
    delete destination;
    delete request;
    delete reply;
}


void tst_QGeoRoutingManager::update()
{
    QString error = "no error";
    position = new QGeoCoordinate(34.34, 89.32);
    route = new QGeoRoute();

    reply = qgeoroutingmanager->updateRoute(*route, *position);

    QCOMPARE(reply->error(), QGeoRouteReply::CommunicationError);
    QCOMPARE(reply->errorString(), error);

    delete position;
    delete route;
    delete reply;
}

QTEST_MAIN(tst_QGeoRoutingManager)

