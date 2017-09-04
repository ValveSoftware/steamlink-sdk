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

#include <QtLocation/private/qgeocameracapabilities_p.h>
#include <QtLocation/private/qgeotiledmap_p.h>

QT_USE_NAMESPACE

class tst_QGeoCameraCapabilities : public QObject
{
    Q_OBJECT

public:
    tst_QGeoCameraCapabilities();

private:
    void populateGeoCameraCapabilitiesData();

private Q_SLOTS:
    void constructorTest_data();
    void constructorTest();
    void minimumZoomLevelTest();
    void maximumZoomLevelTest();
    void supportsBearingTest();
    void supportsRollingTest();
    void supportsTiltingTest();
    void minimumTiltTest();
    void maximumTiltTest();
    void minimumFieldOfViewTest();
    void maximumFieldOfViewTest();
    void operatorsTest_data();
    void operatorsTest();
    void isValidTest();
};

tst_QGeoCameraCapabilities::tst_QGeoCameraCapabilities()
{
}

void tst_QGeoCameraCapabilities::populateGeoCameraCapabilitiesData(){
    QTest::addColumn<double>("minimumZoomLevel");
    QTest::addColumn<double>("maximumZoomLevel");
    QTest::addColumn<double>("minimumTilt");
    QTest::addColumn<double>("maximumTilt");
    QTest::addColumn<double>("minimumFieldOfView");
    QTest::addColumn<double>("maximumFieldOfView");
    QTest::addColumn<bool>("bearingSupport");
    QTest::addColumn<bool>("rollingSupport");
    QTest::addColumn<bool>("tiltingSupport");
    QTest::newRow("zeros") << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << false << false << false;
    QTest::newRow("valid") << 1.0 << 2.0 << 0.5 << 1.5 << 1.0 << 179.0 << true << true << true;
    QTest::newRow("negative values") << 0.0 << 0.5 << -0.5 << -0.1 << -20.0 << -30.0 << true << true << true;
}

void tst_QGeoCameraCapabilities::constructorTest_data(){
    populateGeoCameraCapabilitiesData();
}

