/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "tst_qgeomaneuver.h"


tst_QGeoManeuver::tst_QGeoManeuver()
{
}

void tst_QGeoManeuver::initTestCase()
{

}

void tst_QGeoManeuver::cleanupTestCase()
{

}

void tst_QGeoManeuver::init()
{

    qgeomaneuver = new QGeoManeuver();
}

void tst_QGeoManeuver::cleanup()
{
    delete qgeomaneuver;
}

void tst_QGeoManeuver::constructor()
{
    QString empty ="";

    QVERIFY(!qgeomaneuver->isValid());
    QCOMPARE(qgeomaneuver->direction(),QGeoManeuver::NoDirection);
    QCOMPARE(qgeomaneuver->distanceToNextInstruction(), qreal(0.0));
    QCOMPARE(qgeomaneuver->instructionText(),empty);
    QCOMPARE(qgeomaneuver->timeToNextInstruction(),0);
}

void tst_QGeoManeuver::copy_constructor()
{
    QGeoManeuver *qgeomaneuvercopy = new QGeoManeuver (*qgeomaneuver);

    QCOMPARE(*qgeomaneuver,*qgeomaneuvercopy);

    delete qgeomaneuvercopy;
}

void tst_QGeoManeuver::destructor()
{
    QGeoManeuver *qgeomaneuvercopy;

    qgeomaneuvercopy = new QGeoManeuver();
    delete qgeomaneuvercopy;

    qgeomaneuvercopy = new QGeoManeuver(*qgeomaneuver);
    delete qgeomaneuvercopy;
}

void tst_QGeoManeuver::direction()
{
    QFETCH(QGeoManeuver::InstructionDirection,direction);

    qgeomaneuver->setDirection(direction);

    QCOMPARE(qgeomaneuver->direction(),direction);
}
void tst_QGeoManeuver::direction_data()
{
    QTest::addColumn<QGeoManeuver::InstructionDirection>("direction");

    QTest::newRow("instruction1") << QGeoManeuver::NoDirection;
    QTest::newRow("instruction2") << QGeoManeuver::DirectionForward;
    QTest::newRow("instruction3") << QGeoManeuver::DirectionBearRight;
    QTest::newRow("instruction4") << QGeoManeuver::DirectionLightRight;
    QTest::newRow("instruction5") << QGeoManeuver::DirectionRight;
    QTest::newRow("instruction6") << QGeoManeuver::DirectionHardRight;
    QTest::newRow("instruction7") << QGeoManeuver::DirectionUTurnRight;
    QTest::newRow("instruction8") << QGeoManeuver::DirectionUTurnLeft;
    QTest::newRow("instruction9") << QGeoManeuver::DirectionHardLeft;
    QTest::newRow("instruction10") << QGeoManeuver::DirectionLeft;
    QTest::newRow("instruction11") << QGeoManeuver::DirectionLightLeft;
    QTest::newRow("instruction12") << QGeoManeuver::DirectionBearLeft;
}

void tst_QGeoManeuver::distanceToNextInstruction()
{
    qreal distance = 0.0;
    qgeomaneuver->setDistanceToNextInstruction(distance);

    QCOMPARE (qgeomaneuver->distanceToNextInstruction(), distance);

    distance = -3423.4324;

    QVERIFY (qgeomaneuver->distanceToNextInstruction() != distance);

    qgeomaneuver->setDistanceToNextInstruction(distance);
    QCOMPARE (qgeomaneuver->distanceToNextInstruction(),distance);
}

void tst_QGeoManeuver::instructionText()
{
    QString text = "After 50m turn left";

    qgeomaneuver->setInstructionText(text);

    QCOMPARE (qgeomaneuver->instructionText(),text);

    text="After 40m, turn left";
    QVERIFY (qgeomaneuver->instructionText() != text);

}

void tst_QGeoManeuver::position()
{
    QFETCH(double, latitude);
    QFETCH(double, longitude);

    qgeocoordinate = new QGeoCoordinate (latitude,longitude);

    qgeomaneuver->setPosition(*qgeocoordinate);

    QCOMPARE(qgeomaneuver->position(),*qgeocoordinate);

    delete qgeocoordinate;
}

