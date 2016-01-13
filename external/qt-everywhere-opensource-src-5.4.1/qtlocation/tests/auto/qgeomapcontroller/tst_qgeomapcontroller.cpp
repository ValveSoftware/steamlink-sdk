/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QSignalSpy>

#include <QtLocation/private/qgeomapdata_p.h>
#include <QtLocation/private/qgeomapcontroller_p.h>

// cross-reference test plugin, where concrete subclasses are needed
// in order to create a concrete mapcontroller
#include "../geotestplugin/qgeoserviceproviderplugin_test.h"
#include "../geotestplugin/qgeotiledmapdata_test.h"
#include "../geotestplugin/qgeotiledmappingmanagerengine_test.h"

QT_USE_NAMESPACE

class tst_QGeoMapController : public QObject
{
    Q_OBJECT

public:
    tst_QGeoMapController();
    ~tst_QGeoMapController();

private:
    QGeoTiledMapDataTest *map_;
    QSignalSpy *signalCenterChanged_;
    QSignalSpy *signalBearingChanged_;
    QSignalSpy *signalTiltChanged_;
    QSignalSpy *signalRollChanged_;
    QSignalSpy *signalZoomChanged_;
    void clearSignalSpies();

private Q_SLOTS:
    void signalsConstructedTest();
    void constructorTest_data();
    void constructorTest();
    void centerTest();
    void bearingTest();
    void tiltTest();
    void rollTest();
    void panTest();
    void zoomTest();
};

tst_QGeoMapController::tst_QGeoMapController()
{
    // unlike low level classes, geomapcontroller is built up from several parent classes
    // so, in order to test it, we need to create these parent classes for it to link to
    // such as a GeoMapData
    QGeoServiceProviderFactoryTest serviceProviderTest; // empty constructor

    // TODO: check whether the default constructors of these objects allow the create to work
    QVariantMap parameterMap;
    QGeoServiceProvider::Error mappingError;
    QString mappingErrorString;

    QGeoTiledMappingManagerEngineTest *mapEngine = static_cast<QGeoTiledMappingManagerEngineTest*>(serviceProviderTest.createMappingManagerEngine(parameterMap, &mappingError, &mappingErrorString));
    map_ = new QGeoTiledMapDataTest(mapEngine);
    map_->resize(100, 100);


    signalCenterChanged_ = new QSignalSpy(map_->mapController(), SIGNAL(centerChanged(QGeoCoordinate)));
    signalBearingChanged_ = new QSignalSpy(map_->mapController(), SIGNAL(bearingChanged(qreal)));
    signalTiltChanged_ = new QSignalSpy(map_->mapController(), SIGNAL(tiltChanged(qreal)));
    signalRollChanged_ = new QSignalSpy(map_->mapController(), SIGNAL(rollChanged(qreal)));
    signalZoomChanged_ = new QSignalSpy(map_->mapController(), SIGNAL(zoomChanged(qreal)));
}

void tst_QGeoMapController::clearSignalSpies()
{
    signalCenterChanged_->clear();
    signalBearingChanged_->clear();
    signalTiltChanged_->clear();
    signalRollChanged_->clear();
    signalZoomChanged_->clear();
}

tst_QGeoMapController::~tst_QGeoMapController()
{
    delete signalCenterChanged_;
    delete signalBearingChanged_;
    delete signalTiltChanged_;
    delete signalRollChanged_;
    delete signalZoomChanged_;
    delete map_;
}

void tst_QGeoMapController::signalsConstructedTest()
{
    QVERIFY(signalCenterChanged_->isValid());
    QVERIFY(signalBearingChanged_->isValid());
    QVERIFY(signalTiltChanged_->isValid());
    QVERIFY(signalRollChanged_->isValid());
    QVERIFY(signalZoomChanged_->isValid());
}