void tst_QGeoCameraCapabilities::constructorTest()
{
    QFETCH(double, minimumZoomLevel);
    QFETCH(double, maximumZoomLevel);
    QFETCH(double, minimumTilt);
    QFETCH(double, maximumTilt);
    QFETCH(double, minimumFieldOfView);
    QFETCH(double, maximumFieldOfView);
    QFETCH(bool, bearingSupport);
    QFETCH(bool, rollingSupport);
    QFETCH(bool, tiltingSupport);

    minimumFieldOfView = qBound(1.0, minimumFieldOfView, 179.0);
    maximumFieldOfView = qBound(1.0, maximumFieldOfView, 179.0);

    // contructor test with default values
    QGeoCameraCapabilities cameraCapabilities;
    QGeoCameraCapabilities cameraCapabilities2(cameraCapabilities);
    QCOMPARE(cameraCapabilities.minimumZoomLevel(), cameraCapabilities2.minimumZoomLevel());
    QCOMPARE(cameraCapabilities.maximumZoomLevel(), cameraCapabilities2.maximumZoomLevel());
    QVERIFY2(cameraCapabilities.supportsBearing() == cameraCapabilities2.supportsBearing(), "Copy constructor failed for bearing support");
    QVERIFY2(cameraCapabilities.supportsRolling() == cameraCapabilities2.supportsRolling(), "Copy constructor failed for rolling support ");
    QVERIFY2(cameraCapabilities.supportsTilting() == cameraCapabilities2.supportsTilting(), "Copy constructor failed for tilting support");
    QCOMPARE(cameraCapabilities.minimumTilt(), cameraCapabilities2.minimumTilt());
    QCOMPARE(cameraCapabilities.maximumTilt(), cameraCapabilities2.maximumTilt());
    QCOMPARE(cameraCapabilities.minimumFieldOfView(), cameraCapabilities2.minimumFieldOfView());
    QCOMPARE(cameraCapabilities.maximumFieldOfView(), cameraCapabilities2.maximumFieldOfView());

    // constructor test after setting values
    cameraCapabilities.setMinimumZoomLevel(minimumZoomLevel);
    cameraCapabilities.setMaximumZoomLevel(maximumZoomLevel);
    cameraCapabilities.setMinimumTilt(minimumTilt);
    cameraCapabilities.setMaximumTilt(maximumTilt);
    cameraCapabilities.setMinimumFieldOfView(minimumFieldOfView);
    cameraCapabilities.setMaximumFieldOfView(maximumFieldOfView);
    cameraCapabilities.setSupportsBearing(bearingSupport);
    cameraCapabilities.setSupportsRolling(rollingSupport);
    cameraCapabilities.setSupportsTilting(tiltingSupport);

    QGeoCameraCapabilities cameraCapabilities3(cameraCapabilities);
    // test the correctness of the constructor copy
    QCOMPARE(cameraCapabilities3.minimumZoomLevel(), minimumZoomLevel);
    QCOMPARE(cameraCapabilities3.maximumZoomLevel(), maximumZoomLevel);
    QCOMPARE(cameraCapabilities3.minimumTilt(), minimumTilt);
    QCOMPARE(cameraCapabilities3.maximumTilt(), maximumTilt);
    QCOMPARE(cameraCapabilities3.minimumFieldOfView(), minimumFieldOfView);
    QCOMPARE(cameraCapabilities3.maximumFieldOfView(), maximumFieldOfView);
    QVERIFY2(cameraCapabilities3.supportsBearing() == bearingSupport, "Copy constructor failed for bearing support");
    QVERIFY2(cameraCapabilities3.supportsRolling() == rollingSupport, "Copy constructor failed for rolling support ");
    QVERIFY2(cameraCapabilities3.supportsTilting() == tiltingSupport, "Copy constructor failed for tilting support");
    // verify that values have not changed after a constructor copy
    QCOMPARE(cameraCapabilities.minimumZoomLevel(), cameraCapabilities3.minimumZoomLevel());
    QCOMPARE(cameraCapabilities.maximumZoomLevel(), cameraCapabilities3.maximumZoomLevel());
    QVERIFY2(cameraCapabilities.supportsBearing() == cameraCapabilities3.supportsBearing(), "Copy constructor failed for bearing support");
    QVERIFY2(cameraCapabilities.supportsRolling() == cameraCapabilities3.supportsRolling(), "Copy constructor failed for rolling support ");
    QVERIFY2(cameraCapabilities.supportsTilting() == cameraCapabilities3.supportsTilting(), "Copy constructor failed for tilting support");
    QCOMPARE(cameraCapabilities.minimumTilt(), cameraCapabilities3.minimumTilt());
    QCOMPARE(cameraCapabilities.maximumTilt(), cameraCapabilities3.maximumTilt());
    QCOMPARE(cameraCapabilities.minimumFieldOfView(), cameraCapabilities3.minimumFieldOfView());
    QCOMPARE(cameraCapabilities.maximumFieldOfView(), cameraCapabilities3.maximumFieldOfView());
}

void tst_QGeoCameraCapabilities::minimumZoomLevelTest()
{
    QGeoCameraCapabilities cameraCapabilities;
    cameraCapabilities.setMinimumZoomLevel(1.5);
    QCOMPARE(cameraCapabilities.minimumZoomLevel(), 1.5);

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QCOMPARE(cameraCapabilities2.minimumZoomLevel(), 1.5);
    cameraCapabilities.setMinimumZoomLevel(2.5);
    QCOMPARE(cameraCapabilities2.minimumZoomLevel(), 1.5);
}

void tst_QGeoCameraCapabilities::maximumZoomLevelTest()
{
    QGeoCameraCapabilities cameraCapabilities;
    cameraCapabilities.setMaximumZoomLevel(3.5);
    QCOMPARE(cameraCapabilities.maximumZoomLevel(), 3.5);

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QCOMPARE(cameraCapabilities2.maximumZoomLevel(), 3.5);
    cameraCapabilities.setMaximumZoomLevel(4.5);
    QCOMPARE(cameraCapabilities2.maximumZoomLevel(), 3.5);
}

void tst_QGeoCameraCapabilities::supportsBearingTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QVERIFY(!cameraCapabilities.supportsBearing());
    cameraCapabilities.setSupportsBearing(true);
    QVERIFY2(cameraCapabilities.supportsBearing(), "Camera capabilities should support bearing");

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QVERIFY(cameraCapabilities2.supportsBearing());
    cameraCapabilities.setSupportsBearing(false);
    QVERIFY2(cameraCapabilities2.supportsBearing(), "Camera capabilities should support bearing");
}

void tst_QGeoCameraCapabilities::supportsRollingTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QVERIFY(!cameraCapabilities.supportsRolling());
    cameraCapabilities.setSupportsRolling(true);
    QVERIFY2(cameraCapabilities.supportsRolling(), "Camera capabilities should support rolling");

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QVERIFY(cameraCapabilities2.supportsRolling());
    cameraCapabilities.setSupportsRolling(false);
    QVERIFY2(cameraCapabilities2.supportsRolling(), "Camera capabilities should support rolling");
}