void tst_QGeoManeuver::position_data()
{
    QTest::addColumn<double>("latitude");
    QTest::addColumn<double>("longitude");

    QTest::newRow("invalid0") << -12220.0 << 0.0;
    QTest::newRow("invalid1") << 0.0 << 181.0;

    QTest::newRow("correct0") << 0.0 << 0.0;
    QTest::newRow("correct1") << 90.0 << 0.0;
    QTest::newRow("correct2") << 0.0 << 180.0;
    QTest::newRow("correct3") << -90.0 << 0.0;
    QTest::newRow("correct4") << 0.0 << -180.0;
    QTest::newRow("correct5") << 45.0 << 90.0;
}

void tst_QGeoManeuver::timeToNextInstruction()
{
    int time = 0;
    qgeomaneuver->setTimeToNextInstruction(time);

    QCOMPARE (qgeomaneuver->timeToNextInstruction(),time);

    time = 35;

    QVERIFY (qgeomaneuver->timeToNextInstruction() != time);

    qgeomaneuver->setTimeToNextInstruction(time);
    QCOMPARE (qgeomaneuver->timeToNextInstruction(),time);
}

void tst_QGeoManeuver::waypoint()
{
    QFETCH(double, latitude);
    QFETCH(double, longitude);

    qgeocoordinate = new QGeoCoordinate (latitude,longitude);

    qgeomaneuver->setWaypoint(*qgeocoordinate);

    QCOMPARE(qgeomaneuver->waypoint(),*qgeocoordinate);

    qgeocoordinate->setLatitude(30.3);
    QVERIFY(qgeomaneuver->waypoint() != *qgeocoordinate);


    delete qgeocoordinate;
}
void tst_QGeoManeuver::waypoint_data()
{
    QTest::addColumn<double>("latitude");
    QTest::addColumn<double>("longitude");

    QTest::newRow("invalid0") << -12220.0 << 0.0;
    QTest::newRow("invalid1") << 0.0 << 181.0;

    QTest::newRow("correct0") << 0.0 << 0.0;
    QTest::newRow("correct1") << 90.0 << 0.0;
    QTest::newRow("correct2") << 0.0 << 180.0;
    QTest::newRow("correct3") << -90.0 << 0.0;
    QTest::newRow("correct4") << 0.0 << -180.0;
    QTest::newRow("correct5") << 45.0 << 90.0;
}

void tst_QGeoManeuver::isValid()
{
    QVERIFY(!qgeomaneuver->isValid());
    qgeomaneuver->setDirection(QGeoManeuver::DirectionBearLeft);
    QVERIFY(qgeomaneuver->isValid());
}

void tst_QGeoManeuver::operators(){

    QGeoManeuver *qgeomaneuvercopy = new QGeoManeuver(*qgeomaneuver);

    QVERIFY(qgeomaneuver->operator ==(*qgeomaneuvercopy));
    QVERIFY(!qgeomaneuver->operator !=(*qgeomaneuvercopy));

    qgeomaneuver->setDirection(QGeoManeuver::DirectionBearLeft);
    qgeomaneuver->setInstructionText("Turn left in 50m");
    qgeomaneuver->setTimeToNextInstruction(60);
    qgeomaneuver->setDistanceToNextInstruction(560.45);

    qgeomaneuvercopy->setDirection(QGeoManeuver::DirectionForward);
    qgeomaneuvercopy->setInstructionText("Turn left in 80m");
    qgeomaneuvercopy->setTimeToNextInstruction(70);
    qgeomaneuvercopy->setDistanceToNextInstruction(56065.45);

// Not passing
   QVERIFY(!(qgeomaneuver->operator ==(*qgeomaneuvercopy)));
// Not passing
   QVERIFY(qgeomaneuver->operator !=(*qgeomaneuvercopy));

    *qgeomaneuvercopy = qgeomaneuvercopy->operator =(*qgeomaneuver);
    QVERIFY(qgeomaneuver->operator ==(*qgeomaneuvercopy));
    QVERIFY(!qgeomaneuver->operator !=(*qgeomaneuvercopy));

    delete qgeomaneuvercopy;
}


QTEST_APPLESS_MAIN(tst_QGeoManeuver);