void tst_QGeoMapController::constructorTest_data()
{
    QTest::addColumn<QGeoCoordinate>("center");
    QTest::addColumn<double>("bearing");
    QTest::addColumn<double>("tilt");
    QTest::addColumn<double>("roll");
    QTest::addColumn<double>("zoom");
    QTest::newRow("zeros") << QGeoCoordinate() << 0.0 << 0.0 << 0.0 << 0.0;
    QTest::newRow("valid") << QGeoCoordinate(10.0, 20.5, 30.8) << 0.1 << 0.2 << 0.3 << 2.0;
    QTest::newRow("negative values") << QGeoCoordinate(-50, -20, 100) << -0.1 << -0.2 << -0.3 << 1.0;
}

void tst_QGeoMapController::constructorTest()
{
    QFETCH(QGeoCoordinate, center);
    QFETCH(double, bearing);
    QFETCH(double, tilt);
    QFETCH(double, roll);
    QFETCH(double, zoom);

    // test whether the map controller picks up the camera data
    // from the map during construction
    QGeoCameraData cameraData;
    cameraData.setCenter(center);
    cameraData.setBearing(bearing);
    cameraData.setTilt(tilt);
    cameraData.setRoll(roll);
    cameraData.setZoomLevel(zoom);
    map_->setCameraData(cameraData);
    QGeoMapController mapController(map_);

    // make sure the values come out the same
    // also make sure the values match what they were actually set to
    QCOMPARE(mapController.center(), cameraData.center());
    QCOMPARE(mapController.center(), center);
    QCOMPARE(mapController.zoom(), cameraData.zoomLevel());
    QCOMPARE(mapController.zoom(), zoom);

    if (map_->cameraCapabilities().supportsBearing()){
        QCOMPARE(mapController.bearing(), cameraData.bearing());
        QCOMPARE(mapController.bearing(), bearing);
    }
    if (map_->cameraCapabilities().supportsTilting()){
        QCOMPARE(mapController.tilt(), cameraData.tilt());
        QCOMPARE(mapController.tilt(), tilt);
    }
    if (map_->cameraCapabilities().supportsRolling()){
        QCOMPARE(mapController.roll(), cameraData.roll());
        QCOMPARE(mapController.roll(), roll);
    }
}

void tst_QGeoMapController::centerTest()
{
    QGeoCameraData cameraData;
    cameraData.setCenter(QGeoCoordinate(10.0,-20.4,30.8));
    map_->setCameraData(cameraData);
    QGeoMapController mapController(map_);
    QCOMPARE(mapController.center(),QGeoCoordinate(10.0,-20.4,30.8));

    QGeoCoordinate coord(10.0,20.4,30.8);
    clearSignalSpies();
    mapController.setCenter(coord);

    // check correct signal is triggered
    QCOMPARE(signalCenterChanged_->count(),1);
    QCOMPARE(signalBearingChanged_->count(),0);
    QCOMPARE(signalTiltChanged_->count(),0);
    QCOMPARE(signalRollChanged_->count(),0);
    QCOMPARE(signalZoomChanged_->count(),0);

    QCOMPARE(mapController.center(),QGeoCoordinate(10.0,20.4,30.8));

    mapController.setCenter(QGeoCoordinate(10.0,20.4,30.9));
    QCOMPARE(mapController.center(),QGeoCoordinate(10.0,20.4,30.9));
}

void tst_QGeoMapController::bearingTest()
{
    if (map_->cameraCapabilities().supportsBearing()){
        qreal bearing = 1.4;

        QGeoCameraData cameraData;
        cameraData.setBearing(bearing);
        map_->setCameraData(cameraData);
        QGeoMapController mapController(map_);
        QCOMPARE(mapController.bearing(),bearing);

        clearSignalSpies();
        mapController.setBearing(-1.5);
        QCOMPARE(mapController.bearing(),-1.5);

        // check correct signal is triggered
        QCOMPARE(signalCenterChanged_->count(),0);
        QCOMPARE(signalBearingChanged_->count(),1);
        QCOMPARE(signalTiltChanged_->count(),0);
        QCOMPARE(signalRollChanged_->count(),0);
        QCOMPARE(signalZoomChanged_->count(),0);
    }
}