void tst_QGeoCameraCapabilities::supportsTiltingTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QVERIFY(!cameraCapabilities.supportsTilting());
    cameraCapabilities.setSupportsTilting(true);
    QVERIFY2(cameraCapabilities.supportsTilting(), "Camera capabilities should support tilting");

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QVERIFY(cameraCapabilities2.supportsTilting());
    cameraCapabilities.setSupportsTilting(false);
    QVERIFY2(cameraCapabilities2.supportsTilting(), "Camera capabilities should support tilting");
}

void tst_QGeoCameraCapabilities::minimumTiltTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QCOMPARE(cameraCapabilities.minimumTilt(),0.0);
    cameraCapabilities.setMinimumTilt(0.5);
    QCOMPARE(cameraCapabilities.minimumTilt(),0.5);

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QCOMPARE(cameraCapabilities2.minimumTilt(), 0.5);
    cameraCapabilities.setMinimumTilt(1.5);
    QCOMPARE(cameraCapabilities2.minimumTilt(), 0.5);
}

void tst_QGeoCameraCapabilities::maximumTiltTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QCOMPARE(cameraCapabilities.maximumTilt(),0.0);
    cameraCapabilities.setMaximumTilt(1.5);
    QCOMPARE(cameraCapabilities.maximumTilt(),1.5);

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QCOMPARE(cameraCapabilities2.maximumTilt(), 1.5);
    cameraCapabilities.setMaximumTilt(2.5);
    QCOMPARE(cameraCapabilities2.maximumTilt(), 1.5);
}

void tst_QGeoCameraCapabilities::minimumFieldOfViewTest()
{
    QGeoCameraCapabilities cameraCapabilities;
    QCOMPARE(cameraCapabilities.minimumFieldOfView(), 45.0); // min/max default to 45
    cameraCapabilities.setMinimumFieldOfView(1.5);
    QCOMPARE(cameraCapabilities.minimumFieldOfView(), 1.5);
    cameraCapabilities.setMinimumFieldOfView(-1.5);
    QCOMPARE(cameraCapabilities.minimumFieldOfView(), 1.0);
    cameraCapabilities.setMinimumFieldOfView(245.5);
    QCOMPARE(cameraCapabilities.minimumFieldOfView(), 179.0);

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QCOMPARE(cameraCapabilities2.minimumFieldOfView(), 179.0);
    cameraCapabilities.setMinimumFieldOfView(2.5);
    QCOMPARE(cameraCapabilities2.minimumFieldOfView(), 179.0);
}

void tst_QGeoCameraCapabilities::maximumFieldOfViewTest()
{
    QGeoCameraCapabilities cameraCapabilities;
    QCOMPARE(cameraCapabilities.maximumFieldOfView(), 45.0); // min/max default to 45
    cameraCapabilities.setMaximumFieldOfView(1.5);
    QCOMPARE(cameraCapabilities.maximumFieldOfView(), 1.5);
    cameraCapabilities.setMaximumFieldOfView(-1.5);
    QCOMPARE(cameraCapabilities.maximumFieldOfView(), 1.0);
    cameraCapabilities.setMaximumFieldOfView(245.5);
    QCOMPARE(cameraCapabilities.maximumFieldOfView(), 179.0);

    QGeoCameraCapabilities cameraCapabilities2 = cameraCapabilities;
    QCOMPARE(cameraCapabilities2.maximumFieldOfView(), 179.0);
    cameraCapabilities.setMaximumFieldOfView(2.5);
    QCOMPARE(cameraCapabilities2.maximumFieldOfView(), 179.0);
}

void tst_QGeoCameraCapabilities::operatorsTest_data(){
    populateGeoCameraCapabilitiesData();
}