void tst_QGeoMapController::tiltTest()
{
    if (map_->cameraCapabilities().supportsTilting()){
        qreal tilt = map_->cameraCapabilities().maximumTilt();

        QGeoCameraData cameraData;
        cameraData.setTilt(tilt);
        map_->setCameraData(cameraData);
        QGeoMapController mapController(map_);
        QCOMPARE(mapController.tilt(),tilt);

        tilt = map_->cameraCapabilities().minimumTilt();
        clearSignalSpies();
        mapController.setTilt(tilt);
        QCOMPARE(mapController.tilt(),tilt);

        // check correct signal is triggered
        QCOMPARE(signalCenterChanged_->count(),0);
        QCOMPARE(signalBearingChanged_->count(),0);
        QCOMPARE(signalTiltChanged_->count(),1);
        QCOMPARE(signalRollChanged_->count(),0);
        QCOMPARE(signalZoomChanged_->count(),0);
    }
}

void tst_QGeoMapController::rollTest()
{
    if (map_->cameraCapabilities().supportsRolling()){
        qreal roll = 1.4;

        QGeoCameraData cameraData;
        cameraData.setRoll(roll);
        map_->setCameraData(cameraData);
        QGeoMapController mapController(map_);
        QCOMPARE(mapController.roll(),roll);

        clearSignalSpies();
        mapController.setRoll(-1.5);
        QCOMPARE(mapController.roll(),-1.5);

        // check correct signal is triggered
        QCOMPARE(signalCenterChanged_->count(),0);
        QCOMPARE(signalBearingChanged_->count(),0);
        QCOMPARE(signalTiltChanged_->count(),0);
        QCOMPARE(signalRollChanged_->count(),1);
        QCOMPARE(signalZoomChanged_->count(),0);
    }
}

void tst_QGeoMapController::panTest()
{
    QGeoMapController mapController(map_);

    mapController.setCenter(QGeoCoordinate(-1.0,-2.4,3.8));

    // check that pan of zero leaves the camera centre unaltered
    mapController.pan(0, 0);
    QCOMPARE(mapController.center().altitude(), 3.8);
    QCOMPARE(mapController.center().latitude(), -1.0);
    QCOMPARE(mapController.center().longitude(), -2.4);

    qreal dx = 13.1;
    qreal dy = -9.3;
    clearSignalSpies();
    mapController.pan(dx, dy);

    // rather than verify the exact new position, we check that the position has changed and the altitude
    // is unaffected
    QCOMPARE(mapController.center().altitude(), 3.8);
    QVERIFY(qFuzzyCompare(mapController.center().latitude(), -1.0) == false);
    QVERIFY(qFuzzyCompare(mapController.center().longitude(), -2.4) == false);

    // check correct signal is triggered
    QCOMPARE(signalCenterChanged_->count(),1);
    QCOMPARE(signalBearingChanged_->count(),0);
    QCOMPARE(signalTiltChanged_->count(),0);
    QCOMPARE(signalRollChanged_->count(),0);
    QCOMPARE(signalZoomChanged_->count(),0);
}

void tst_QGeoMapController::zoomTest()
{
    QGeoCameraData cameraData;
    cameraData.setZoomLevel(1.4);
    map_->setCameraData(cameraData);
    QGeoMapController mapController(map_);

    QCOMPARE(mapController.zoom(),1.4);
    mapController.setZoom(1.4);
    QCOMPARE(mapController.zoom(),1.4);
    clearSignalSpies();
    mapController.setZoom(1.5);
    QCOMPARE(mapController.zoom(),1.5);

    // check correct signal is triggered
    QCOMPARE(signalCenterChanged_->count(),0);
    QCOMPARE(signalBearingChanged_->count(),0);
    QCOMPARE(signalTiltChanged_->count(),0);
    QCOMPARE(signalRollChanged_->count(),0);
    QCOMPARE(signalZoomChanged_->count(),1);
}


QTEST_APPLESS_MAIN(tst_QGeoMapController)

#include "tst_qgeomapcontroller.moc"