void tst_QGeoCameraCapabilities::operatorsTest(){

    QFETCH(double, minimumZoomLevel);
    QFETCH(double, maximumZoomLevel);
    QFETCH(double, minimumTilt);
    QFETCH(double, maximumTilt);
    QFETCH(double, minimumFieldOfView);
    QFETCH(double, maximumFieldOfView);
    QFETCH(bool, bearingSupport);
    QFETCH(bool, rollingSupport);
    QFETCH(bool, tiltingSupport);

    minimumFieldOfView = qBound(1.0, minimumFieldOfView, 179.0);
    maximumFieldOfView = qBound(1.0, maximumFieldOfView, 179.0);

    QGeoCameraCapabilities cameraCapabilities;
    cameraCapabilities.setMinimumZoomLevel(minimumZoomLevel);
    cameraCapabilities.setMaximumZoomLevel(maximumZoomLevel);
    cameraCapabilities.setMinimumTilt(minimumTilt);
    cameraCapabilities.setMaximumTilt(maximumTilt);
    cameraCapabilities.setMinimumFieldOfView(minimumFieldOfView);
    cameraCapabilities.setMaximumFieldOfView(maximumFieldOfView);
    cameraCapabilities.setSupportsBearing(bearingSupport);
    cameraCapabilities.setSupportsRolling(rollingSupport);
    cameraCapabilities.setSupportsTilting(tiltingSupport);
    QGeoCameraCapabilities cameraCapabilities2;
    cameraCapabilities2 = cameraCapabilities;
    // test the correctness of the assignment
    QCOMPARE(cameraCapabilities2.minimumZoomLevel(), minimumZoomLevel);
    QCOMPARE(cameraCapabilities2.maximumZoomLevel(), maximumZoomLevel);
    QCOMPARE(cameraCapabilities2.minimumTilt(), minimumTilt);
    QCOMPARE(cameraCapabilities2.maximumTilt(), maximumTilt);
    QVERIFY2(cameraCapabilities2.supportsBearing() == bearingSupport, "Assignment operator failed for bearing support");
    QVERIFY2(cameraCapabilities2.supportsRolling() == rollingSupport, "Assignment operator failed for rolling support ");
    QVERIFY2(cameraCapabilities2.supportsTilting() == tiltingSupport, "Assignment operator failed for tilting support");
    QCOMPARE(cameraCapabilities2.minimumFieldOfView(), minimumFieldOfView);
    QCOMPARE(cameraCapabilities2.maximumFieldOfView(), maximumFieldOfView);
    // verify that values have not changed after a constructor copy
    QCOMPARE(cameraCapabilities.minimumZoomLevel(), cameraCapabilities2.minimumZoomLevel());
    QCOMPARE(cameraCapabilities.maximumZoomLevel(), cameraCapabilities2.maximumZoomLevel());
    QVERIFY2(cameraCapabilities.supportsBearing() == cameraCapabilities2.supportsBearing(), "Assignment operator failed for bearing support");
    QVERIFY2(cameraCapabilities.supportsRolling() == cameraCapabilities2.supportsRolling(), "Assignment operator failed for rolling support ");
    QVERIFY2(cameraCapabilities.supportsTilting() == cameraCapabilities2.supportsTilting(), "Assignment operator failed for tilting support");
    QCOMPARE(cameraCapabilities.minimumTilt(), cameraCapabilities2.minimumTilt());
    QCOMPARE(cameraCapabilities.maximumTilt(), cameraCapabilities2.maximumTilt());
    QCOMPARE(cameraCapabilities.minimumFieldOfView(), cameraCapabilities2.minimumFieldOfView());
    QCOMPARE(cameraCapabilities.maximumFieldOfView(), cameraCapabilities2.maximumFieldOfView());
}

void tst_QGeoCameraCapabilities::isValidTest(){
    QGeoCameraCapabilities cameraCapabilities;
    QVERIFY2(!cameraCapabilities.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities.setSupportsBearing(true);
    QVERIFY2(cameraCapabilities.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities2;
    QVERIFY2(!cameraCapabilities2.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities2.setSupportsRolling(true);
    QVERIFY2(cameraCapabilities2.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities3;
    QVERIFY2(!cameraCapabilities3.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities3.setSupportsTilting(true);
    QVERIFY2(cameraCapabilities3.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities4;
    QVERIFY2(!cameraCapabilities4.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities4.setMinimumZoomLevel(1.0);
    QVERIFY2(cameraCapabilities4.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities5;
    QVERIFY2(!cameraCapabilities5.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities5.setMaximumZoomLevel(1.5);
    QVERIFY2(cameraCapabilities5.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities6;
    QVERIFY2(!cameraCapabilities6.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities6.setMinimumTilt(0.2);
    QVERIFY2(cameraCapabilities6.isValid(), "Camera capabilities should be valid");

    QGeoCameraCapabilities cameraCapabilities7;
    QVERIFY2(!cameraCapabilities7.isValid(), "Camera capabilities should default to invalid");
    cameraCapabilities7.setMaximumTilt(0.8);
    QVERIFY2(cameraCapabilities7.isValid(), "Camera capabilities should be valid");
}

QTEST_APPLESS_MAIN(tst_QGeoCameraCapabilities)

#include "tst_qgeocameracapabilities.moc"
